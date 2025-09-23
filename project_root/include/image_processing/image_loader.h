#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include "../common.h"

// Chargement et sauvegarde d'images
Image* load_image(const char* filename);
Image* create_image(int width, int height, int channels);
void free_image(Image* img);
ErrorCode save_image(Image* img, const char* filename);

// Conversions
void convert_to_grayscale(Image* img);
void convert_to_binary(Image* img, int threshold);
Image* copy_image(const Image* src);

// Utilitaires
bool is_valid_image(const Image* img);
void print_image_info(const Image* img);

#endif // IMAGE_LOADER_H