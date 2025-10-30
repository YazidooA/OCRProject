#ifndef ACTIVATION_H
#define ACTIVATION_H

#include "../common.h"

// Fonctions d'activation
double sigmoid(double x);
double sigmoid_derivative(double x);
double relu(double x);
double relu_derivative(double x);
double tanh_activation(double x);
double tanh_derivative(double x);

// Fonctions vectorielles
void softmax(const double* input, double* output, int size);
void apply_activation(double* values, int size, double (*activation_func)(double));

// Fonctions de coût
double mean_squared_error(const double* predicted, const double* actual, int size);
double cross_entropy_loss(const double* predicted, const double* actual, int size);

#endif // ACTIVATION_H