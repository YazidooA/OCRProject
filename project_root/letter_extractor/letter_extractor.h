#ifndef LETTER_EXTRACTION_H
#define LETTER_EXTRACTION_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "structure_detection.h" // doit définir Image et Rectangle

// Structure pour une lettre extraite
typedef struct {
    unsigned char* data;  // Image de la lettre normalisée (ex: 28x28, 1 channel)
    int width;            // Largeur normalisée (ex: 28)
    int height;           // Hauteur normalisée (ex: 28)
    int original_x;       // Position X dans l'image originale (cell origin)
    int original_y;       // Position Y dans l'image originale (cell origin)
    int grid_row;         // Ligne dans la grille
    int grid_col;         // Colonne dans la grille
} Letter;

// Structure pour stocker toutes les lettres de la grille
typedef struct {
    Letter* letters;      // Tableau de lettres (count éléments)
    int count;            // Nombre de lettres effectivement extraites
    int rows;             // Nombre de lignes attendues dans la grille
    int cols;             // Nombre de colonnes attendues dans la grille
} LetterGrid;

// Fonctions d'extraction
LetterGrid* extract_letters_from_grid(Image* img, Rectangle grid_area, int rows, int cols);
Letter* extract_single_letter(Image* img, Rectangle cell_area, int row, int col);

// Fonctions de prétraitement pour le réseau de neurones
Image* normalize_letter(Image* letter_img, int target_width, int target_height);
Image* center_letter(Image* letter_img);
Image* crop_to_content(Image* img);
void binarize_image(Image* img, int threshold);

// Utilities
void get_bounding_box(Image* img, int* min_x, int* min_y, int* max_x, int* max_y, int threshold);
Image* resize_image(Image* img, int new_width, int new_height);
Image* extract_subimage(Image* img, Rectangle area);

// Conversion helper (multi->grayscale)
int ensure_grayscale_inplace(Image* img);

// Libération mémoire
void free_letter(Letter* letter);
void free_letter_grid(LetterGrid* grid);

#endif // LETTER_EXTRACTION_H
