#ifndef NN_TRAIN_H
#define NN_TRAIN_H

#include "nn.h"
#include <stddef.h>

/* =========================
 *  TRAINING HYPERPARAMETERS
 * ========================= */

#define LR           0.0008f   /* base learning rate   */
#define WD           2e-4f     /* L2 weight decay      */
#define EPOCHS       90        /* example max epochs   */
#define TRAIN_SPLIT  0.90f     /* 90% train / 10% val  */

/* Optional binarisation for CSV-loaded images. */
#define BINARIZE     1         /* 1: threshold (THR)  /  0: gray/255.f   */
#define THR          160       /* binary threshold on [0..255]           */
#define INVERT       0         /* 1: invert pixels, 0: keep as-is        */

/* =========================
 *  DATASET STRUCTURE
 * ========================= */

typedef struct {
    int             n;   /* number of samples */
    float          *X;   /* [n * 784] floats in [0,1], row-major */
    unsigned char  *y;   /* [n] labels in [0..25] */
} Dataset;

/* Load a CSV of the form:
 *   id,p0,...,p783,label
 * pixels in [0..255], label in [0..25] or [A..Z]/[a..z].
 */
Dataset load_csv(const char *path);

/* Free the buffers allocated by load_csv(). */
void    free_dataset(Dataset *D);

/* Fisher–Yates shuffle on a list of indices [0..n-1]. */
void    shuffle_idx(int *idx, int n);

/* =========================
 *  TRAINING PRIMITIVES
 * ========================= */

/* He/Xavier initialization of all layers. */
void  init_network(Network *net);

/* One SGD step (forward + backward) on a single sample.
 *  - x01   : pointer to 28x28 float image in [0,1]
 *  - label : integer in [0..25]
 *  - lr    : learning rate for this step
 * Returns: cross-entropy loss (with label smoothing).
 */
float train_one(Network *net, const float *x01, int label, float lr);

/* Label-preserving data augmentation (optional).
 *  - dst, src : [H*W] images in stroke convention (1=bg, 0=stroke)
 *  - label    : class in [0..25]
 *  - rng_state: pointer to RNG state for reproducibility
 */
void  augment_sample(float *dst, const float *src, int label,
                     unsigned *rng_state);

#endif /* NN_TRAIN_H */
