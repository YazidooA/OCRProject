#include "nn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================
 *  LOCAL SHAPES (SEULEMENT DANS CE FICHIER)
 * ============================================================ */

#define H   IMAGE_SIZE
#define W   IMAGE_SIZE
#define HO  (IMAGE_SIZE / POOL)
#define WO  (IMAGE_SIZE / POOL)

/* Small helper for 3D tensor [C, H, W] stored as [c][y][x]. */
static inline int NN_I3(int c, int y, int x, int C, int HH, int WW)
{
    (void)C;
    return c * HH * WW + y * WW + x;
}

/* Fast clamp (pas forcément utilisé ici mais safe). */
static inline float NN_clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

/* ============================================================
 *  INIT RESEAU (SIMPLE)
 * ============================================================ */

void init_network(Network *net)
{
    /* Pour l'interface : on met tout à zéro.
       Ensuite tu fais load_model("model.bin", &net); */
    memset(net, 0, sizeof(*net));
}

/* ============================================================
 *  BASIC MATH HELPERS
 * ============================================================ */

/* In-place softmax on a vector of logits. */
static void softmax(float *z, int n)
{
    float m = z[0];
    for (int i = 1; i < n; ++i)
        if (z[i] > m) m = z[i];

    float s = 0.f;
    for (int i = 0; i < n; ++i) {
        z[i] = expf(z[i] - m);
        s   += z[i];
    }
    float inv = 1.f / (s + 1e-12f);
    for (int i = 0; i < n; ++i)
        z[i] *= inv;
}

/* Log-softmax with temperature (for calibration / top-k). */
static void log_softmax_T(const float *z, int n, float *logp, float temperature)
{
    float T    = (temperature > 0.f ? temperature : 1.0f);
    float invT = 1.0f / T;

    float m = z[0] * invT;
    for (int i = 1; i < n; ++i) {
        float v = z[i] * invT;
        if (v > m) m = v;
    }

    double sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += expf(z[i] * invT - m);

    float logZ = m + logf((float)sum);

    for (int i = 0; i < n; ++i)
        logp[i] = z[i] * invT - logZ;
}

/* Small O(nk) top-k selection, descending scores. */
static int topk_desc(const float *score, int n, int k, int *idx)
{
    if (k < 1)    k = 1;
    if (k > n)    k = n;
    int used[OUTPUT_SIZE] = {0};

    for (int t = 0; t < k; ++t) {
        int best = -1;
        for (int i = 0; i < n; ++i) {
            if (used[i]) continue;
            if (best < 0 || score[i] > score[best])
                best = i;
        }
        idx[t]    = best;
        used[best] = 1;
    }
    return k;
}

/* ============================================================
 *  FORWARD PASS (INFERENCE ONLY)
 * ============================================================ */

/* conv1: input x[1,28,28] -> y1[C1_OUT,28,28], ReLU in place. */
static void conv1_forward(const Network *net, const float *x, float *y1)
{
    for (int oc = 0; oc < C1_OUT; ++oc) {
        const float *F = &net->Wc1[oc * K1 * K1];
        float b        = net->bc1[oc];

        for (int y = 0; y < H; ++y) {
            for (int x0 = 0; x0 < W; ++x0) {
                float s = b;
                for (int ky = 0; ky < K1; ++ky) {
                    int yy = y + ky - PAD1;
                    if ((unsigned)yy >= (unsigned)H) continue;
                    for (int kx = 0; kx < K1; ++kx) {
                        int xx = x0 + kx - PAD1;
                        if ((unsigned)xx >= (unsigned)W) continue;
                        s += x[yy * W + xx] * F[ky * K1 + kx];
                    }
                }
                int idx = NN_I3(oc, y, x0, C1_OUT, H, W);
                y1[idx] = (s > 0.f) ? s : 0.f;
            }
        }
    }
}

/* conv2: y1[C1_OUT,28,28] -> y1b[C2_OUT,28,28], ReLU in place. */
static void conv2_forward(const Network *net, const float *y1, float *y1b)
{
    for (int oc = 0; oc < C2_OUT; ++oc) {
        float b = net->bc2[oc];
        const float *Foc = &net->Wc2[oc * (C1_OUT * K2 * K2)];

        for (int y = 0; y < H; ++y) {
            for (int x0 = 0; x0 < W; ++x0) {
                float s = b;
                for (int ic = 0; ic < C1_OUT; ++ic) {
                    const float *F = &Foc[ic * (K2 * K2)];
                    for (int ky = 0; ky < K2; ++ky) {
                        int yy = y + ky - PAD2;
                        if ((unsigned)yy >= (unsigned)H) continue;
                        for (int kx = 0; kx < K2; ++kx) {
                            int xx = x0 + kx - PAD2;
                            if ((unsigned)xx >= (unsigned)W) continue;
                            s += y1[NN_I3(ic, yy, xx, C1_OUT, H, W)] *
                                 F[ky * K2 + kx];
                        }
                    }
                }
                y1b[NN_I3(oc, y, x0, C2_OUT, H, W)] = (s > 0.f) ? s : 0.f;
            }
        }
    }
}

/* Average pool 2x2: [C,H,W] -> [C,HO,WO]. */
static void avgpool2x2_forward(const float *x, int C, float *y)
{
    for (int c = 0; c < C; ++c) {
        for (int y0 = 0; y0 < HO; ++y0) {
            for (int x0 = 0; x0 < WO; ++x0) {
                int yy = 2 * y0;
                int xx = 2 * x0;
                float m =
                    x[NN_I3(c, yy,   xx,   C, H, W)] +
                    x[NN_I3(c, yy,   xx+1, C, H, W)] +
                    x[NN_I3(c, yy+1, xx,   C, H, W)] +
                    x[NN_I3(c, yy+1, xx+1, C, H, W)];
                y[NN_I3(c, y0, x0, C, HO, WO)] = 0.25f * m;
            }
        }
    }
}

/* Fully-connected: flatten C2_OUT*HO*WO -> OUTPUT_SIZE logits. */
static void fc_forward(const Network *net, const float *y2, float *z)
{
    int F = C2_OUT * HO * WO;
    for (int i = 0; i < OUTPUT_SIZE; ++i) {
        float s = net->bf[i];
        const float *w = &net->Wf[i * F];
        for (int j = 0; j < F; ++j)
            s += y2[j] * w[j];
        z[i] = s;
    }
}

/* Variant: conv2 on 14x14 feature maps (pool before conv2). */
static void conv2_forward14(const Network *net, const float *in14, float *out14)
{
    for (int oc = 0; oc < C2_OUT; ++oc) {
        for (int y = 0; y < HO; ++y) {
            for (int x = 0; x < WO; ++x) {
                float s = net->bc2[oc];
                const float *Foc = &net->Wc2[oc * (C1_OUT * K2 * K2)];

                for (int ic = 0; ic < C1_OUT; ++ic) {
                    const float *F = &Foc[ic * (K2 * K2)];
                    for (int ky = 0; ky < K2; ++ky) {
                        int yy = y + ky - PAD2;
                        if ((unsigned)yy >= (unsigned)HO) continue;
                        for (int kx = 0; kx < K2; ++kx) {
                            int xx = x + kx - PAD2;
                            if ((unsigned)xx >= (unsigned)WO) continue;
                            s += F[ky * K2 + kx] *
                                 in14[NN_I3(ic, yy, xx, C1_OUT, HO, WO)];
                        }
                    }
                }
                out14[NN_I3(oc, y, x, C2_OUT, HO, WO)] = (s > 0.f) ? s : 0.f;
            }
        }
    }
}

/* ============================================================
 *  PUBLIC INFERENCE API
 * ============================================================ */

/* Simple forward (pool after conv2) -> softmax -> argmax. */
int predict(const Network *net, const float *x01)
{
    float y1  [C1_OUT * H * W];
    float y1p [C1_OUT * HO * WO];
    float y2b [C2_OUT * HO * WO];
    float z   [OUTPUT_SIZE];

    conv1_forward(net, x01, y1);
    avgpool2x2_forward(y1, C1_OUT, y1p);
    conv2_forward14(net, y1p, y2b);
    fc_forward(net, y2b, z);
    softmax(z, OUTPUT_SIZE);

    int a = 0;
    for (int i = 1; i < OUTPUT_SIZE; ++i)
        if (z[i] > z[a])
            a = i;
    return a;
}

/* Convenience wrapper on top of smart_predict_k(k=1). */
int smart_predict(const Network *net, const float *x01)
{
    int idx;
    smart_predict_k(net, x01, 1, &idx, NULL, NULL);
    return idx;
}

/* Single forward + log-softmax + top-k.
 * If you don't need some outputs, pass NULL.
 */
int smart_predict_k(const Network *net, const float *x01, int k,
                    int *out_idx, float *out_logp, float *out_prob)
{
    float y1  [C1_OUT * H * W];
    float y1b [C2_OUT * H * W];
    float y2  [C2_OUT * HO * WO];
    float z   [OUTPUT_SIZE];
    float logp[OUTPUT_SIZE];

    /* Forward path: conv1 -> conv2 -> pool -> fc. */
    conv1_forward(net, x01, y1);
    conv2_forward(net, y1, y1b);
    avgpool2x2_forward(y1b, C2_OUT, y2);
    fc_forward(net, y2, z);

    const float T = 1.0f; /* temperature, can be tuned */
    log_softmax_T(z, OUTPUT_SIZE, logp, T);

    if (k > OUTPUT_SIZE)
        k = OUTPUT_SIZE;

    int idxk[OUTPUT_SIZE];
    int kk = topk_desc(logp, OUTPUT_SIZE, k, idxk);

    for (int t = 0; t < kk; ++t) {
        int c = idxk[t];
        if (out_idx)   out_idx[t]  = c;
        if (out_logp)  out_logp[t] = logp[c];
        if (out_prob)  out_prob[t] = expf(logp[c]);
    }
    return kk;
}

/* ============================================================
 *  SAVE / LOAD
 * ============================================================ */

int save_model(const char *path, const Network *net)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    unsigned int magic = 0x324E4E43;  /* "CNN2" */
    fwrite(&magic, 4, 1, f);

    fwrite(net->Wc1, sizeof(float), C1_OUT * K1 * K1,          f);
    fwrite(net->bc1, sizeof(float), C1_OUT,                    f);
    fwrite(net->Wc2, sizeof(float), C2_OUT * C1_OUT * K2 * K2, f);
    fwrite(net->bc2, sizeof(float), C2_OUT,                    f);
    fwrite(net->Wf,  sizeof(float), (C2_OUT * HO * WO) * OUTPUT_SIZE, f);
    fwrite(net->bf,  sizeof(float), OUTPUT_SIZE,               f);

    fclose(f);
    return 0;
}

int load_model(const char *path, Network *net)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    unsigned int magic = 0;
    if (fread(&magic, 4, 1, f) != 1) {
        fclose(f);
        return -2;
    }
    if (magic != 0x324E4E43) {
        fclose(f);
        return -3;
    }

    size_t r1 = fread(net->Wc1, sizeof(float), C1_OUT * K1 * K1,          f);
    size_t r2 = fread(net->bc1, sizeof(float), C1_OUT,                    f);
    size_t r3 = fread(net->Wc2, sizeof(float), C2_OUT * C1_OUT * K2 * K2, f);
    size_t r4 = fread(net->bc2, sizeof(float), C2_OUT,                    f);
    size_t r5 = fread(net->Wf,  sizeof(float), (C2_OUT * HO * WO) * OUTPUT_SIZE, f);
    size_t r6 = fread(net->bf,  sizeof(float), OUTPUT_SIZE,               f);
    fclose(f);

    return (r1 == (size_t)(C1_OUT * K1 * K1) &&
            r2 == (size_t)C1_OUT &&
            r3 == (size_t)(C2_OUT * C1_OUT * K2 * K2) &&
            r4 == (size_t)C2_OUT &&
            r5 == (size_t)(C2_OUT * HO * WO) * OUTPUT_SIZE &&
            r6 == (size_t)OUTPUT_SIZE) ? 0 : -4;
}
