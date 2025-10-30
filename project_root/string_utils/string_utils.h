#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include "../common.h"

// Manipulation de chaînes
char* safe_strdup(const char* str);
void safe_strcpy(char* dest, const char* src, size_t dest_size);
void safe_strcat(char* dest, const char* src, size_t dest_size);

// Transformation de chaînes
void string_to_upper(char* str);
void string_to_lower(char* str);
void trim_whitespace(char* str);
void remove_extra_spaces(char* str);

// Recherche et validation
bool string_contains(const char* haystack, const char* needle);
bool string_starts_with(const char* str, const char* prefix);
bool string_ends_with(const char* str, const char* suffix);
bool is_valid_word_character(char c);

// Utilitaires de parsing
char** split_string(const char* str, char delimiter, int* count);
void free_string_array(char** array, int count);
int count_occurrences(const char* str, char target);

// Comparaison
int case_insensitive_compare(const char* str1, const char* str2);
double string_similarity(const char* str1, const char* str2);

#endif // STRING_UTILS_H