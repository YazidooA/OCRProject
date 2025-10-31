#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "letter_extractor.h"

/*
 * Helper: ensure image is single-channel grayscale in-place.
 * If img->channels == 1 => nothing
 * If img->channels > 1 => allocate new buffer (w*h), compute luminance and replace data,
 *                        update img->channels to 1.
 * Returns 0 on success, -1 on error.
 */
int ensure_grayscale_inplace(Image* img) {
    if (!img || !img->data) return -1;
    if (img->channels == 1) return 0;

    size_t pixels = (size_t)img->width * img->height;
    unsigned char *gray = (unsigned char*)malloc(pixels);
    if (!gray) return -1;

    // Convert RGB(A) -> luminance (Y = 0.299 R + 0.587 G + 0.114 B)
    for (size_t i = 0; i < pixels; i++) {
        int base = (int)(i * img->channels);
        unsigned char r = img->data[base + 0];
        unsigned char g = (img->channels > 1) ? img->data[base + 1] : r;
        unsigned char b = (img->channels > 2) ? img->data[base + 2] : r;
        int y = (int)(0.299f * r + 0.587f * g + 0.114f * b + 0.5f);
        if (y < 0) y = 0;
        if (y > 255) y = 255;
        gray[i] = (unsigned char)y;
    }

    free(img->data);
    img->data = gray;
    img->channels = 1;
    return 0;
}

// Extract subimage from rectangular area
Image* extract_subimage(Image* img, Rectangle area) {
    if (!img || !img->data) return NULL;

    if (area.x < 0) area.x = 0;
    if (area.y < 0) area.y = 0;
    if (area.x + area.width > img->width) area.width = img->width - area.x;
    if (area.y + area.height > img->height) area.height = img->height - area.y;

    if (area.width <= 0 || area.height <= 0) return NULL;

    Image* sub = (Image*)malloc(sizeof(Image));
    if (!sub) return NULL;
    sub->width = area.width;
    sub->height = area.height;
    sub->channels = img->channels;
    size_t bufsize = (size_t)area.width * area.height * sub->channels;
    sub->data = (unsigned char*)malloc(bufsize);
    if (!sub->data) { free(sub); return NULL; }

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

// Binarize image (pure black and white) — expects single channel
void binarize_image(Image* img, int threshold) {
    if (!img || !img->data) return;
    if (img->channels != 1) {
        // attempt conversion
        if (ensure_grayscale_inplace(img) != 0) return;
    }

    size_t pixels = (size_t)img->width * img->height;
    for (size_t i = 0; i < pixels; i++) {
        img->data[i] = (img->data[i] < threshold) ? 0u : 255u;
    }
}

// Find bounding box of content (black pixels). threshold on grayscale.
void get_bounding_box(Image* img, int* min_x, int* min_y, int* max_x, int* max_y, int threshold) {
    if (!img || !img->data) { *min_x = *min_y = *max_x = *max_y = 0; return; }
    if (img->channels != 1) {
        if (ensure_grayscale_inplace(img) != 0) { *min_x = *min_y = *max_x = *max_y = 0; return; }
    }

    *min_x = img->width;
    *min_y = img->height;
    *max_x = -1;
    *max_y = -1;

    for (int y = 0; y < img->height; y++) {
        for (int x = 0; x < img->width; x++) {
            unsigned char v = img->data[y * img->width + x];
            if (v < threshold) {
                if (x < *min_x) *min_x = x;
                if (x > *max_x) *max_x = x;
                if (y < *min_y) *min_y = y;
                if (y > *max_y) *max_y = y;
            }
        }
    }

    // No content found -> return full image as fallback (caller can decide)
    if (*max_x < 0 || *max_y < 0) {
        *min_x = 0;
        *min_y = 0;
        *max_x = img->width - 1;
        *max_y = img->height - 1;
    }
}

// Crop image to content
Image* crop_to_content(Image* img) {
    int min_x, min_y, max_x, max_y;
    // use threshold 128 by default
    get_bounding_box(img, &min_x, &min_y, &max_x, &max_y, 128);

    Rectangle crop_area = {min_x, min_y, max_x - min_x + 1, max_y - min_y + 1};
    return extract_subimage(img, crop_area);
}

// Resize image with nearest-neighbor interpolation (simple, deterministic)
Image* resize_image(Image* img, int new_width, int new_height) {
    if (!img || !img->data) return NULL;

    Image* resized = (Image*)malloc(sizeof(Image));
    if (!resized) return NULL;
    resized->width = new_width;
    resized->height = new_height;
    resized->channels = img->channels;
    size_t bufsize = (size_t)new_width * new_height * img->channels;
    resized->data = (unsigned char*)malloc(bufsize);
    if (!resized->data) { free(resized); return NULL; }

    float x_ratio = (float)img->width / (float)new_width;
    float y_ratio = (float)img->height / (float)new_height;

    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            int src_x = (int)(x * x_ratio);
            int src_y = (int)(y * y_ratio);

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

// Center letter in square image with padding (works on single-channel or multi-channel)
Image* center_letter(Image* letter_img) {
    if (!letter_img || !letter_img->data) return NULL;

    int max_dim = (letter_img->width > letter_img->height) ?
                   letter_img->width : letter_img->height;
    // keep a small margin (10%)
    max_dim = (int)(max_dim * 1.1f);
    if (max_dim < 1) max_dim = 1;

    Image* centered = (Image*)malloc(sizeof(Image));
    if (!centered) return NULL;
    centered->width = max_dim;
    centered->height = max_dim;
    centered->channels = letter_img->channels;
    size_t bufsize = (size_t)max_dim * max_dim * centered->channels;
    centered->data = (unsigned char*)malloc(bufsize);
    if (!centered->data) { free(centered); return NULL; }

    // Fill with white
    memset(centered->data, 255, bufsize);

    int offset_x = (max_dim - letter_img->width) / 2;
    int offset_y = (max_dim - letter_img->height) / 2;

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

// Normalize letter for neural network (target_width x target_height, single-channel)
Image* normalize_letter(Image* letter_img, int target_width, int target_height) {
    if (!letter_img || !letter_img->data) return NULL;

    // Ensure grayscale first
    if (ensure_grayscale_inplace(letter_img) != 0) return NULL;

    // Binarize in-place (helps bounding box)
    binarize_image(letter_img, 128);

    Image* cropped = crop_to_content(letter_img);
    if (!cropped) return NULL;

    // Ensure cropped is grayscale single-channel (should be)
    if (cropped->channels != 1) {
        // convert if necessary (shouldn't happen but safe)
        ensure_grayscale_inplace(cropped);
    }

    Image* centered = center_letter(cropped);
    // free cropped structure and buffer
    if (cropped->data) free(cropped->data);
    free(cropped);
    if (!centered) return NULL;

    // For NN we want a single-channel image
    if (centered->channels != 1) {
        // Convert to grayscale if needed
        ensure_grayscale_inplace(centered);
    }

    Image* normalized = resize_image(centered, target_width, target_height);

    if (centered->data) free(centered->data);
    free(centered);

    // At this point normalized should be single-channel 28x28
    if (normalized && normalized->channels != 1) {
        // convert (rare)
        ensure_grayscale_inplace(normalized);
    }

    return normalized;
}

// Extract single letter from cell
Letter* extract_single_letter(Image* img, Rectangle cell_area, int row, int col) {
    if (!img || !img->data) return NULL;

    Letter* letter = (Letter*)malloc(sizeof(Letter));
    if (!letter) return NULL;
    memset(letter, 0, sizeof(Letter));

    Image* cell_img = extract_subimage(img, cell_area);
    if (!cell_img) {
        free(letter);
        return NULL;
    }

    Image* normalized = normalize_letter(cell_img, 28, 28);
    // free cell image
    if (cell_img->data) free(cell_img->data);
    free(cell_img);

    if (!normalized) {
        free(letter);
        return NULL;
    }

    // Move ownership of buffer to letter (so caller must free letter->data)
    letter->data = normalized->data;
    letter->width = normalized->width;
    letter->height = normalized->height;
    letter->original_x = cell_area.x;
    letter->original_y = cell_area.y;
    letter->grid_row = row;
    letter->grid_col = col;

    // free normalized struct but not its data
    free(normalized);

    return letter;
}

// Extract all letters from grid
LetterGrid* extract_letters_from_grid(Image* img, Rectangle grid_area, int rows, int cols)
{
    if (!img || !img->data || rows <= 0 || cols <= 0) return NULL;

    printf("Extraction de %dx%d = %d lettres...\n", rows, cols, rows * cols);

    LetterGrid* grid = (LetterGrid*)malloc(sizeof(LetterGrid));
    if (!grid) return NULL;
    memset(grid, 0, sizeof(LetterGrid));
    grid->rows = rows;
    grid->cols = cols;
    // allocate array and zero it
    grid->letters = (Letter*)calloc((size_t)rows * cols, sizeof(Letter));
    if (!grid->letters) { free(grid); return NULL; }

    // compute cell sizes with remainder distribution to keep edges included
    int base_w = grid_area.width / cols;
    int rem_w = grid_area.width % cols;
    int base_h = grid_area.height / rows;
    int rem_h = grid_area.height % rows;

    int letter_idx = 0;
    int y0 = grid_area.y;
    for (int r = 0; r < rows; r++) {
        int h = base_h + (r < rem_h ? 1 : 0);
        int x0 = grid_area.x;
        for (int c = 0; c < cols; c++) {
            int w = base_w + (c < rem_w ? 1 : 0);

            Rectangle cell = { x0, y0, w, h };

            Letter* letter = extract_single_letter(img, cell, r, c);
            if (letter) {
                // copy struct (pointer ownership of data)
                grid->letters[letter_idx] = *letter;
                free(letter); // free temporary struct but not its data
                letter_idx++;
            } else {
                fprintf(stderr, "Erreur extraction lettre [%d,%d]\n", r, c);
                // leave grid->letters[letter_idx] zeroed
            }

            x0 += w;
        }
        y0 += h;
    }

    grid->count = letter_idx;
    printf("✓ %d lettres extraites et normalisées (28x28)\n", letter_idx);

    return grid;
}

// Memory cleanup
void free_letter(Letter* letter) {
    if (!letter) return;
    if (letter->data) {
        free(letter->data);
        letter->data = NULL;
    }
    // zero-out other fields (optional)
    letter->width = letter->height = letter->original_x = letter->original_y = -1;
    letter->grid_row = letter->grid_col = -1;
}

void free_letter_grid(LetterGrid* grid) {
    if (!grid) return;
    if (grid->letters) {
        for (int i = 0; i < grid->count; i++) {
            if (grid->letters[i].data) {
                free(grid->letters[i].data);
                grid->letters[i].data = NULL;
            }
        }
        free(grid->letters);
        grid->letters = NULL;
    }
    free(grid);
}
