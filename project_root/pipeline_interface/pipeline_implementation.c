// pipeline_implementation.c
// Simple pipeline interface for UI - just image processing steps

#include "../image_cleaner/image_cleaner.h"
#include "pipeline_interface.h"

#include <SDL2/SDL.h>
#include <stdio.h>


SDL_Surface *simple_pipeline(SDL_Surface *surface, SDL_Renderer *renderer) {
  (void)renderer; // Unused in this simple version

  if (!surface) {
    fprintf(stderr, "simple_pipeline: surface is NULL\n");
    return NULL;
  }

  printf("Running simple OCR preprocessing pipeline...\n");

  // Step 1: Grayscale conversion
  printf("  1/3 Converting to grayscale...\n");
  convert_to_grayscale(surface);

  // Step 2: Otsu thresholding (binarization)
  printf("  2/3 Applying Otsu thresholding...\n");
  apply_otsu_thresholding(surface);

  // Step 3: Noise removal
  printf("  3/3 Removing noise...\n");
  apply_noise_removal(surface, 3); // threshold of 3 neighbors

  printf("Simple pipeline complete.\n");

  return surface;
}
