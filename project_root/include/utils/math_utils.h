#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include "../common.h"

// Fonctions mathématiques de base
double clamp(double value, double min_val, double max_val);
int max_int(int a, int b);
int min_int(int a, int b);
double max_double(double a, double b);
double min_double(double a, double b);

// Fonctions trigonométriques utilitaires
double degrees_to_radians(double degrees);
double radians_to_degrees(double radians);
double normalize_angle(double angle);

// Statistiques
double calculate_mean_array(const double* array, int size);
double calculate_variance_array(const double* array, int size);
double calculate_standard_deviation(const double* array, int size);

// Géométrie
double calculate_distance(Position p1, Position p2);
double calculate_angle_between_points(Position p1, Position p2);
bool point_in_rectangle(Position point, Rectangle rect);
Rectangle get_bounding_rectangle(Position* points, int count);

// Utilitaires numériques
int round_to_int(double value);
bool is_approximately_equal(double a, double b, double epsilon);
double random_double(double min_val, double max_val);
int random_int(int min_val, int max_val);

// Opérations matricielles simples
void matrix_multiply(double** a, double** b, double** result, int rows_a, int cols_a, int cols_b);
void matrix_transpose(double** matrix, double** result, int rows, int cols);
void matrix_add(double** a, double** b, double** result, int rows, int cols);

#endif // MATH_UTILS_H