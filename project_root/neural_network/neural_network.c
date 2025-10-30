#include "neural_network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* =========================
   Hyperparameters
   ========================= */
#define EPOCHS       80      // Number of full passes over the training set
#define LR           0.001f  // Base learning rate used by SGD updates
#define TRAIN_SPLIT  0.90f   // Fraction of samples used for training (the rest is test)

/* =========================
   Input binarization
   ========================= */
// 0 = keep grayscale in [0-1]; 1 = convert each pixel to 0/1 using a threshold
#define BINARIZE_INPUT   1   

// 0 = use BIN_FIXED_THR as a global threshold; 1 = Otsu threshold (256-bin histogram)
#define BIN_THR_METHOD   1  

#define BIN_FIXED_THR    160 // If BIN_THR_METHOD==0: pixels >=160 -> 1, else 0 (160 works better than 256 there)

// 0: black background / white letter (MNIST-like); 1: invert (white bg / black letter)
#define BIN_INVERT       0  

/* =========================
   L2 regularization (weight decay)
   ========================= */
#define WD 1e-4f            


/* =========================
   Utils num.
   ========================= */
static float frand01(void){ 
    return (float)rand() / (float)RAND_MAX; 
}

/* He (Kaiming) initialization

   What it is: a weight initialization designed for ReLU (and variants).
   How it works: it keeps the variance of activations roughly constant across layers
   by scaling weights with sqrt(2 / fan_in). Here fan_in == 'in'.
   Why we need it: if weights are too small/large, signals can vanish/explode when
   they pass through many layers. This scheme gives a good starting point for ReLU.
*/
static void he_init(float *W, int in, int out) {
    // scale = sqrt(2 / fan_in) for ReLU
    float scale = sqrtf(2.0f / (float)in);

    // Fill W (size in*out) with random values ~ U(-1,1) then scale
    for (int i = 0; i < in*out; ++i) {
        float r = 2.f * frand01() - 1.f;   // r in [-1, 1], frand01() is uniform in [0,1]
        W[i] = r * scale;                  // apply He scaling
    }
}

/* Xavier (Glorot) initialization

   What it is: a general-purpose init for tanh/sigmoid/linear activations.
   How it works: it balances fan_in and fan_out using sqrt(6 / (fan_in + fan_out))
   for a uniform distribution, so forward and backward variances stay similar.
   Why we need it: helps avoid vanishing/exploding gradients at the start of training
   when activations are not ReLU-like.
*/
static void xavier_init(float *W, int in, int out) {
    // limit for U(-limit, +limit), based on fan_in + fan_out
    float limit = sqrtf(6.0f / (float)(in + out));

    // Fill W with random values ~ U(-1,1), then scale to [-limit, limit]
    for (int i = 0; i < in*out; ++i) {
        float r = 2.f * frand01() - 1.f;   // r in [-1, 1]
        W[i] = r * limit;                  // map to [-limit, +limit]
    }
}


/* =========================
   Cycle of life
   ========================= */
// Initialize one fully-connected layer (alloc memory + weight init)
void init_layer(Layer *L, int in_size, int out_size, int he) {
    L->input_size  = in_size;
    L->output_size = out_size;
    L->W = (float*)malloc(sizeof(float) * in_size * out_size);
    L->b = (float*)calloc(out_size, sizeof(float));
    if (!L->W || !L->b) {
        fprintf(stderr, "OOM layer\n");                
        exit(1);                                     
    }
    if (he)                                            // choose which initialization to use
        he_init(L->W, in_size, out_size);              // He init: good for ReLU layers
    else                                               // otherwise…
        xavier_init(L->W, in_size, out_size);          // Xavier init: good general default (output layer here)
}

// Free all memory owned by one layer
void free_layer(Layer *L) {
    free(L->W); free(L->b);                            // release weights and biases
    L->W = L->b = NULL;                                // clear pointers to avoid dangling references
    L->input_size = L->output_size = 0;                // reset sizes to a safe default
}

// Build the whole network (hidden + output)
void init_network(Network *net) {
    init_layer(&net->hidden, INPUT_SIZE, HIDDEN_SIZE, /*he=*/1); // hidden: 784->512, ReLU, use He init
    init_layer(&net->out,    HIDDEN_SIZE, OUTPUT_SIZE,/*he=*/0); // output: 512->26, use Xavier init
}

// Free the whole network
void free_network(Network *net) {
    free_layer(&net->hidden);                       
    free_layer(&net->out);                            
}

/* =========================
   Forward
   ========================= */

// Fully-connected forward: y = xW + b
void affine_forward(const Layer *L, const float *x, float *y) {
    int in = L->input_size, out = L->output_size;
    for (int i = 0; i < out; ++i)                      // start by copying the bias
        y[i] = L->b[i];                              

    for (int j = 0; j < in; ++j) {                     // loop over input features
        float xj = x[j];                              
        const float *wrow = &L->W[j * out];            // pointer to weights for input j (length = out)
        for (int i = 0; i < out; ++i)                  // add xj * W[j, i] to every output unit
            y[i] += xj * wrow[i];                     
    }
}

// In-place ReLU: replace negatives by 0
void relu_inplace(float *v, int n) {
    for (int i = 0; i < n; ++i)                       
        if (v[i] < 0.f)                               
            v[i] = 0.f;                                
}

// In-place softmax with basic numerical stability
void softmax_inplace(float *z, int n) {
    float m = z[0];                                    // find the max logit m = max(z)
    for (int i = 1; i < n; ++i) if (z[i] > m) m = z[i];

    float s = 0.f;                                     // s = sum(exp(z - m))
    for (int i = 0; i < n; ++i) {                      // subtract m before exp to avoid overflow
        z[i] = expf(z[i] - m);                         // z[i] becomes exp-shifted value
        s += z[i];                                     
    }

    float inv = 1.f / (s + 1e-12f);                    // precompute 1/s with small epsilon
    for (int i = 0; i < n; ++i)                        // normalize to probabilities
        z[i] *= inv;                                   
}

// Run the whole network on one input and return the predicted class index
int predict(const Network *net, const float *x) {
    float h[HIDDEN_SIZE];                              
    float z[OUTPUT_SIZE];                             

    affine_forward(&net->hidden, x, h);                // hidden pre-activation: h = xW1 + b1
    relu_inplace(h, HIDDEN_SIZE);                      // apply ReLU: h = ReLU(h)
    affine_forward(&net->out, h, z);                   // output logits: z = hW2 + b2
    softmax_inplace(z, OUTPUT_SIZE);                   // convert logits to probabilities

    int a = 0;                                         
    for (int i = 1; i < OUTPUT_SIZE; ++i)
        if (z[i] > z[a])
            a = i;
    return a;                                          // return predicted class id
}

/* =========================
   Backprop (SGD + L2)
   ========================= */
/* Returns the NLL loss. Also updates W and b using plain SGD with learning rate `lr`
   and L2 weight decay `WD` (i.e., we add WD * W to each weight gradient). */
float train_one(Network *net, const float *x, int label, float lr) {
    /* forward pass */
    float h[HIDDEN_SIZE];                         
    float z[OUTPUT_SIZE];                           

    affine_forward(&net->hidden, x, h);            // h = x * W1 + b1
    relu_inplace(h, HIDDEN_SIZE);                  // h = ReLU(h)
    affine_forward(&net->out, h, z);               // z = h * W2 + b2  (logits)
    softmax_inplace(z, OUTPUT_SIZE);               // z becomes probabilities (sum = 1)

    /* loss = -log p[label] (negative log-likelihood / cross-entropy) */
    float loss = -logf(z[label] + 1e-12f);         // small epsilon to avoid log(0)

    /* gradients at the output layer */
    float gz[OUTPUT_SIZE];                         // gradient dL/d(logits) after softmax+CE
    for (int i = 0; i < OUTPUT_SIZE; ++i)         
        gz[i] = z[i];                             
    gz[label] -= 1.f;                             

    /* Backprop to hidden: g_h = gz * W_out^T */
    float gh[HIDDEN_SIZE];                         
    for (int j = 0; j < HIDDEN_SIZE; ++j) {       
        float s = 0.f;                             
        for (int i = 0; i < OUTPUT_SIZE; ++i) {
            s += gz[i] * net->out.W[j * OUTPUT_SIZE + i];
        }
        gh[j] = s;                                  // g_h[j] ready (pre-activation gradient)
    }

    /* ReLU derivative: if h[j] <= 0 then gradient is zero (gate) */
    for (int j = 0; j < HIDDEN_SIZE; ++j)          
        if (h[j] <= 0.f)                            // inactive neuron => no gradient flows
            gh[j] = 0.f;

    /* update output layer (W2, b2) with L2 decay */
    for (int j = 0; j < HIDDEN_SIZE; ++j) {        // acts like input to layer 2
        float hj = h[j];                            
        float *Wrow = &net->out.W[j * OUTPUT_SIZE]; 
        for (int i = 0; i < OUTPUT_SIZE; ++i) {
            /* grad_W2[j,i] = hj * gz[i]  (outer product)
               with L2 decay: + WD * W2[j,i] */
            Wrow[i] -= lr * (hj * gz[i] + WD * Wrow[i]); 
        }
    }
    for (int i = 0; i < OUTPUT_SIZE; ++i)          // bias gradient gz[i]
        net->out.b[i] -= lr * gz[i];

    /* update hidden layer (W1, b1) with L2 decay */
    for (int k = 0; k < INPUT_SIZE; ++k) {         // for each pixel k
        float xk = x[k];                            
        float *Wrow = &net->hidden.W[k * HIDDEN_SIZE]; 
        for (int j = 0; j < HIDDEN_SIZE; ++j) {
            /* grad_W1[k,j] = xk * gh[j]
               with L2 decay: + WD * W1[k,j] */
            Wrow[j] -= lr * (xk * gh[j] + WD * Wrow[j]); 
        }
    }
    for (int j = 0; j < HIDDEN_SIZE; ++j)          // bias gradient for layer 1 gh[j]
        net->hidden.b[j] -= lr * gh[j];            // no decay on biases

    return loss;
}


/* =========================
   CSV helpers + loader
   ========================= */

/* starts_with_id:
   Return 1 (true) if the string starts with "id"
   after skipping leading spaces or tabs. Otherwise 0.
   Used to detect a CSV header like:  id,p0,p1,...,p783,label
*/
static int starts_with_id(const char *s) {
    while (*s == ' ' || *s == '\t') ++s;     
    return (s[0] == 'i' && s[1] == 'd');     
}

/* otsu_threshold_256:
   Returns the optimal threshold in [0..255].
*/
static int otsu_threshold_256(const unsigned char *buf, int n) {
    int hist[256] = {0};                       // histogram
    for (int i = 0; i < n; ++i) hist[buf[i]]++; 

    int total = n;                            
    double sum = 0.0;                          
    for (int t = 0; t < 256; ++t) sum += (double)t * hist[t];

    double sumB = 0.0;                         
    int wB = 0;                                 // background weight (number of pixels)
    double maxVar = -1.0;                       // best between-class variance found so far
    int bestT = 128;                            // default threshold if everything is flat

    for (int t = 0; t < 256; ++t) {
        wB += hist[t];                       
        if (wB == 0)                     
            continue;
        int wF = total - wB;                    // foreground weight
        if (wF == 0)                          
            break;

        sumB += (double)t * hist[t];       

        double mB = sumB / wB;                  // background mean
        double mF = (sum - sumB) / wF;          // foreground mean
        double diff = mB - mF;                  // mean difference
        double varBetween = (double)wB * (double)wF * diff * diff; // between-class variance

        if (varBetween > maxVar) {             
            maxVar = varBetween;
            bestT = t;
        }
    }
    return bestT;
}

/* =========================
   CSV loader (id,p0-p783,label)
   Reads a CSV file with rows like:
     id,p0,p1,...,p783,label
   - id is ignored
   - p0-p783 are grayscale pixels in [0-255]
   - label is an int in [0-25] for A-Z
   It returns a Dataset with X normalized to [0-1] (or binarized),
   and y holding the labels.
   ========================= */
Dataset load_csv(const char *path) {
    Dataset D = {0};                            
    FILE *f = fopen(path, "r");               
    if (!f) {                           
        perror("fopen");
        return D;
    }

    float *X = NULL;                            // will hold all images (n * 784)
    unsigned char *y = NULL;                    // will hold all labels (n)
    int n = 0, cap = 0;                         

    char line[20000];                           // CSV lines can be long

    long pos = ftell(f);                       
    if (fgets(line, sizeof(line), f)) {        
        if (!starts_with_id(line))              // if not a header
            fseek(f, pos, SEEK_SET);            
    }

    /* Read each CSV line -> parse id, pixels, label */
    while (fgets(line, sizeof(line), f)) {
        if (n >= cap) {
            int ncap = (cap == 0) ? 1024 : cap * 2; // new capacity
            float *Xn = (float*)realloc(X, sizeof(float) * ncap * INPUT_SIZE);
            unsigned char *yn = (unsigned char*)realloc(y, sizeof(unsigned char) * ncap);
            if (!Xn || !yn) {                   // out of memory check
                fprintf(stderr, "OOM csv\n");
                exit(1);
            }
            X = Xn; y = yn; cap = ncap;         // new buffers
        }

        /* parse: id, p0-p783, label */
        char *tok = strtok(line, ",");         
        if (!tok)                            
            continue;

        /* read 784 pixel bytes into a temp buffer u8[] */
        unsigned char u8[INPUT_SIZE];
        int ok = 1;
        for (int i = 0; i < INPUT_SIZE; ++i) {
            tok = strtok(NULL, ",");           
            if (!tok) {                        
                ok = 0;
                break;
            }
            long v = strtol(tok, NULL, 10);     // parse integer
            if (v < 0)  v = 0;                  // clamp to [0-255]
            if (v > 255) v = 255;
            u8[i] = (unsigned char)v;           // store pixel
        }
        if (!ok)                               
            continue;

        tok = strtok(NULL, ",\r\n");            // read label (last column)
        if (!tok)                              
            continue;
        long lab = strtol(tok, NULL, 10);     
        if (lab < 0 || lab >= OUTPUT_SIZE)      // must be in [0-25]
            continue;

        float *row = &X[n * INPUT_SIZE];      

#if BINARIZE_INPUT
        /* If BINARIZE_INPUT == 1:
           We threshold each image to 0/1 either using Otsu (per image)
           or a fixed threshold. Optionally invert foreground/background. */
        int thr = (BIN_THR_METHOD == 1) ? otsu_threshold_256(u8, INPUT_SIZE)
                                        : BIN_FIXED_THR;
        for (int i = 0; i < INPUT_SIZE; ++i) {
            int fg = (u8[i] >= thr);            
            if (BIN_INVERT)                     
                fg = !fg;
            row[i] = fg ? 1.0f : 0.0f;         
        }
#else
        /* If BINARIZE_INPUT == 0:
           We keep grayscale and just normalize to [0-1].
           Optionally invert grayscale. */
        for (int i = 0; i < INPUT_SIZE; ++i) {
            int v = u8[i];
            if (BIN_INVERT)                    
                v = 255 - v;
            row[i] = v / 255.0f;                // normalize to [0-1]
        }
#endif
        y[n] = (unsigned char)lab;              
        n++;                                    
    }
    fclose(f);                                   

    /* fill the Dataset struct and log a small summary */
    D.n = n; D.X = X; D.y = y;
    fprintf(stderr, "CSV: %s -> %d samples (binarize=%d method=%s invert=%d)\n",
            path, n, BINARIZE_INPUT, (BIN_THR_METHOD ? "Otsu" : "fixed"), BIN_INVERT);
    return D;                                    
}

void free_dataset(Dataset *D) {
    free(D->X);
    free(D->y);
    D->X = NULL;
    D->y = NULL;
    D->n = 0;
}

/* =========================
   Shuffle of indices
   Fisher–Yates shuffle in-place: gives a random permutation.
   ========================= */
void shuffle_idx(int *idx, int n) {
    for (int i = n - 1; i > 0; --i) {          
        int j = rand() % (i + 1);              // pick a random index in [0-i]
        int t = idx[i];                        
        idx[i] = idx[j];
        idx[j] = t;
    }
}

/* =========================
   Save / Load model (binary)
   We just dump the raw arrays in a fixed order so load_model
   can read them back exactly the same way.
   ========================= */

/* save_model:
   Open a file in binary write mode, write all weights/biases,
   then close. Returns 0 on success, -1 if opening failed.
*/
int save_model(const char *path, const Network *net) {
    FILE *f = fopen(path, "wb");                               
    if (!f) return -1;                                         

    /* write hidden layer weights then biases */
    fwrite(net->hidden.W, sizeof(float), INPUT_SIZE * HIDDEN_SIZE, f);  
    fwrite(net->hidden.b, sizeof(float), HIDDEN_SIZE, f);            

    /* write output layer weights then biases */
    fwrite(net->out.W,    sizeof(float), HIDDEN_SIZE * OUTPUT_SIZE, f); 
    fwrite(net->out.b,    sizeof(float), OUTPUT_SIZE, f);               

    fclose(f);                                                 
    return 0;                                                 
}

/* load_model:
   Open a file in binary read mode, read all arrays in the same
   order they were saved, then close. Return 0 if all counts match,
   -1 if open failed, -2 if file contents had the wrong size.
*/
int load_model(const char *path, Network *net) {
    FILE *f = fopen(path, "rb");                               
    if (!f)                                                   
        return -1;

    /* read back hidden layer (weights then biases) */
    size_t r1 = fread(net->hidden.W, sizeof(float), INPUT_SIZE * HIDDEN_SIZE, f);
    size_t r2 = fread(net->hidden.b, sizeof(float), HIDDEN_SIZE, f);

    /* read back output layer (weights then biases) */
    size_t r3 = fread(net->out.W,    sizeof(float), HIDDEN_SIZE * OUTPUT_SIZE, f);
    size_t r4 = fread(net->out.b,    sizeof(float), OUTPUT_SIZE, f);

    fclose(f);                                               

    /* verify we read exactly the expected number of floats */
    return (r1 == INPUT_SIZE * HIDDEN_SIZE &&           
            r2 == HIDDEN_SIZE &&
            r3 == HIDDEN_SIZE * OUTPUT_SIZE &&
            r4 == OUTPUT_SIZE) ? 0 : -2;
}

/* =========================
   Main: train / test CLI
   Usage:
     ./nn train <data.csv> [model.bin]
     ./nn test  <data.csv> <model.bin>
   ========================= */


