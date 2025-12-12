#ifndef NN_H
#define NN_H

#include <SDL2/SDL.h>

/* =========================
 *  CNN HYPERPARAMETERS
 * ========================= */

/* Input: 28x28 grayscale letters. */
#define IMAGE_SIZE   28

/* Output: 26 classes, 'A'..'Z'. */
#define OUTPUT_SIZE  26

/* Convolution block 1: 1 channel -> C1_OUT feature maps (28x28). */
#define C1_OUT       64
#define K1           5   /* 5x5 kernel */
#define PAD1         2   /* padding to keep 28x28 */

/* Convolution block 2: C1_OUT -> C2_OUT feature maps. */
#define C2_OUT       64
#define K2           3   /* 3x3 kernel */
#define PAD2         1   /* padding to keep 28x28 */

/* Average pooling 2x2: 28x28 -> 14x14. */
#define POOL         2

/* =========================
 *  CNN NETWORK STRUCTURE
 * ========================= */

typedef struct {
    /* conv1: 1 -> C1_OUT, feature maps 28x28 */
    float Wc1[C1_OUT * K1 * K1];  /* layout: [oc, ky, kx] */
    float bc1[C1_OUT];

    /* conv2: C1_OUT -> C2_OUT, feature maps 28x28 */
    float Wc2[C2_OUT * C1_OUT * K2 * K2]; /* layout: [oc, ic, ky, kx] */
    float bc2[C2_OUT];

    /* Fully connected: (C2_OUT * 14 * 14) -> OUTPUT_SIZE */
    float Wf[(C2_OUT * (IMAGE_SIZE/POOL) * (IMAGE_SIZE/POOL)) * OUTPUT_SIZE];
    float bf[OUTPUT_SIZE];
} Network;

/* =========================
 *  PUBLIC API
 * ========================= */

/* Init "safe" : on met tout à zéro.
   En pratique, tu feras ensuite load_model("model.bin", &net); */
void  init_network(Network *net);

/* Simple prediction (single forward, top-1 argmax). */
int   predict(const Network *net, const float *x01);

/* "Smart" API: one forward, then optional top-k + probs/log-probs.
 *  - k         : requested top-k (clamped to OUTPUT_SIZE)
 *  - out_idx   : size>=k, receives class indices
 *  - out_logp  : optional (can be NULL), log-probabilities
 *  - out_prob  : optional (can be NULL), probabilities
 * Returns: actual number of entries written (<= k).
 */
int   smart_predict_k(const Network *net, const float *x01, int k,
                      int *out_idx, float *out_logp, float *out_prob);

/* Convenience wrapper: behaves like predict(), but uses smart path. */
int   smart_predict(const Network *net, const float *x01);

/* Save / load weights to a binary file. */
int   save_model(const char *path, const Network *net);
int   load_model(const char *path, Network *net);

#endif /* NN_H */
