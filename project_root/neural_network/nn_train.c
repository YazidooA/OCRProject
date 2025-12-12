#include "nn_train.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>

/* ============================================================
 *  BASIC MATH / RANDOM HELPERS
 * ============================================================ */

static inline float frand01(void)
{
    return (float)rand() / (float)RAND_MAX;
}

static void he_init_conv_1ch(float *W, int k)
{
    /* fan_in = 1 * k * k */
    float s = sqrtf(2.f / (float)(k * k));
    for (int i = 0; i < k * k; ++i)
        W[i] = (2.f * frand01() - 1.f) * s;
}

static void he_init_conv_ch(float *W, int in_ch, int k, int count)
{
    /* fan_in = in_ch * k * k ; "count" = total number of weights. */
    float s = sqrtf(2.f / (float)(in_ch * k * k));
    for (int i = 0; i < count; ++i)
        W[i] = (2.f * frand01() - 1.f) * s;
}

static void xavier_init(float *W, int fin, int fout)
{
    float a = sqrtf(6.f / (float)(fin + fout));
    for (int i = 0; i < fin * fout; ++i)
        W[i] = (2.f * frand01() - 1.f) * a;
}

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

/* ============================================================
 *  LABEL PARSING
 * ============================================================ */

/* Parse a label in 26 classes.
 * Accepts:
 *   - "0".."25"
 *   - "A".."Z" / "a".."z"
 * Returns -1 if invalid.
 */
static int parse_label26(const char *tok)
{
    if (!tok || !*tok) return -1;

    /* Single-char letter. */
    if (tok[1] == '\0') {
        unsigned char c = (unsigned char)tok[0];
        if (c >= 'A' && c <= 'Z') return (int)(c - 'A');
        if (c >= 'a' && c <= 'z') return (int)(c - 'a');
    }

    /* Decimal integer. */
    char *end = NULL;
    long v = strtol(tok, &end, 10);
    if (end == tok)          return -1;
    if (v < 0 || v >= OUTPUT_SIZE) return -1;
    return (int)v;
}

/* ============================================================
 *  LIGHT RNG FOR AUGMENTATION
 * ============================================================ */

static inline unsigned rng_next_u32(unsigned *st)
{
    *st = (*st) * 1664525u + 1013904223u;
    return *st;
}

static inline float rng_f01(unsigned *st)
{
    return (rng_next_u32(st) >> 8) * (1.0f / 16777216.0f);
}

static inline int rng_int(unsigned *st, int a, int b)
{
    /* inclusive range [a,b] */
    float u = rng_f01(st);
    int r = a + (int)(u * (float)(b - a + 1));
    if (r < a) r = a;
    if (r > b) r = b;
    return r;
}

/* ============================================================
 *  SMALL MORPHO OPERATORS (INPUT DOMAIN: 1 = bg, 0 = stroke)
 * ============================================================ */

static void min3x3(const float *in, float *out)
{
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float m = 1.f;
            for (int ky = -1; ky <= 1; ++ky) {
                int yy = y + ky; if (yy < 0 || yy >= H) continue;
                for (int kx = -1; kx <= 1; ++kx) {
                    int xx = x + kx; if (xx < 0 || xx >= W) continue;
                    float v = in[yy * W + xx];
                    if (v < m) m = v;
                }
            }
            out[y * W + x] = m;
        }
}

static void max3x3(const float *in, float *out)
{
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            float M = 0.f;
            for (int ky = -1; ky <= 1; ++ky) {
                int yy = y + ky; if (yy < 0 || yy >= H) continue;
                for (int kx = -1; kx <= 1; ++kx) {
                    int xx = x + kx; if (xx < 0 || xx >= W) continue;
                    float v = in[yy * W + xx];
                    if (v > M) M = v;
                }
            }
            out[y * W + x] = M;
        }
}

static void copy_img(const float *in, float *out)
{
    memcpy(out, in, sizeof(float) * H * W);
}

/* Force a white vertical band on the right side (simulates an opening). */
static void lighten_right_band(float *img, int band)
{
    if (band < 1) band = 1;
    if (band > 4) band = 4;
    for (int y = 0; y < H; ++y)
        for (int x = W - band; x < W; ++x)
            if (x >= 0) img[y * W + x] = 1.f;
}

/* Thicken the right band (reinforce right stroke / stem). */
static void dilate_right_band(const float *in, float *out, int band)
{
    if (band < 1) band = 1;
    if (band > 4) band = 4;
    copy_img(in, out);
    float tmp[H * W];
    min3x3(in, tmp);
    for (int y = 0; y < H; ++y)
        for (int x = W - band; x < W; ++x)
            if (x >= 0) {
                float v = out[y * W + x];
                float d = tmp[y * W + x];
                out[y * W + x] = fminf(v, d);
            }
}

/* Erase a few bottom rows (reduce the "feet"). */
static void trim_bottom_rows(float *img, int rows)
{
    if (rows < 1) rows = 1;
    if (rows > 3) rows = 3;
    for (int y = H - rows; y < H; ++y) {
        if (y < 0) continue;
        for (int x = 0; x < W; ++x)
            img[y * W + x] = 1.f;
    }
}

/* Thicken the bottom band (reinforce feet). */
static void thicken_bottom_band(const float *in, float *out, int rows)
{
    if (rows < 1) rows = 1;
    if (rows > 3) rows = 3;
    copy_img(in, out);
    float tmp[H * W];
    min3x3(in, tmp);
    for (int y = H - rows; y < H; ++y) {
        if (y < 0) continue;
        for (int x = 0; x < W; ++x)
            out[y * W + x] = fminf(out[y * W + x], tmp[y * W + x]);
    }
}

/* Small label-invariant translation. */
static void shift_copy(const float *in, float *out, int dx, int dy)
{
    for (int i = 0; i < H * W; ++i)
        out[i] = 1.f; /* white background */

    for (int y = 0; y < H; ++y) {
        int ys = y - dy; if (ys < 0 || ys >= H) continue;
        for (int x = 0; x < W; ++x) {
            int xs = x - dx; if (xs < 0 || xs >= W) continue;
            out[y * W + x] = in[ys * W + xs];
        }
    }
}

/* ============================================================
 *  CONFUSION CLUSTERS (A=0..Z)
 * ============================================================ */
/* Vertical letters: I(8), K(10), L(11), T(19), F(5) */
static int cluster_vert(int lbl)
{
    return (lbl == 8 || lbl == 10 || lbl == 11 || lbl == 19 || lbl == 5);
}
/* Round / circular: O(14), D(3), Q(16), C(2) */
static int cluster_round(int lbl)
{
    return (lbl == 14 || lbl == 3 || lbl == 16 || lbl == 2);
}
/* Bowl-shaped: P(15), B(1), R(17) */
static int cluster_bowl(int lbl)
{
    return (lbl == 15 || lbl == 1 || lbl == 17);
}

/* Classes considered "hard" globally. */
static int is_hard(int lbl)
{
    return cluster_vert(lbl) || cluster_round(lbl) || cluster_bowl(lbl);
}

/* Per-class augmentation multiplier (can be tuned by user). */
static float g_cls_boost[OUTPUT_SIZE];

static void reset_boosts(void)
{
    for (int c = 0; c < OUTPUT_SIZE; ++c)
        g_cls_boost[c] = 1.0f;
}

/* ============================================================
 *  DATA AUGMENTATION (LABEL-PRESERVING)
 * ============================================================ */

void augment_sample(float *dst, const float *src, int label,
                    unsigned *rng_state)
{
    memcpy(dst, src, sizeof(float) * H * W);

    if (label < 0 || label >= OUTPUT_SIZE)
        label = 0;

    if (g_cls_boost[0] == 0.f && g_cls_boost[1] == 0.f)
        reset_boosts();  /* lazy init */

    const int hard = is_hard(label);
    float mult = g_cls_boost[label];
    mult = NN_clampf(mult, 0.6f, 1.8f);

    float p_shift    = NN_clampf((hard ? 0.60f : 0.30f) * mult, 0.f, 0.90f);
    float p_thick    = NN_clampf((hard ? 0.55f : 0.25f) * mult, 0.f, 0.90f);
    float p_contrast = NN_clampf((hard ? 0.45f : 0.20f) * mult, 0.f, 0.90f);

    (void)p_contrast; /* reserved for later tweaks */

    float buf1[H * W], buf2[H * W];
    int cur = 0; /* 0 = dst, 1 = buf1, 2 = buf2 */

#define CURPTR()   ((cur==0)?dst:(cur==1?buf1:buf2))
#define NEXTBUF()  ((cur==0)?buf1:(cur==1?buf2:dst))
#define SWAPBUF()  do { cur = (cur + 1) % 3; } while (0)

    /* 1) Small random translation. */
    if (rng_f01(rng_state) < p_shift) {
        int dx = rng_int(rng_state, -2, 2);
        int dy = rng_int(rng_state, -2, 2);
        const float *srcp = CURPTR();
        float *dstp       = NEXTBUF();
        shift_copy(srcp, dstp, dx, dy);
        SWAPBUF();
    }

    /* 2) Global morpho: random thickening or thinning. */
    if (rng_f01(rng_state) < p_thick) {
        int do_dilate = (rng_f01(rng_state) < 0.5f);
        const float *srcp = CURPTR();
        float *dstp       = NEXTBUF();
        if (do_dilate) min3x3(srcp, dstp);
        else           max3x3(srcp, dstp);
        SWAPBUF();
    }

    /* 3) Cluster-specific tweaks. */
    if (cluster_vert(label)) {
        float p_vert = NN_clampf(0.55f * mult, 0.f, 0.90f);
        if (rng_f01(rng_state) < p_vert) {
            const float *srcp = CURPTR();
            float *dstp       = NEXTBUF();
            int rows = rng_int(rng_state, 1, 2);
            if (label == 8 /*I*/ || label == 19 /*T*/) {
                memcpy(dstp, srcp, sizeof(float) * H * W);
                trim_bottom_rows(dstp, rows);
            } else {
                thicken_bottom_band(srcp, dstp, rows);
            }
            SWAPBUF();
        }
    }

    if (cluster_round(label)) {
        float p_round = NN_clampf(0.55f * mult, 0.f, 0.90f);
        if (rng_f01(rng_state) < p_round) {
            const float *srcp = CURPTR();
            float *dstp       = NEXTBUF();
            int band = rng_int(rng_state, 1, 2);
            if (label == 14 /*O*/ || label == 2 /*C*/) {
                memcpy(dstp, srcp, sizeof(float) * H * W);
                lighten_right_band(dstp, band);
            } else {
                dilate_right_band(srcp, dstp, band);
            }
            SWAPBUF();
        }
    }

    if (cluster_bowl(label)) {
        float p_bowl = NN_clampf(0.45f * mult, 0.f, 0.90f);
        if (rng_f01(rng_state) < p_bowl) {
            const float *srcp = CURPTR();
            float *dstp       = NEXTBUF();
            dilate_right_band(srcp, dstp, 1);
            SWAPBUF();
        }
    }

    if (CURPTR() != dst)
        memcpy(dst, CURPTR(), sizeof(float) * H * W);

#undef CURPTR
#undef NEXTBUF
#undef SWAPBUF
}

/* ============================================================
 *  CSV LOADER: id,p0..p783,label
 * ============================================================ */

Dataset load_csv(const char *path)
{
    Dataset D = {0};
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("fopen");
        return D;
    }

    char line[20000];
    long pos = ftell(f);

    /* Optional header starting with "id". */
    if (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "id", 2) != 0)
            fseek(f, pos, SEEK_SET);
    }

    int n = 0, cap = 0;
    float *X = NULL;
    unsigned char *y = NULL;

    while (fgets(line, sizeof(line), f)) {
        if (n >= cap) {
            int nc = cap ? cap * 2 : 1024;
            X = (float*)realloc(X, sizeof(float) * nc * H * W);
            y = (unsigned char*)realloc(y, sizeof(unsigned char) * nc);
            if (!X || !y) {
                fprintf(stderr, "OOM\n");
                exit(1);
            }
            cap = nc;
        }

        char *tok = strtok(line, ","); /* id */
        if (!tok) continue;

        unsigned char u8[H * W];
        for (int i = 0; i < H * W; ++i) {
            tok = strtok(NULL, ",");
            if (!tok) goto next_line;
            long v = strtol(tok, NULL, 10);
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            u8[i] = (unsigned char)v;
        }

        tok = strtok(NULL, ",\r\n");
        if (!tok) goto next_line;
        int lab = parse_label26(tok);
        if (lab < 0) goto next_line;

        float *row = &X[n * H * W];

#if BINARIZE
        for (int i = 0; i < H * W; ++i) {
            int fg = (u8[i] >= THR);
            if (INVERT) fg = !fg;
            row[i] = fg ? 1.f : 0.f;
        }
#else
        for (int i = 0; i < H * W; ++i) {
            int v = u8[i];
            if (INVERT) v = 255 - v;
            row[i] = v / 255.f;
        }
#endif
        y[n] = (unsigned char)lab;
        n++;
        continue;

    next_line:
        ; /* skip invalid line */
    }

    fclose(f);
    D.n = n; D.X = X; D.y = y;
    fprintf(stderr, "CSV: %s -> %d samples\n", path, n);
    return D;
}

void free_dataset(Dataset *D)
{
    free(D->X);
    free(D->y);
    D->X = NULL;
    D->y = NULL;
    D->n = 0;
}

void shuffle_idx(int *idx, int n)
{
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
    }
}

/* ============================================================
 *  NETWORK INITIALIZATION
 * ============================================================ */

void init_network(Network *net)
{
    /* conv1: 1 channel -> C1_OUT */
    for (int oc = 0; oc < C1_OUT; ++oc) {
        he_init_conv_1ch(&net->Wc1[oc * K1 * K1], K1);
        net->bc1[oc] = 0.f;
    }

    /* conv2: C1_OUT -> C2_OUT */
    he_init_conv_ch(net->Wc2, C1_OUT, K2,
                    C2_OUT * C1_OUT * K2 * K2);
    for (int oc = 0; oc < C2_OUT; ++oc)
        net->bc2[oc] = 0.f;

    /* Fully connected: flatten C2_OUT x HO x WO -> OUTPUT_SIZE. */
    xavier_init(net->Wf, C2_OUT * HO * WO, OUTPUT_SIZE);
    for (int i = 0; i < OUTPUT_SIZE; ++i)
        net->bf[i] = 0.f;

    reset_boosts();
}

/* ============================================================
 *  FORWARD PASS (TRAINING PATH)
 * ============================================================ */

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
                y1[NN_I3(oc, y, x0, C1_OUT, H, W)] = (s > 0.f) ? s : 0.f;
            }
        }
    }
}

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

/* ============================================================
 *  BACKWARD + SGD UPDATE (WITH L2 + LABEL SMOOTHING)
 * ============================================================ */

float train_one(Network *net, const float *x01, int label, float lr)
{
    /* Forward. */
    float y1 [C1_OUT * H * W];
    float y1b[C2_OUT * H * W];
    float y2 [C2_OUT * HO * WO];
    float z  [OUTPUT_SIZE];

    conv1_forward(net, x01, y1);
    conv2_forward(net, y1, y1b);
    avgpool2x2_forward(y1b, C2_OUT, y2);
    fc_forward(net, y2, z);
    softmax(z, OUTPUT_SIZE);

    /* Label smoothing: on = 1-eps, off = eps/(K-1). */
    const float eps = 0.05f;
    const float on  = 1.f - eps;
    const float off = eps / (OUTPUT_SIZE - 1);

    float loss = 0.f;
    for (int i = 0; i < OUTPUT_SIZE; ++i) {
        float yi = (i == label) ? on : off;
        loss -= yi * logf(z[i] + 1e-12f);
    }

    /* dL/dz = softmax(z) - y_smooth. */
    float gz[OUTPUT_SIZE];
    for (int i = 0; i < OUTPUT_SIZE; ++i) {
        float yi = (i == label) ? on : off;
        gz[i] = z[i] - yi;
    }

    /* Fully connected gradients. */
    int F = C2_OUT * HO * WO;
    float gy2[F];
    memset(gy2, 0, sizeof(gy2));

    for (int i = 0; i < OUTPUT_SIZE; ++i) {
        float gi = gz[i];
        float *w = &net->Wf[i * F];

        for (int j = 0; j < F; ++j) {
            gy2[j] += gi * w[j];
            float g = y2[j] * gi + WD * w[j];
            g = NN_clampf(g, -3.f, 3.f);
            w[j] -= lr * g;
        }

        float gb = NN_clampf(gi, -3.f, 3.f);
        net->bf[i] -= lr * gb;
    }

    /* Backward through avgpool 2x2. */
    float gy1b[C2_OUT * H * W];
    memset(gy1b, 0, sizeof(gy1b));

    for (int c = 0; c < C2_OUT; ++c) {
        for (int y0 = 0; y0 < HO; ++y0) {
            for (int x0 = 0; x0 < WO; ++x0) {
                float g = gy2[NN_I3(c, y0, x0, C2_OUT, HO, WO)] * 0.25f;
                int yy = 2 * y0;
                int xx = 2 * x0;
                gy1b[NN_I3(c, yy,   xx,   C2_OUT, H, W)] += g;
                gy1b[NN_I3(c, yy,   xx+1, C2_OUT, H, W)] += g;
                gy1b[NN_I3(c, yy+1, xx,   C2_OUT, H, W)] += g;
                gy1b[NN_I3(c, yy+1, xx+1, C2_OUT, H, W)] += g;
            }
        }
    }

    /* Gate ReLU of conv2. */
    for (int i = 0; i < C2_OUT * H * W; ++i)
        if (y1b[i] <= 0.f) gy1b[i] = 0.f;

    /* Gradients for conv2 and backprop to y1. */
    float gy1[C1_OUT * H * W];
    memset(gy1, 0, sizeof(gy1));

    for (int oc = 0; oc < C2_OUT; ++oc) {
        /* Bias grad for conv2: average over space + clip. */
        double sb = 0.0;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                sb += gy1b[NN_I3(oc, y, x, C2_OUT, H, W)];
        float sb_avg = NN_clampf((float)(sb / (H * W)), -3.f, 3.f);
        net->bc2[oc] -= lr * sb_avg;

        /* Weights grad for conv2. */
        float *Foc = &net->Wc2[oc * (C1_OUT * K2 * K2)];
        for (int ic = 0; ic < C1_OUT; ++ic) {
            float *F2 = &Foc[ic * (K2 * K2)];
            for (int ky = 0; ky < K2; ++ky)
                for (int kx = 0; kx < K2; ++kx) {
                    double s = 0.0;
                    for (int y = 0; y < H; ++y) {
                        int yy = y + ky - PAD2;
                        if ((unsigned)yy >= (unsigned)H) continue;
                        for (int x = 0; x < W; ++x) {
                            int xx = x + kx - PAD2;
                            if ((unsigned)xx >= (unsigned)W) continue;
                            s += gy1b[NN_I3(oc, y, x, C2_OUT, H, W)] *
                                 y1  [NN_I3(ic, yy, xx, C1_OUT, H, W)];
                        }
                    }
                    float wv   = F2[ky * K2 + kx];
                    float grad = (float)(s / (H * W)) + WD * wv;
                    grad = NN_clampf(grad, -3.f, 3.f);
                    F2[ky * K2 + kx] -= lr * grad;
                }
        }

        /* Backprop to y1. */
        const float *Foc_ro = &net->Wc2[oc * (C1_OUT * K2 * K2)];
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                float go = gy1b[NN_I3(oc, y, x, C2_OUT, H, W)];
                if (go == 0.f) continue;
                for (int ic = 0; ic < C1_OUT; ++ic) {
                    const float *F2 = &Foc_ro[ic * (K2 * K2)];
                    for (int ky = 0; ky < K2; ++ky) {
                        int yy = y + ky - PAD2;
                        if ((unsigned)yy >= (unsigned)H) continue;
                        for (int kx = 0; kx < K2; ++kx) {
                            int xx = x + kx - PAD2;
                            if ((unsigned)xx >= (unsigned)W) continue;
                            gy1[NN_I3(ic, yy, xx, C1_OUT, H, W)] +=
                                go * F2[ky * K2 + kx];
                        }
                    }
                }
            }
        }
    }

    /* Gate ReLU of conv1. */
    for (int i = 0; i < C1_OUT * H * W; ++i)
        if (y1[i] <= 0.f) gy1[i] = 0.f;

    /* Gradients for conv1 (single input channel). */
    for (int oc = 0; oc < C1_OUT; ++oc) {
        /* Bias grad for conv1. */
        double sb = 0.0;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                sb += gy1[NN_I3(oc, y, x, C1_OUT, H, W)];
        float sb_avg = NN_clampf((float)(sb / (H * W)), -3.f, 3.f);
        net->bc1[oc] -= lr * sb_avg;

        /* Weights grad for conv1. */
        for (int ky = 0; ky < K1; ++ky)
            for (int kx = 0; kx < K1; ++kx) {
                double s = 0.0;
                for (int y = 0; y < H; ++y) {
                    int yy = y + ky - PAD1;
                    if ((unsigned)yy >= (unsigned)H) continue;
                    for (int x = 0; x < W; ++x) {
                        int xx = x + kx - PAD1;
                        if ((unsigned)xx >= (unsigned)W) continue;
                        s += gy1[NN_I3(oc, y, x, C1_OUT, H, W)] *
                             x01[yy * W + xx];
                    }
                }
                int wi   = oc * K1 * K1 + ky * K1 + kx;
                float wv = net->Wc1[wi];
                float grad = (float)(s / (H * W)) + WD * wv;
                grad = NN_clampf(grad, -3.f, 3.f);
                net->Wc1[wi] -= lr * grad;
            }
    }

    return loss;
}
