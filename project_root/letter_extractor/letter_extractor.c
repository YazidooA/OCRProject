#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "letter_extractor.h"

// Extrait une sous-image d'une zone rectangulaire
Image* extract_subimage(Image* img, Rectangle area) {
    if (!img || !img->data) return NULL;
    
    // Validation des limites
    if (area.x < 0) area.x = 0;
    if (area.y < 0) area.y = 0;
    if (area.x + area.width > img->width) area.width = img->width - area.x;
    if (area.y + area.height > img->height) area.height = img->height - area.y;
    
    if (area.width <= 0 || area.height <= 0) return NULL;
    
    Image* sub = (Image*)malloc(sizeof(Image));
    sub->width = area.width;
    sub->height = area.height;
    sub->channels = img->channels;
    sub->data = (unsigned char*)malloc(area.width * area.height * img->channels);
    
    for (int y = 0; y < area.height; y++) {
        for (int x = 0; x < area.width; x++) {
            int src_idx = ((area.y + y) * img->width + (area.x + x)) * img->channels;
            int dst_idx = (y * area.width + x) * img->channels;
            for (int c = 0; c < img->channels; c++) {
                sub->data[dst_idx + c] = img->data[src_idx + c];
            }
        }
    }
    
    return sub;
}

// Binarise l'image (noir et blanc pur)
void binarize_image(Image* img, int threshold) {
    if (!img || !img->data) return;
    
    for (int i = 0; i < img->width * img->height; i++) {
        img->data[i] = (img->data[i] < threshold) ? 0 : 255;
    }
}

// Trouve la boîte englobante du contenu (pixels noirs)
void get_bounding_box(Image* img, int* min_x, int* min_y, int* max_x, int* max_y) {
    *min_x = img->width;
    *min_y = img->height;
    *max_x = 0;
    *max_y = 0;
    
    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            if (img->data[y * img->width + x] < 128) {  // Pixel noir
                if (x < *min_x) *min_x = x;
                if (x > *max_x) *max_x = x;
                if (y < *min_y) *min_y = y;
                if (y > *max_y) *max_y = y;
            }
        }
    }
    
    // Si aucun pixel noir trouvé
    if (*min_x > *max_x) {
        *min_x = 0;
        *min_y = 0;
        *max_x = img->width - 1;
        *max_y = img->height - 1;
    }
}

// Recadre l'image sur le contenu
Image* crop_to_content(Image* img) {
    int min_x, min_y, max_x, max_y;
    get_bounding_box(img, &min_x, &min_y, &max_x, &max_y);
    
    Rectangle crop_area = {min_x, min_y, max_x - min_x + 1, max_y - min_y + 1};
    return extract_subimage(img, crop_area);
}

// Redimensionne l'image avec interpolation simple
Image* resize_image(Image* img, int new_width, int new_height) {
    if (!img || !img->data) return NULL;
    
    Image* resized = (Image*)malloc(sizeof(Image));
    resized->width = new_width;
    resized->height = new_height;
    resized->channels = img->channels;
    resized->data = (unsigned char*)malloc(new_width * new_height * img->channels);
    
    float x_ratio = (float)img->width / new_width;
    float y_ratio = (float)img->height / new_height;
    
    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            int src_x = (int)(x * x_ratio);
            int src_y = (int)(y * y_ratio);
            
            // Clamp pour éviter les dépassements
            if (src_x >= img->width) src_x = img->width - 1;
            if (src_y >= img->height) src_y = img->height - 1;
            
            int src_idx = (src_y * img->width + src_x) * img->channels;
            int dst_idx = (y * new_width + x) * img->channels;
            
            for (int c = 0; c < img->channels; c++) {
                resized->data[dst_idx + c] = img->data[src_idx + c];
            }
        }
    }
    
    return resized;
}

// Centre la lettre dans une image carrée avec padding
Image* center_letter(Image* letter_img) {
    if (!letter_img || !letter_img->data) return NULL;
    
    // Créer une image carrée de la taille maximale
    int max_dim = (letter_img->width > letter_img->height) ? 
                   letter_img->width : letter_img->height;
    
    // Ajouter un padding de 10%
    max_dim = (int)(max_dim * 1.1);
    
    Image* centered = (Image*)malloc(sizeof(Image));
    centered->width = max_dim;
    centered->height = max_dim;
    centered->channels = letter_img->channels;
    centered->data = (unsigned char*)malloc(max_dim * max_dim * letter_img->channels);
    
    // Remplir avec du blanc
    memset(centered->data, 255, max_dim * max_dim * letter_img->channels);
    
    // Calculer l'offset pour centrer
    int offset_x = (max_dim - letter_img->width) / 2;
    int offset_y = (max_dim - letter_img->height) / 2;
    
    // Copier la lettre au centre
    for (int y = 0; y < letter_img->height; y++) {
        for (int x = 0; x < letter_img->width; x++) {
            int src_idx = (y * letter_img->width + x) * letter_img->channels;
            int dst_idx = ((offset_y + y) * max_dim + (offset_x + x)) * letter_img->channels;
            
            for (int c = 0; c < letter_img->channels; c++) {
                centered->data[dst_idx + c] = letter_img->data[src_idx + c];
            }
        }
    }
    
    return centered;
}

// Normalise une lettre pour le réseau de neurones (28x28 centré, binarisé)
Image* normalize_letter(Image* letter_img, int target_width, int target_height) {
    if (!letter_img || !letter_img->data) return NULL;
    
    // 1. Binariser
    binarize_image(letter_img, 128);
    
    // 2. Recadrer sur le contenu
    Image* cropped = crop_to_content(letter_img);
    if (!cropped) return NULL;
    
    // 3. Centrer dans une image carrée
    Image* centered = center_letter(cropped);
    free(cropped->data);
    free(cropped);
    if (!centered) return NULL;
    
    // 4. Redimensionner à la taille cible
    Image* normalized = resize_image(centered, target_width, target_height);
    free(centered->data);
    free(centered);
    
    return normalized;
}

// Extrait une seule lettre d'une cellule
Letter* extract_single_letter(Image* img, Rectangle cell_area, int row, int col) {
    if (!img || !img->data) return NULL;
    
    Letter* letter = (Letter*)malloc(sizeof(Letter));
    if (!letter) return NULL;
    
    // Extraire la cellule
    Image* cell_img = extract_subimage(img, cell_area);
    if (!cell_img) {
        free(letter);
        return NULL;
    }
    
    // Normaliser pour le réseau (28x28 est standard pour MNIST/OCR)
    Image* normalized = normalize_letter(cell_img, 28, 28);
    free(cell_img->data);
    free(cell_img);
    
    if (!normalized) {
        free(letter);
        return NULL;
    }
    
    // Remplir la structure Letter
    letter->data = normalized->data;
    letter->width = normalized->width;
    letter->height = normalized->height;
    letter->original_x = cell_area.x;
    letter->original_y = cell_area.y;
    letter->grid_row = row;
    letter->grid_col = col;
    
    free(normalized); // On garde juste les data
    
    return letter;
}

// Extrait toutes les lettres de la grille
LetterGrid* extract_letters_from_grid(Image* img, Rectangle grid_area, int rows, int cols) {
    if (!img || !img->data || rows <= 0 || cols <= 0) return NULL;
    
    printf("Extraction de %dx%d = %d lettres...\n", rows, cols, rows * cols);
    
    LetterGrid* grid = (LetterGrid*)malloc(sizeof(LetterGrid));
    grid->rows = rows;
    grid->cols = cols;
    grid->count = rows * cols;
    grid->letters = (Letter*)malloc(grid->count * sizeof(Letter));
    
    // Calculer la taille de chaque cellule
    int cell_width = grid_area.width / cols;
    int cell_height = grid_area.height / rows;
    
    int letter_idx = 0;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            Rectangle cell = {
                grid_area.x + c * cell_width,
                grid_area.y + r * cell_height,
                cell_width,
                cell_height
            };
            
            Letter* letter = extract_single_letter(img, cell, r, c);
            if (letter) {
                grid->letters[letter_idx] = *letter;
                free(letter);
                letter_idx++;
            } else {
                fprintf(stderr, "Erreur extraction lettre [%d,%d]\n", r, c);
            }
        }
    }
    
    printf("✓ %d lettres extraites et normalisées (28x28)\n", letter_idx);
    return grid;
}

// Sauvegarde une lettre individuelle (pour debugging)
void save_letter(Letter* letter, const char* filename) {
    if (!letter || !letter->data) return;
    
    FILE* fp = fopen(filename, "wb");
    if (fp) {
        fprintf(fp, "P5\n%d %d\n255\n", letter->width, letter->height);
        fwrite(letter->data, 1, letter->width * letter->height, fp);
        fclose(fp);
    }
}

// Sauvegarde toutes les lettres dans un dossier (pour debugging)
void save_letter_grid(LetterGrid* grid, const char* output_dir) {
    if (!grid || !grid->letters) return;
    
    // Créer le dossier s'il n'existe pas
    mkdir(output_dir, 0755);
    
    printf("Sauvegarde de %d lettres dans %s/...\n", grid->count, output_dir);
    
    for (int i = 0; i < grid->count; i++) {
        char filename[256];
        snprintf(filename, 256, "%s/letter_%02d_%02d.pgm", 
                output_dir, grid->letters[i].grid_row, grid->letters[i].grid_col);
        save_letter(&grid->letters[i], filename);
    }
    
    printf("✓ Lettres sauvegardées\n");
}

// Libération mémoire
void free_letter(Letter* letter) {
    if (letter && letter->data) {
        free(letter->data);
        letter->data = NULL;
    }
}

void free_letter_grid(LetterGrid* grid) {
    if (grid) {
        if (grid->letters) {
            for (int i = 0; i < grid->count; i++) {
                if (grid->letters[i].data) {
                    free(grid->letters[i].data);
                }
            }
            free(grid->letters);
        }
        free(grid);
    }
}