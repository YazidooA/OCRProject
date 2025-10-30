#ifndef PIPELINE_INTERFACE_H
#define PIPELINE_INTERFACE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "structure_detection.h"
#include "letter_extractor.h"
#include "solver.h"
#include "draw_outline.h"

/* ========================================
   PIPELINE COMPLET RDN
   ========================================
   
   Flux :
   1. Ibrahim donne SDL_Surface
   2. Extraction lettres -> CSV (format IA)
   3. IA externe traite CSV -> retourne liste lettres
   4. Écriture fichiers (grille pour solver)
   5. Solver cherche mots -> retourne indices grille
   6. Conversion indices -> positions pixel
   7. draw_outline() entoure les mots
   8. Retour SDL_Surface à Ibrahim
*/

// Structure pour une lettre extraite (pour le CSV)
typedef struct {
    int id;                    // ID unique de la lettre
    unsigned char pixels[784]; // 28x28 pixels en grayscale [0-255]
    int grid_row;              // Position ligne dans la grille
    int grid_col;              // Position colonne dans la grille
    int pixel_x;               // Position X pixel dans l'image originale
    int pixel_y;               // Position Y pixel dans l'image originale
} ExtractedLetter;

// Structure pour le résultat d'extraction
typedef struct {
    ExtractedLetter* letters;  // Tableau de lettres
    int count;                 // Nombre de lettres
    int rows;                  // Nombre de lignes grille
    int cols;                  // Nombre de colonnes grille
    Rectangle grid_area;       // Zone de la grille en pixels
} ExtractionResult;

// Structure pour indices de résolution (retour solver)
typedef struct {
    char word[50];             // Mot cherché
    int found;                 // 1 si trouvé, 0 sinon
    int grid_start_col;        // Colonne début (grille)
    int grid_start_row;        // Ligne début (grille)
    int grid_end_col;          // Colonne fin (grille)
    int grid_end_row;          // Ligne fin (grille)
} SolverResult;

// Structure pour positions pixel (pour draw_outline)
typedef struct {
    char word[50];             // Mot trouvé
    int pixel_x1, pixel_y1;    // Position pixel début
    int pixel_x2, pixel_y2;    // Position pixel fin
} PixelPosition;

/* ========================================
   ÉTAPE 1 : Ibrahim -> Extraction -> CSV
   ======================================== */

/**
 * Extrait les lettres d'une SDL_Surface et génère un CSV pour l'IA
 * 
 * @param surface Surface SDL contenant la grille
 * @param csv_output_path Chemin du fichier CSV à créer
 * @return ExtractionResult* Structure contenant les lettres extraites (à libérer avec free_extraction_result)
 */
ExtractionResult* surface_to_csv_for_ia(SDL_Surface* surface, const char* csv_output_path);

/**
 * Libère la mémoire d'un ExtractionResult
 */
void free_extraction_result(ExtractionResult* result);

/* ========================================
   ÉTAPE 2 : IA traite le CSV (EXTERNE)
   ======================================== */
// L'IA externe lit le CSV et retourne une liste de caractères
// Format attendu : "ABCDEFG..." (string de caractères reconnus)

/* ========================================
   ÉTAPE 3 : Liste lettres -> Fichiers
   ======================================== */

/**
 * Crée le fichier grille pour le solver à partir des lettres reconnues
 * 
 * @param recognized_letters String de lettres reconnues (ex: "ABCDEFG...")
 * @param rows Nombre de lignes
 * @param cols Nombre de colonnes
 * @param grid_output_path Chemin du fichier grille (format solver.c)
 * @return 0 si succès, -1 si erreur
 */
int write_grid_file_for_solver(const char* recognized_letters, int rows, int cols, 
                                const char* grid_output_path);

/* ========================================
   ÉTAPE 4 : Solver cherche mots
   ======================================== */

/**
 * Lance le solver pour chercher plusieurs mots
 * 
 * @param grid_file_path Chemin du fichier grille
 * @param words_to_find Tableau de mots à chercher
 * @param word_count Nombre de mots
 * @param results Tableau de SolverResult à remplir (doit être alloué)
 * @return Nombre de mots trouvés
 */
int run_solver_on_words(const char* grid_file_path, const char** words_to_find, 
                        int word_count, SolverResult* results);

/* ========================================
   ÉTAPE 5 : Indices grille -> Positions pixel
   ======================================== */

/**
 * Convertit les indices grille en positions pixel
 * 
 * @param solver_results Résultats du solver (indices grille)
 * @param result_count Nombre de résultats
 * @param grid_area Zone de la grille en pixels
 * @param rows Nombre de lignes
 * @param cols Nombre de colonnes
 * @param pixel_positions Tableau de PixelPosition à remplir (doit être alloué)
 */
void convert_grid_to_pixel_positions(const SolverResult* solver_results, int result_count,
                                     Rectangle grid_area, int rows, int cols,
                                     PixelPosition* pixel_positions);

/* ========================================
   ÉTAPE 6 : draw_outline entoure les mots
   ======================================== */

/**
 * Dessine les contours des mots trouvés sur la surface
 * 
 * @param surface Surface SDL à modifier
 * @param pixel_positions Positions pixel des mots
 * @param position_count Nombre de positions
 * @param color Couleur SDL (ex: 0xFF0000 pour rouge)
 */
void draw_word_outlines_on_surface(SDL_Surface* surface, const PixelPosition* pixel_positions,
                                   int position_count, Uint32 color);

/* ========================================
   FONCTION PIPELINE COMPLÈTE
   ======================================== */

/**
 * Pipeline complet : Surface -> CSV -> (IA externe) -> Lettres -> Solver -> Surface
 * 
 * @param input_surface Surface d'entrée (Ibrahim)
 * @param words_to_find Mots à chercher
 * @param word_count Nombre de mots
 * @param csv_path Chemin du CSV temporaire (pour IA)
 * @param grid_path Chemin du fichier grille temporaire (pour solver)
 * @param recognized_letters String de lettres reconnues par l'IA (doit être fournie)
 * @return SDL_Surface* Surface annotée (à libérer avec SDL_FreeSurface)
 * 
 * UTILISATION :
 * 1. Appeler cette fonction avec recognized_letters = NULL pour générer le CSV
 * 2. L'IA externe traite le CSV et retourne les lettres
 * 3. Rappeler cette fonction avec les recognized_letters pour finaliser
 */
SDL_Surface* pipeline_complete(SDL_Surface* input_surface, const char** words_to_find,
                               int word_count, const char* csv_path, const char* grid_path,
                               const char* recognized_letters);

/* ========================================
   VERSION SIMPLIFIÉE (2 APPELS)
   ======================================== */

/**
 * PHASE 1 : Génération du CSV pour l'IA
 * Retourne un ExtractionResult à conserver pour la phase 2
 */
ExtractionResult* pipeline_phase1_generate_csv(SDL_Surface* surface, const char* csv_path);

/**
 * PHASE 2 : Résolution et annotation
 * Prend les lettres reconnues et retourne la surface annotée
 */
SDL_Surface* pipeline_phase2_solve_and_draw(SDL_Surface* surface, ExtractionResult* extraction,
                                            const char* recognized_letters,
                                            const char** words_to_find, int word_count,
                                            const char* grid_path);

#endif // PIPELINE_INTERFACE_H