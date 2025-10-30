#ifndef NN_H
#define NN_H

/* Fixed sizes (dataset 28x28, labels A-Z) */
#define IMAGE_SIZE   28
#define INPUT_SIZE   (IMAGE_SIZE * IMAGE_SIZE)  /* 784 */
#define HIDDEN_SIZE  512
#define OUTPUT_SIZE  26

/* One fully-connected layer */
typedef struct {
    int   input_size;
    int   output_size;
    float *W;  /* weights [input_size * output_size], rows = inputs */
    float *b;  /* biases  [output_size] */
} Layer;

/* Tiny network: 784 -> 512 (ReLU) -> 26 (logits) */
typedef struct {
    Layer hidden; /* 784 -> 512 (hidden neurons) with ReLU -> ReLU = Rectified Linear Unit -> ReLU(x) max(0,x) */
    Layer out;    /* 512 ->  26 (A-Z)   */
} Network;

/* Lifecycle */
void init_layer(Layer *L, int in_size, int out_size, int he_init);
void free_layer(Layer *L);

void init_network(Network *net);
void free_network(Network *net);

/* Inference */
void affine_forward(const Layer *L, const float *x, float *y); /* y = b + xW */
void relu_inplace(float *v, int n);
void softmax_inplace(float *z, int n);
int  predict(const Network *net, const float *x);              /* argmax */

/* One training step
   Returns NLL = -log p[label].
*/
float train_one(Network *net, const float *x, int label, float lr);

/* CSV I/O (id,p0-p783,label) */
typedef struct {
    int n;                 /* number of samples */
    float *X;              /* [n * INPUT_SIZE], in [0-1] or {0,1} */
    unsigned char *y;      /* [n], labels 0-25 */
} Dataset;

Dataset load_csv(const char *path);
void    free_dataset(Dataset *D);

/* Utils */
void shuffle_idx(int *idx, int n);

/* Save / Load model */
int save_model(const char *path, const Network *net);
int load_model(const char *path, Network *net);

#endif
