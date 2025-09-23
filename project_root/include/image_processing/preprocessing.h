#ifndef PREPROCESSING_H
#define PREPROCESSING_H

#include "../common.h"

// Rotation d'image
void rotate_image(Image* img, double angle);
void rotate_image_90(Image* img);
double detect_skew_angle(const Image* img);
void auto_rotate(Image* img);

// Amélioration d'image
void remove_noise(Image* img);
void enhance_contrast(Image* img);
void apply_gaussian_blur(Image* img, int kernel_size);
void sharpen_image(Image* img);

// Normalisation
void normalize_brightness(Image* img);
void adjust_gamma(Image* img, double gamma);

#endif // PREPROCESSING_H