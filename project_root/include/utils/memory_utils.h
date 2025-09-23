// ===========================================
// include/utils/memory_utils.h
// ===========================================
#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include "../common.h"

// Allocation sécurisée
void* safe_malloc(size_t size);
void* safe_calloc(size_t num, size_t size);
void* safe_realloc(void* ptr, size_t new_size);
void safe_free(void** ptr);

// Allocation de tableaux 2D
char** allocate_2d_char_array(int rows, int cols);
double** allocate_2d_double_array(int rows, int cols);
int** allocate_2d_int_array(int rows, int cols);

// Libération de tableaux 2D
void free_2d_char_array(char** array, int rows);
void free_2d_double_array(double** array, int rows);
void free_2d_int_array(int** array, int rows);

// Utilitaires de copie
void* safe_memcpy(void* dest, const void* src, size_t n);
char* safe_strdup(const char* str);

// Statistiques mémoire (debug)
void print_memory_usage(void);
void reset_memory_counters(void);

#endif // MEMORY_UTILS_H