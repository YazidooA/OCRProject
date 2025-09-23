#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include "../common.h"

// Utilitaires généraux pour images
unsigned char get_pixel(const Image* img, int x, int y, int channel);
void set_pixel(Image* img, int x, int y, int channel, unsigned char value);

// Opérations sur pixels
unsigned char get_grayscale_pixel(const Image* img, int x, int y);
void set_grayscale_pixel(Image* img, int x, int y, unsigned char value);

// Statistiques d'image
double calculate_mean(const Image* img);
double calculate_variance(const Image* img);
int* calculate_histogram(const Image* img, int* hist_size);

// Opérations morphologiques
void dilate_image(Image* img, int kernel_size);
void erode_image(Image* img, int kernel_size);
void opening(Image* img, int kernel_size);
void closing(Image* img, int kernel_size);

#endif // IMAGE_UTILS_H
