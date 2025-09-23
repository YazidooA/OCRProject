#ifndef TRAINING_H
#define TRAINING_H

#include "../common.h"
#include "neural_network.h"

typedef struct {
    double* input;
    double* expected_output;
} TrainingExample;

typedef struct {
    TrainingExample* examples;
    int count;
    int input_size;
    int output_size;
} TrainingDataset;

// Gestion du dataset
TrainingDataset* create_dataset(int max_examples, int input_size, int output_size);
void add_training_example(TrainingDataset* dataset, const double* input, const double* output);
void free_dataset(TrainingDataset* dataset);

// Entraînement
void train_network(NeuralNetwork* nn, TrainingDataset* dataset, int epochs);
void train_batch(NeuralNetwork* nn, TrainingExample* batch, int batch_size);
double calculate_accuracy(NeuralNetwork* nn, TrainingDataset* test_set);

// Chargement de données
TrainingDataset* load_training_data_from_directory(const char* directory);
TrainingDataset* load_training_data_from_file(const char* filename);

#endif // TRAINING_H