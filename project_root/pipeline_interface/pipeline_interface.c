#include "pipeline_interface.h"

#include "../draw_outline/draw_outline.h"         // draw_outline / rectangle
#include "../letter_extractor/letter_extractor.h" // extract_letters
#include "../neural_network/digitalisation.h"     // (if needed by nn)
#include "../neural_network/nn.h"                 // Network, smart_predict_k
#include "../solver/solver.h" // CellCand, resolution, resolution_prob
#include "../structure_detection/structure_detection.h" // grid/list detection

#include <SDL2/SDL_image.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------- Global config (local to pipeline) --------------------
 */
#define ACCEPT_P1_THR                                                          \
  0.90f // if top1 prob >= this, we accept directly (if margin OK)
#define ACCEPT_MARGIN 0.25f // default required margin p1 - p2
#define HARD_MARGIN 0.35f   // stronger margin for confusing letter families

// Letter prior: we downweight 'W' so it appears less often (index 22).
static const float LETTER_PRIOR[26] = {
    /* A  B  C  D  E  F  G  H  I  J  K  L  M */
    1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f,
    /* N  O  P  Q  R  S  T  U  V  W    X  Y  Z */
    1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 0.35f, 1.f, 1.f, 1.f};

/* -------------------- Small local structs -------------------- */
typedef struct {
  int x, y, w, h;
} Box; // simple integer rectangle

/* Logical representation of the LIST area */
typedef struct {
  int n_lines;     // number of text lines detected
  int *n_words;    // [line]    number of words per line
  int **n_chars;   // [line][w] number of characters per word
  Uint8 ****tiles; // tiles[line][word][char] -> 784-byte (28x28) buffers
} WordMatrix;

/* -------------------- Debug: dump 28x28 buffer to BMP -------------------- */
static int save_buf784_bmp(const Uint8 *buf784, const char *path) {
  if (!buf784 || !path)
    return -1; // invalid parameters

  SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(
      0, 28, 28, 32, SDL_PIXELFORMAT_ARGB8888); // 28x28 ARGB surface
  if (!surf)
    return -2;
  if (SDL_LockSurface(surf) != 0) {
    SDL_FreeSurface(surf);
    return -3;
  }

  Uint32 *px = (Uint32 *)surf->pixels; // pixel buffer (ARGB8888)
  int pitch = surf->pitch / 4;         // pitch in pixels (not bytes)
  for (int y = 0; y < 28; ++y) {
    for (int x = 0; x < 28; ++x) {
      Uint8 v = buf784[y * 28 + x];                       // grayscale 0..255
      Uint32 p = (255u << 24) | (v << 16) | (v << 8) | v; // ARGB: opaque gray
      px[y * pitch + x] = p;
    }
  }

  SDL_UnlockSurface(surf);
  if (SDL_SaveBMP(surf, path) != 0) {
    SDL_FreeSurface(surf);
    return -4;
  }
  SDL_FreeSurface(surf);
  return 0;
}

/* -------------------- LIST utils: binarize + segment + resize 28
 * -------------------- */
static inline Uint8 luminance(Uint32 px, SDL_PixelFormat *fmt) {
  Uint8 r, g, b;
  SDL_GetRGB(px, fmt, &r, &g, &b);          // extract RGB channels
  int y = (30 * r + 59 * g + 11 * b) / 100; // standard luma approximation
  return (Uint8)y;
}

static int otsu_from_hist(const int hist[256], int total) {
  if (total <= 0)
    return 128; // degenerate case
  double sum = 0.0;
  for (int t = 0; t < 256; ++t)
    sum += (double)t * hist[t]; // sum of intensities

  double sumB = 0.0, maxVar = -1.0;
  int wB = 0, bestT = 128;
  for (int t = 0; t < 256; ++t) {
    wB += hist[t];
    if (wB == 0)
      continue; // background weight
    int wF = total - wB;
    if (wF == 0)
      break; // foreground weight
    sumB += (double)t * hist[t];
    double mB = sumB / wB;         // background mean
    double mF = (sum - sumB) / wF; // foreground mean
    double diff = mB - mF;
    double varBetween =
        (double)wB * (double)wF * diff * diff; // between-class variance
    if (varBetween > maxVar) {
      maxVar = varBetween;
      bestT = t;
    }
  }
  return bestT; // chosen threshold
}

static void binarize_roi(SDL_Surface *s, SDL_Rect roi, Uint8 *bin) {
  SDL_LockSurface(s);       // lock for safe pixel access
  int W = roi.w, H = roi.h; // ROI width/height

  Uint8 *G = (Uint8 *)malloc((size_t)W * (size_t)H); // grayscale buffer
  if (!G) {
    SDL_UnlockSurface(s);
    return;
  }

  int hist[256] = {0}; // grayscale histogram
  for (int y = 0; y < H; ++y) {
    Uint8 *p = (Uint8 *)s->pixels + (roi.y + y) * s->pitch +
               roi.x * 4; // pointer to row start
    for (int x = 0; x < W; ++x) {
      Uint32 px = *(Uint32 *)(p + 4 * x); // ARGB pixel
      Uint8 Y = luminance(px, s->format); // grayscale 0..255
      G[y * W + x] = Y;
      hist[Y]++; // build histogram
    }
  }
  SDL_UnlockSurface(s);

  int total = W * H;                   // number of pixels in ROI
  int T = otsu_from_hist(hist, total); // global Otsu threshold
  int BLACK_THR = T;                   // starting threshold
  if (BLACK_THR < 20)
    BLACK_THR = 20; // clamp very low values
  if (BLACK_THR > 235)
    BLACK_THR = 235; // clamp very high values

  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      Uint8 Y = G[y * W + x];
      bin[y * W + x] =
          (Y < BLACK_THR) ? 0 : 255; // 0 = black (ink), 255 = white
    }

  free(G);
}

static void horiz_proj(const Uint8 *bin, int w, int h, int *hp) {
  for (int y = 0; y < h; ++y) {
    int s = 0;
    const Uint8 *row = bin + y * w; // row pointer
    for (int x = 0; x < w; ++x)
      s += (row[x] == 0); // count black pixels
    hp[y] = s;            // horizontal projection
  }
}

static void find_runs_over(const int *arr, int n, int thr, int minw, Box *out,
                           int *nout) {
  int o = 0, i = 0;
  while (i < n) {
    while (i < n && arr[i] <= thr)
      i++; // skip below threshold
    if (i >= n)
      break;
    int a = i;
    while (i < n && arr[i] > thr)
      i++; // run of values > thr
    int b = i - 1;
    if (b - a + 1 >= minw) // keep only wide enough runs
      out[o++] = (Box){.x = a, .y = 0, .w = b - a + 1, .h = 1};
  }
  *nout = o; // number of detected runs
}

static Uint8 *crop_resize_28(const Uint8 *bin, int W, int H, Box bb) {
  const int OUT = 28, PAD = 2, INNER = OUT - 2 * PAD; // 28 with padding around
  Uint8 *out = (Uint8 *)malloc(OUT * OUT); // 28x28 destination buffer
  if (!out)
    return NULL;
  for (int i = 0; i < OUT * OUT; ++i)
    out[i] = 255; // start with white background

  int cw = bb.w, ch = bb.h; // bounding box size
  if (cw < 1 || ch < 1)
    return out; // degenerate: return blank tile

  float sx = (float)INNER / (float)cw; // scale in x
  float sy = (float)INNER / (float)ch; // scale in y
  float s = (sx < sy) ? sx : sy;       // uniform scale (keep aspect)

  int tw = (int)(cw * s + 0.5f);     // target width
  int th = (int)(ch * s + 0.5f);     // target height
  int offx = PAD + (INNER - tw) / 2; // center inside INNER region
  int offy = PAD + (INNER - th) / 2;

  for (int yy = 0; yy < th; ++yy) {
    int syy = bb.y + (int)(yy / s + 0.5f); // source y (nearest neighbor)
    if (syy < 0)
      syy = 0;
    else if (syy >= H)
      syy = H - 1;
    for (int xx = 0; xx < tw; ++xx) {
      int sxx = bb.x + (int)(xx / s + 0.5f); // source x (nearest neighbor)
      if (sxx < 0)
        sxx = 0;
      else if (sxx >= W)
        sxx = W - 1;
      out[(offy + yy) * OUT + (offx + xx)] = bin[syy * W + sxx]; // copy pixel
    }
  }
  return out;
}

/* -------------------- Memory helpers -------------------- */
static void free_out_matrix(Uint8 ***Mat, int N, int M) {
  if (!Mat)
    return;
  for (int i = 0; i < N; ++i) {
    if (!Mat[i])
      continue;
    for (int j = 0; j < M; ++j)
      free(Mat[i][j]); // free each 28x28 tile
    free(Mat[i]);      // free row of pointers
  }
  free(Mat); // free row array
}

static void free_word_matrix(WordMatrix *WM) {
  if (!WM)
    return;
  for (int L = 0; L < WM->n_lines; ++L) {
    for (int Wd = 0; Wd < WM->n_words[L]; ++Wd) {
      for (int C = 0; C < WM->n_chars[L][Wd]; ++C)
        free(WM->tiles[L][Wd][C]); // free each character tile
      free(WM->tiles[L][Wd]);      // free characters of word
    }
    free(WM->tiles[L]);   // free words in that line
    free(WM->n_chars[L]); // free char counts for line
  }
  free(WM->tiles);            // free all tiles pointers
  free(WM->n_chars);          // free char count per word
  free(WM->n_words);          // free word count per line
  memset(WM, 0, sizeof(*WM)); // reset structure
}

/* -------------------- LIST extraction: connected components
 * -------------------- */
#define MAX_CHARS_PER_LINE 1024 // safety limit to avoid insane lines

static int cmp_box_x(const void *a, const void *b) {
  const Box *A = (const Box *)a, *B = (const Box *)b;
  return (A->x > B->x) - (A->x < B->x); // sort by x (left to right)
}

static int cmp_int(const void *a, const void *b) {
  int A = *(const int *)a, B = *(const int *)b;
  return (A > B) - (A < B); // ascending integer compare
}

static int extract_words(SDL_Surface *surf, SDL_Rect list, WordMatrix *WM) {
  memset(WM, 0, sizeof(*WM)); // reset output structure

  int W = list.w, H = list.h; // LIST ROI size
  if (W <= 0 || H <= 0)
    return 0; // nothing to do

  Uint8 *bin =
      (Uint8 *)malloc((size_t)W * (size_t)H); // binary image of the list
  if (!bin)
    return -1;
  binarize_roi(surf, list, bin); // threshold list region

  int *hp = (int *)malloc(sizeof(int) * H); // horizontal projection
  if (!hp) {
    free(bin);
    return -2;
  }
  horiz_proj(bin, W, H, hp); // count strokes per row

  Box lineRuns[1024]; // potential text lines
  int nL = 0;
  find_runs_over(hp, H, (int)(0.02 * W), 4, lineRuns,
                 &nL); // rows with enough black pixels
  free(hp);

  if (nL <= 0) {
    free(bin);
    return 0;
  } // no lines detected

  WM->n_lines = nL;
  WM->n_words = (int *)calloc((size_t)nL, sizeof(int)); // words per line
  WM->n_chars =
      (int **)calloc((size_t)nL, sizeof(int *)); // chars per word per line
  WM->tiles = (Uint8 ****)calloc((size_t)nL,
                                 sizeof(Uint8 ***)); // tiles per word per line
  if (!WM->n_words || !WM->n_chars || !WM->tiles) {
    free(WM->n_words);
    free(WM->n_chars);
    free(WM->tiles);
    memset(WM, 0, sizeof(*WM));
    free(bin);
    return -3;
  }

  for (int L = 0; L < nL; ++L) {
    int y0 = lineRuns[L].x; // starting row of line
    int hL = lineRuns[L].w; // height of line in rows
    if (hL < 4)
      continue; // skip too small lines

    int lineSize = W * hL;
    unsigned char *vis =
        (unsigned char *)calloc((size_t)lineSize, 1); // visited mask
    int *qx = (int *)malloc(sizeof(int) * lineSize);  // BFS queue x
    int *qy = (int *)malloc(sizeof(int) * lineSize);  // BFS queue y
    if (!vis || !qx || !qy) {
      free(vis);
      free(qx);
      free(qy);
      continue;
    }

    Box charBoxes[MAX_CHARS_PER_LINE]; // per-character bounding boxes
    int nC = 0;
    const int dx4[4] = {1, -1, 0, 0}; // 4-neighbourhood (4-connectivity)
    const int dy4[4] = {0, 0, 1, -1};

    for (int yy = 0; yy < hL; ++yy) {
      for (int xx = 0; xx < W; ++xx) {
        int idx = yy * W + xx; // index inside line window
        if (bin[(y0 + yy) * W + xx] == 0 &&
            !vis[idx]) { // black pixel not yet visited
          if (nC >= MAX_CHARS_PER_LINE)
            break; // avoid overflow
          int front = 0, back = 0;
          vis[idx] = 1; // mark first pixel as visited
          qx[back] = xx;
          qy[back] = yy;
          back++; // enqueue

          int minx = xx, maxx = xx; // bounding box for this component
          int miny_rel = yy, maxy_rel = yy;

          while (front < back) { // BFS over connected black pixels
            int cx = qx[front], cy = qy[front];
            front++;
            for (int k = 0; k < 4; ++k) {
              int nx = cx + dx4[k], ny = cy + dy4[k];
              if (nx < 0 || nx >= W || ny < 0 || ny >= hL)
                continue;
              int nidx = ny * W + nx;
              if (!vis[nidx] && bin[(y0 + ny) * W + nx] == 0) {
                vis[nidx] = 1;
                qx[back] = nx;
                qy[back] = ny;
                back++; // add neighbour to queue
                if (nx < minx)
                  minx = nx;
                if (nx > maxx)
                  maxx = nx;
                if (ny < miny_rel)
                  miny_rel = ny;
                if (ny > maxy_rel)
                  maxy_rel = ny;
              }
            }
          }

          int bb_w = maxx - minx + 1;
          int bb_h = maxy_rel - miny_rel + 1;
          int area = bb_w * bb_h;
          if (area < 15 || bb_h < hL * 0.3f)
            continue; // reject tiny/noise components

          int expand = 1; // small padding
          int left = minx - expand;
          int right = maxx + expand;
          int topRel = miny_rel - expand;
          int botRel = maxy_rel + expand;
          if (left < 0)
            left = 0;
          if (right >= W)
            right = W - 1;
          if (topRel < 0)
            topRel = 0;
          if (botRel >= hL)
            botRel = hL - 1;

          Box bb;
          bb.x = left;        // x in list-local coords
          bb.y = y0 + topRel; // y in full list coords
          bb.w = right - left + 1;
          bb.h = botRel - topRel + 1;
          charBoxes[nC++] = bb; // store character bounding box
        }
      }
      if (nC >= MAX_CHARS_PER_LINE)
        break; // safety break
    }

    free(vis);
    free(qx);
    free(qy);
    if (nC <= 0)
      continue; // no characters on this line

    qsort(charBoxes, (size_t)nC, sizeof(Box),
          cmp_box_x); // sort characters left→right

    int nGaps = (nC > 1) ? (nC - 1) : 0;
    int *gaps = (nGaps > 0) ? (int *)malloc(sizeof(int) * nGaps) : NULL;
    if (gaps) {
      for (int i = 0; i < nGaps; ++i) {
        int leftEnd = charBoxes[i].x + charBoxes[i].w; // right side of char i
        int gap = charBoxes[i + 1].x - leftEnd; // horizontal gap to next char
        if (gap < 0)
          gap = 0;
        gaps[i] = gap;
      }
      qsort(gaps, (size_t)nGaps, sizeof(int),
            cmp_int); // sort gaps to estimate spacing
    }

    int thrGap;
    if (!gaps || nGaps == 0) {
      thrGap = 1000000; // no gap info → no splits
    } else {
      int median = gaps[nGaps / 2]; // median gap
      thrGap =
          (median <= 0) ? 4 : (median * 2 + 2); // gaps much larger ⇒ new word
    }

    int nW = 1; // at least one word
    for (int i = 0; i < nGaps; ++i) {
      int leftEnd = charBoxes[i].x + charBoxes[i].w;
      int gap = charBoxes[i + 1].x - leftEnd;
      if (gap < 0)
        gap = 0;
      if (gap > thrGap)
        nW++; // large gap → new word
    }

    WM->n_words[L] = nW; // store number of words
    WM->n_chars[L] =
        (int *)calloc((size_t)nW, sizeof(int)); // char counts per word
    WM->tiles[L] =
        (Uint8 ***)calloc((size_t)nW, sizeof(Uint8 **)); // tiles per word
    if (!WM->n_chars[L] || !WM->tiles[L]) {
      free(WM->n_chars[L]);
      free(WM->tiles[L]);
      WM->n_chars[L] = NULL;
      WM->tiles[L] = NULL;
      WM->n_words[L] = 0;
      if (gaps)
        free(gaps);
      continue;
    }

    int wIdx = 0, count = 1; // build char count per word
    for (int i = 0; i < nGaps; ++i) {
      int leftEnd = charBoxes[i].x + charBoxes[i].w;
      int gap = charBoxes[i + 1].x - leftEnd;
      if (gap < 0)
        gap = 0;
      if (gap > thrGap) {
        WM->n_chars[L][wIdx] = count; // close current word
        wIdx++;
        count = 1; // start new word
      } else
        count++;
    }
    WM->n_chars[L][wIdx] = count; // last word count

    for (int w = 0; w < nW; ++w)
      WM->tiles[L][w] = (Uint8 **)calloc(
          (size_t)WM->n_chars[L][w], sizeof(Uint8 *)); // allocate char slots

    wIdx = 0;
    int cInWord = 0;
    for (int i = 0; i < nC; ++i) {
      Box bb = charBoxes[i];
      Uint8 *tile =
          crop_resize_28(bin, W, H, bb);  // crop + resize character to 28x28
      WM->tiles[L][wIdx][cInWord] = tile; // store tile
      cInWord++;

      if (i < nGaps) {
        int leftEnd = charBoxes[i].x + charBoxes[i].w;
        int gap = charBoxes[i + 1].x - leftEnd;
        if (gap < 0)
          gap = 0;
        if (gap > thrGap) {
          wIdx++;
          cInWord = 0;
        } // start next word
      }
    }
    if (gaps)
      free(gaps);
  }

  free(bin);
  return 0;
}

/* -------------------- Local top-k decision (still in pipeline)
 * -------------------- */
static int accept_or_candidates(const int *idx, const float *prob, int k,
                                int *accepted, Candidate *cand, int *ncand) {
  float p1 = prob[0];                  // probability of top1 class
  float p2 = (k >= 2) ? prob[1] : 0.f; // probability of top2 class (if any)
  int a = idx[0];                      // top1 class index

  int vert = (a == 8 || a == 10 || a == 11 || a == 19 ||
              a == 5); // letters with vertical strokes: I,K,L,T,F
  int round =
      (a == 14 || a == 3 || a == 16 || a == 2); // round letters: O,D,Q,C
  int bowl = (a == 15 || a == 1 || a == 17);    // bowl-type: P,B,R
  float margin = (vert || round || bowl)
                     ? HARD_MARGIN
                     : ACCEPT_MARGIN; // stricter margin for ambiguous shapes

  if (p1 >= ACCEPT_P1_THR && (p1 - p2) >= margin) {
    *accepted = a;
    *ncand = 0;
    return 1; // one clean winner, no candidate list
  }

  float s = 0.f;
  for (int i = 0; i < k; ++i)
    s += prob[i]; // sum probabilities
  if (s < 1e-12f)
    s = 1e-12f;
  for (int i = 0; i < k; ++i) {
    cand[i].cls = idx[i];         // store class index
    cand[i].weight = prob[i] / s; // normalized weight
  }
  *accepted = -1;
  *ncand = k; // we return candidates
  return 0;
}

/* -------------------- OCR: 28x28 tile → top-k -------------------- */
static int ocr_tile_topk(Network *net, const Uint8 *buf784, int k, int *idx,
                         float *logp, float *prob) {
  float x[784], mean = 0.f;
  for (int i = 0; i < 784; ++i) {
    x[i] = (float)buf784[i] / 255.f; // normalize 0..255 → 0..1
    mean += x[i];
  }
  mean /= 784.f; // average intensity (for inversion check)

  if (mean < 0.5f) // if mostly dark → probably inverted
    for (int i = 0; i < 784; ++i)
      x[i] = 1.f - x[i]; // invert so background is bright

  return smart_predict_k(net, x, k, idx, logp, prob); // CNN forward + top-k
}

/* -------------------- Export grid + words to text file -------------------- */
static int save_export_file(const char *path, int rows, int cols, char **grid,
                            char **words, int n_words) {
  FILE *f = fopen(path, "w");
  if (!f) {
    perror("fopen export");
    return -1;
  }

  fprintf(f, "%d %d\n", rows, cols); // first line: grid size
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      char c = grid[i][j];
      if (c < 'A' || c > 'Z')
        c = '?'; // sanitize invalid chars
      fprintf(f, "%c", c);
      if (j + 1 < cols)
        fprintf(f, " "); // space-separated letters
    }
    fprintf(f, "\n");
  }
  for (int w = 0; w < n_words; ++w)
    fprintf(f, "%s\n", words[w] ? words[w] : ""); // one word per line

  fclose(f);
  return 0;
}

/* -------------------- Main pipeline -------------------- */
SDL_Surface *pipeline(SDL_Surface *surface, SDL_Renderer *render) {
  if (!surface || !render)
    return surface; // safety guard

  SDL_Rect grid = {0, 0, 0, 0},
           list = {0, 0, 0, 0}; // bounding boxes for grid & list
  if (detect_grid_and_list(surface, &grid, &list) == 0) {
    printf("GRID:  (%d,%d) -> %dx%d\n", grid.x, grid.y, grid.w, grid.h);
    printf("LIST:  (%d,%d) -> %dx%d\n", list.x, list.y, list.w, list.h);
  } else {
    fprintf(stderr, "detect_grid_and_list: failed\n");
    return surface;
  }

  Uint8 ***out_matrix = NULL; // [rows][cols] → 28x28 tile
  int out_N = 0, out_M = 0;   // grid size (rows, cols)
  int rc = extract_letters(surface, grid.x, grid.y, grid.x + grid.w - 1,
                           grid.y + grid.h - 1, &out_matrix, &out_N,
                           &out_M); // segmentation of grid
  if (rc != 0 || !out_matrix || out_N <= 0 || out_M <= 0) {
    fprintf(stderr, "extract_letters failed rc=%d\n", rc);
    return surface;
  }
  printf("Extracted %d rows and %d columns of letters.\n", out_N, out_M);

  Network net;                              // CNN weights
  init_network(&net);                       // initialize / zero / boosters
  if (load_model("model.bin", &net) != 0) { // load trained model
    fprintf(stderr, "load_model: failed\n");
    free_out_matrix(out_matrix, out_N, out_M);
    return surface;
  }

  CellCand *cells = (CellCand *)calloc((size_t)out_N * out_M,
                                       sizeof(CellCand)); // solver cells
  char **grid_mat =
      (char **)malloc((size_t)out_N * sizeof(char *)); // char grid
  if (!cells || !grid_mat) {
    fprintf(stderr, "OOM cells/grid_mat\n");
    free_out_matrix(out_matrix, out_N, out_M);
    free(cells);
    free(grid_mat);
    return surface;
  }
  for (int i = 0; i < out_N; ++i) {
    grid_mat[i] =
        (char *)malloc((size_t)out_M * sizeof(char)); // one row of chars
    if (!grid_mat[i]) {
      for (int t = 0; t < i; ++t)
        free(grid_mat[t]);
      free(grid_mat);
      free_out_matrix(out_matrix, out_N, out_M);
      free(cells);
      return surface;
    }
  }

  for (int i = 0; i < out_N; ++i) {
    for (int j = 0; j < out_M; ++j) {
      Uint8 *buf = out_matrix[i][j]; // 28x28 image for this cell
      if (!buf) {
        grid_mat[i][j] = '?';       // unknown cell
        cells[i * out_M + j].n = 0; // no candidates
        continue;
      }

      int idx[KTOP];                // top-k class indices
      float logp[KTOP], prob[KTOP]; // log-probs and probs
      int k = ocr_tile_topk(&net, buf, KTOP, idx, logp, prob);
      if (k < 1) {
        grid_mat[i][j] = '?';
        cells[i * out_M + j].n = 0;
        continue;
      }

      for (int t = 0; t < k; ++t) {
        int cls = idx[t];
        if (cls >= 0 && cls < 26)
          prob[t] *= LETTER_PRIOR[cls]; // apply class prior
      }

      for (int a = 0; a < k - 1; ++a) // re-sort by prob after prior
        for (int b = a + 1; b < k; ++b)
          if (prob[b] > prob[a]) {
            float tf = prob[a];
            prob[a] = prob[b];
            prob[b] = tf;
            float tl = logp[a];
            logp[a] = logp[b];
            logp[b] = tl;
            int ti = idx[a];
            idx[a] = idx[b];
            idx[b] = ti;
          }

      float s = 0.f;
      for (int t = 0; t < k; ++t)
        s += prob[t]; // renormalize probabilities
      if (s < 1e-12f)
        s = 1e-12f;
      for (int t = 0; t < k; ++t) {
        prob[t] /= s;
        logp[t] = logf(prob[t] + 1e-12f);
      }

      int accepted, nc = 0;
      Candidate tmp[KTOP];
      if (accept_or_candidates(idx, prob, k, &accepted, tmp, &nc)) {
        grid_mat[i][j] = (char)('A' + accepted); // direct decision
        cells[i * out_M + j].n = 1;
        cells[i * out_M + j].cls[0] = (unsigned char)accepted;
        cells[i * out_M + j].weight[0] = 1.f; // weight 1: strong belief
      } else {
        cells[i * out_M + j].n = k; // keep multiple candidates
        for (int t = 0; t < k; ++t) {
          cells[i * out_M + j].cls[t] = (unsigned char)idx[t];
          cells[i * out_M + j].weight[t] = prob[t];
        }
        grid_mat[i][j] = (char)('A' + idx[0]); // still display top1 in grid
      }
    }
  }

  printf("\n===== GRID OCR (top1, after prior) =====\n");
  for (int i = 0; i < out_N; ++i) {
    for (int j = 0; j < out_M; ++j)
      printf("%c ", grid_mat[i][j]);
    printf("\n");
  }
  printf("========================================\n");

  WordMatrix WM = (WordMatrix){0}; // list of text words from right area
  int words_cap = 64, words_cnt = 0;
  char **words = (char **)calloc((size_t)words_cap,
                                 sizeof(char *)); // dynamic array of words

  if (list.w > 0 && list.h > 0 && words) { // only if list area exists
    if (extract_words(surface, list, &WM) == 0) {
      printf("LIST: %d lines\n", WM.n_lines);
      for (int L = 0; L < WM.n_lines; ++L) {
        for (int Wd = 0; Wd < WM.n_words[L]; ++Wd) {
          int nC = WM.n_chars[L][Wd]; // number of chars in this word
          if (nC <= 0)
            continue;

          if (words_cnt >= words_cap) { // grow buffer if needed
            words_cap *= 2;
            char **tmp = (char **)realloc(words, sizeof(char *) * words_cap);
            if (!tmp) {
              fprintf(stderr, "realloc words OOM\n");
              break;
            }
            words = tmp;
          }

          words[words_cnt] =
              (char *)malloc((size_t)nC + 1); // allocate word string
          for (int C = 0; C < nC; ++C) {
            Uint8 *buf = WM.tiles[L][Wd][C]; // 28x28 tile for this character
            if (!buf) {
              words[words_cnt][C] = '?';
              continue;
            }
            int idx[KTOP];
            float logp[KTOP], prob[KTOP];
            int kk = ocr_tile_topk(&net, buf, KTOP, idx, logp, prob);
            (void)kk; // we only use the top1
            words[words_cnt][C] =
                (char)('A' + idx[0]); // best guess for this letter
          }
          words[words_cnt][nC] = '\0'; // null-terminate word
          printf("WORD[%d,%d]: %s\n", L, Wd, words[words_cnt]);
          words_cnt++;
        }
      }
    }
  }

  SDL_SetRenderDrawColor(render, 0, 255, 0, 255); // green rectangle for grid
  rectangle(render, grid.x, grid.y, grid.x + grid.w, grid.y + grid.h, 4, 2);
  SDL_SetRenderDrawColor(render, 0, 128, 255, 255); // blue rectangle for list
  rectangle(render, list.x, list.y, list.x + list.w, list.y + list.h, 4, 2);

  if (save_export_file("grid", out_N, out_M, grid_mat, words, words_cnt) == 0)
    printf("Export written to file 'grid'\n");

  double stepX =
      (out_M > 0) ? ((double)grid.w / (double)out_M) : 0.0; // width per cell
  double stepY =
      (out_N > 0) ? ((double)grid.h / (double)out_N) : 0.0; // height per cell
  double base =
      (stepX < stepY) ? stepX : stepY; // use smallest dimension as reference

  int outline_width =
      (int)(0.90 * base); // approximate visual width of word box
  if (outline_width < 1)
    outline_width = 1;
  int outline_stroke = 2; // thickness of outline

  // ====== PREPARE TO SAVE WORD POSITIONS ======
  typedef struct {
    int x1, y1, x2, y2;
  } WordPos;
  WordPos *word_positions = NULL;
  int saved_words_count = 0;

  if (words_cnt > 0) {
    word_positions = (WordPos *)malloc(sizeof(WordPos) * words_cnt);
    if (word_positions) {
      for (int wi = 0; wi < words_cnt; ++wi) {
        word_positions[wi].x1 = -1; // Mark as not found initially
      }
    }
  }

  SDL_SetRenderDrawColor(render, 255, 0, 0,
                         255); // red outlines for found words

  for (int wi = 0; wi < words_cnt; ++wi) {
    const char *Wstr = words[wi]; // current word string
    if (!Wstr || !*Wstr)
      continue;

    int L = (int)strlen(Wstr);
    if (L <= 0)
      continue;

    int has_non_letter = 0;
    for (int k = 0; k < L; ++k) {
      char c = Wstr[k];
      if (c < 'A' || c > 'Z') {
        has_non_letter = 1;
        break;
      } // skip weird words
    }

    int out[4];
    float sc = 0.f;
    if (!has_non_letter)
      resolution_prob(cells, grid_mat, out_N, out_M, Wstr, out,
                      &sc); // probabilistic matching
    else
      out[0] = -1;

    int x1 = -1, y1 = -1, x2 = -1, y2 = -1; // Word pixel coordinates

    if (out[0] == -1) { // fallback to exact search if prob fails
      int out2[4];
      resolution(grid_mat, out_N, out_M, Wstr,
                 out2); // exact matching without probabilities
      if (out2[0] == -1) {
        printf("Not found: %s\n", Wstr); // word not found anywhere
        continue;
      }

      int c0 = out2[0], r0 = out2[1]; // start cell (col,row)
      int c1 = out2[2], r1 = out2[3]; // end cell (col,row)

      double x0L = floor((double)c0 * stepX);
      double x0R = floor((double)(c0 + 1) * stepX) - 1.0;
      double y0T = floor((double)r0 * stepY);
      double y0B = floor((double)(r0 + 1) * stepY) - 1.0;

      double x1L = floor((double)c1 * stepX);
      double x1R = floor((double)(c1 + 1) * stepX) - 1.0;
      double y1T = floor((double)r1 * stepY);
      double y1B = floor((double)(r1 + 1) * stepY) - 1.0;

      double cx0 = 0.5 * (x0L + x0R); // center of first cell
      double cy0 = 0.5 * (y0T + y0B);
      double cx1 = 0.5 * (x1L + x1R); // center of last cell
      double cy1 = 0.5 * (y1T + y1B);

      x1 = grid.x + (int)lround(cx0); // translate to image coordinates
      y1 = grid.y + (int)lround(cy0);
      x2 = grid.x + (int)lround(cx1);
      y2 = grid.y + (int)lround(cy1);

      draw_outline(render, x1, y1, x2, y2, outline_width,
                   outline_stroke); // draw path rectangle
      printf("Found exact (fallback): %s  (%d,%d)->(%d,%d)\n", Wstr, out2[0],
             out2[1], out2[2], out2[3]);
    } else {
      int c0 = out[0], r0 = out[1]; // best match start cell
      int c1 = out[2], r1 = out[3]; // best match end cell

      double x0L = floor((double)c0 * stepX);
      double x0R = floor((double)(c0 + 1) * stepX) - 1.0;
      double y0T = floor((double)r0 * stepY);
      double y0B = floor((double)(r0 + 1) * stepY) - 1.0;

      double x1L = floor((double)c1 * stepX);
      double x1R = floor((double)(c1 + 1) * stepX) - 1.0;
      double y1T = floor((double)r1 * stepY);
      double y1B = floor((double)(r1 + 1) * stepY) - 1.0;

      double cx0 = 0.5 * (x0L + x0R); // center of first cell
      double cy0 = 0.5 * (y0T + y0B);
      double cx1 = 0.5 * (x1L + x1R); // center of last cell
      double cy1 = 0.5 * (y1T + y1B);

      x1 = grid.x + (int)lround(cx0); // translate to global coordinates
      y1 = grid.y + (int)lround(cy0);
      x2 = grid.x + (int)lround(cx1);
      y2 = grid.y + (int)lround(cy1);

      draw_outline(render, x1, y1, x2, y2, outline_width,
                   outline_stroke); // highlight matched word

      float mean_log = sc / (float)L; // average log-score per letter
      printf("Found prob (matches/prefix first): %s  (%d,%d)->(%d,%d)  "
             "score=%.3f  mean=%.3f\n",
             Wstr, out[0], out[1], out[2], out[3], sc, mean_log);
    }

    // Save word position for redrawing in result.png
    if (word_positions && x1 != -1) {
      word_positions[saved_words_count].x1 = x1;
      word_positions[saved_words_count].y1 = y1;
      word_positions[saved_words_count].x2 = x2;
      word_positions[saved_words_count].y2 = y2;
      saved_words_count++;
    }
  }

  if (out_matrix && out_matrix[0] && out_matrix[0][0]) {
    Uint8 *buf = out_matrix[0][0]; // take first tile as sample
    for (int y = 0; y < 28; ++y)
      for (int x = 0; x < 28; ++x)
        if (!(x > 3 && x < 24 && y > 3 && y < 24)) // leave inner region intact
          buf[y * 28 + x] = 255; // make border white (debug framing)
    if (save_buf784_bmp(buf, "tile_debug.bmp") == 0)
      printf("Tile saved: tile_debug.bmp\n");
  }

  free_word_matrix(&WM); // free list (right side) data
  if (words) {
    for (int i = 0; i < words_cnt; ++i)
      free(words[i]);
    free(words);
  }
  free_out_matrix(out_matrix, out_N, out_M); // free grid tiles
  free(cells);                               // free solver cells
  for (int i = 0; i < out_N; ++i)
    free(grid_mat[i]); // free grid rows
  free(grid_mat);

  // ====== SAVE ANNOTATED IMAGE TO result.png ======
  // Create a temporary window and renderer with the same size as the surface
  SDL_Window *temp_window =
      SDL_CreateWindow("temp", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                       surface->w, surface->h, SDL_WINDOW_HIDDEN);
  if (temp_window) {
    SDL_Renderer *temp_renderer =
        SDL_CreateRenderer(temp_window, -1, SDL_RENDERER_SOFTWARE);
    if (temp_renderer) {
      // Create texture from the original surface
      SDL_Texture *temp_texture =
          SDL_CreateTextureFromSurface(temp_renderer, surface);
      if (temp_texture) {
        // Draw the original image
        SDL_RenderClear(temp_renderer);
        SDL_RenderCopy(temp_renderer, temp_texture, NULL, NULL);

        // Redraw all the annotations
        // Green rectangle for grid
        SDL_SetRenderDrawColor(temp_renderer, 0, 255, 0, 255);
        rectangle(temp_renderer, grid.x, grid.y, grid.x + grid.w,
                  grid.y + grid.h, 4, 2);

        // Blue rectangle for list (if exists)
        if (list.w > 0 && list.h > 0) {
          SDL_SetRenderDrawColor(temp_renderer, 0, 128, 255, 255);
          rectangle(temp_renderer, list.x, list.y, list.x + list.w,
                    list.y + list.h, 4, 2);
        }

        // Red outlines for found words
        SDL_SetRenderDrawColor(temp_renderer, 255, 0, 0, 255);
        for (int i = 0; i < saved_words_count; i++) {
          if (word_positions[i].x1 != -1) {
            draw_outline(temp_renderer, word_positions[i].x1,
                         word_positions[i].y1, word_positions[i].x2,
                         word_positions[i].y2, outline_width, outline_stroke);
          }
        }

        SDL_RenderPresent(temp_renderer);

        // Now read the pixels and save
        SDL_Surface *result_surface =
            SDL_CreateRGBSurface(0, surface->w, surface->h, 32, 0x00FF0000,
                                 0x0000FF00, 0x000000FF, 0xFF000000);
        if (result_surface) {
          if (SDL_RenderReadPixels(
                  temp_renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                  result_surface->pixels, result_surface->pitch) == 0) {
            if (SDL_SaveBMP(result_surface, "result.png") == 0) {
              printf("✓ Saved annotated image to result.png\n");
            }
          }
          SDL_FreeSurface(result_surface);
        }

        SDL_DestroyTexture(temp_texture);
      }
      SDL_DestroyRenderer(temp_renderer);
    }
    SDL_DestroyWindow(temp_window);
  }

  // Clean up word positions
  if (word_positions) {
    free(word_positions);
  }

  return surface; // surface is modified in-place
}