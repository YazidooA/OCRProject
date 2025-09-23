#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdbool.h>

// Structures de base
typedef struct {
    int x, y;
} Position;

typedef struct {
    int x, y, width, height;
} Rectangle;

typedef struct {
    unsigned char* data;
    int width;
    int height;
    int channels;
    char* filename;
} Image;

// Constantes globales
#define MAX_WORD_LENGTH 50
#define MAX_WORDS 100
#define MAX_GRID_SIZE 50
#define IMAGE_THRESHOLD 128

// Codes d'erreur
typedef enum {
    SUCCESS = 0,
    ERROR_FILE_NOT_FOUND,
    ERROR_MEMORY_ALLOCATION,
    ERROR_INVALID_FORMAT,
    ERROR_PROCESSING_FAILED
} ErrorCode;

#endif // COMMON_H