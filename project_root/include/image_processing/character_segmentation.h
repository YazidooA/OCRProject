#ifndef CHARACTER_SEGMENTATION_H
#define CHARACTER_SEGMENTATION_H

#include "../common.h"

typedef struct {
    Image* char_image;
    Position position;
    Rectangle bounds;
    char recognized_char;
    double confidence;
} Character;

// Segmentation de caractères
Character* extract_grid_characters(const Image* img, const Rectangle* cells, int cell_count, int* char_count);
Character* extract_word_list_characters(const Image* img, const Rectangle* word_area, int* char_count);

// Traitement de caractères individuels
Image* extract_character_at_position(const Image* img, const Rectangle* bounds);
void normalize_character_image(Image* char_img);
void resize_character_image(Image* char_img, int target_width, int target_height);

// Gestion mémoire
void free_characters(Character* chars, int count);
Character* create_character(int x, int y, int width, int height);

#endif // CHARACTER_SEGMENTATION_H