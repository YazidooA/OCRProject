#include "letter_extractor.h"
#include "../neural_network/digitalisation.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// ============================= OTSU THRESHOLD ============================= //

// Compute Otsu threshold on a gray buffer [0..255].
static int otsu_threshold_gray(const Uint8 *g, int n) {
  int hist[256] = {0};
  for (int i = 0; i < n; ++i)
    hist[g[i]]++;

  int total = n;
  double sum = 0.0;
  for (int t = 0; t < 256; ++t)
    sum += (double)t * hist[t];

  double sumB = 0.0, maxVar = -1.0;
  int wB = 0, bestT = 128;

  for (int t = 0; t < 256; ++t) {
    wB += hist[t];
    if (wB == 0)
      continue;
    int wF = total - wB;
    if (wF == 0)
      break;

    sumB += (double)t * hist[t];
    double mB = sumB / wB;
    double mF = (sum - sumB) / wF;
    double diff = mB - mF;
    double varBetween = (double)wB * (double)wF * diff * diff;

    if (varBetween > maxVar) {
      maxVar = varBetween;
      bestT = t;
    }
  }
  return bestT;
}

// ============================= ZHANG-SUEN THINNING
// ============================= //

// Apply Zhang–Suen thinning on a 28x28 binary mask.
// mask[i] = 0 (background) or 1 (black pixel).
static void thin_zhang_suen_28(unsigned char *mask) {
  const int W = 28, H = 28;
  int changed;

  do {
    changed = 0;

    // ---- Step 1 ----
    unsigned char *to_remove = (unsigned char *)calloc(W * H, 1);
    if (!to_remove)
      return;

    for (int y = 1; y < H - 1; ++y) {
      for (int x = 1; x < W - 1; ++x) {
        int idx = y * W + x;
        if (!mask[idx])
          continue;

        int p2 = mask[(y - 1) * W + x];
        int p3 = mask[(y - 1) * W + (x + 1)];
        int p4 = mask[y * W + (x + 1)];
        int p5 = mask[(y + 1) * W + (x + 1)];
        int p6 = mask[(y + 1) * W + x];
        int p7 = mask[(y + 1) * W + (x - 1)];
        int p8 = mask[y * W + (x - 1)];
        int p9 = mask[(y - 1) * W + (x - 1)];

        int N = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;

        // Count transitions 0->1 around the 8 neighbors.
        int S = 0;
        int seq[9] = {p2, p3, p4, p5, p6, p7, p8, p9, p2};
        for (int k = 0; k < 8; ++k)
          if (seq[k] == 0 && seq[k + 1] == 1)
            S++;

        if (N >= 2 && N <= 6 && S == 1 && (p2 * p4 * p6 == 0) &&
            (p4 * p6 * p8 == 0)) {
          to_remove[idx] = 1;
        }
      }
    }

    for (int i = 0; i < W * H; ++i)
      if (to_remove[i] && mask[i]) {
        mask[i] = 0;
        changed = 1;
      }

    free(to_remove);

    // ---- Step 2 ----
    to_remove = (unsigned char *)calloc(W * H, 1);
    if (!to_remove)
      return;

    for (int y = 1; y < H - 1; ++y) {
      for (int x = 1; x < W - 1; ++x) {
        int idx = y * W + x;
        if (!mask[idx])
          continue;

        int p2 = mask[(y - 1) * W + x];
        int p3 = mask[(y - 1) * W + (x + 1)];
        int p4 = mask[y * W + (x + 1)];
        int p5 = mask[(y + 1) * W + (x + 1)];
        int p6 = mask[(y + 1) * W + x];
        int p7 = mask[(y + 1) * W + (x - 1)];
        int p8 = mask[y * W + (x - 1)];
        int p9 = mask[(y - 1) * W + (x - 1)];

        int N = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;

        int S = 0;
        int seq[9] = {p2, p3, p4, p5, p6, p7, p8, p9, p2};
        for (int k = 0; k < 8; ++k)
          if (seq[k] == 0 && seq[k + 1] == 1)
            S++;

        if (N >= 2 && N <= 6 && S == 1 && (p2 * p4 * p8 == 0) &&
            (p2 * p6 * p8 == 0)) {
          to_remove[idx] = 1;
        }
      }
    }

    for (int i = 0; i < W * H; ++i)
      if (to_remove[i] && mask[i]) {
        mask[i] = 0;
        changed = 1;
      }

    free(to_remove);

  } while (changed);
}

// ===================== LETTER THINNING + SIZE NORMALIZATION
// ===================== //

// Thin a 28x28 letter if it is "fat", keep the main connected component,
// optionally zoom small letters, recenter, and convert to 0/255.
static void maybe_thin_letter(Uint8 *buf784) {
  const int W = 28, H = 28, N = W * H;
  const double cx = (W - 1) / 2.0; // ~13.5
  const double cy = (H - 1) / 2.0;

  // 1) Detect polarity: dark vs bright pixels
  int dark_count = 0, light_count = 0;
  for (int i = 0; i < N; ++i) {
    Uint8 v = buf784[i];
    if (v < 128)
      dark_count++;
    else
      light_count++;
  }
  if (dark_count == 0 && light_count == 0)
    return;

  int minority_is_dark = (dark_count <= light_count);

  // 2) Build a binary mask: "black" = minority side (to handle inverted images)
  unsigned char mask[28 * 28];
  int black_count = 0;

  if (minority_is_dark) {
    for (int i = 0; i < N; ++i) {
      Uint8 v = buf784[i];
      if (v < 128) {
        mask[i] = 1;
        black_count++;
      } else
        mask[i] = 0;
    }
  } else {
    for (int i = 0; i < N; ++i) {
      Uint8 v = buf784[i];
      if (v >= 128) {
        mask[i] = 1;
        black_count++;
      } else
        mask[i] = 0;
    }
  }
  if (black_count == 0)
    return;

  double fill = (double)black_count / (double)N;
  const double FILL_THICK = 0.2; // threshold to say "fat letter"
  if (fill <= FILL_THICK)
    return;

  // Keep a safe copy of the original buffer in case we destroy everything
  Uint8 original[28 * 28];
  for (int i = 0; i < N; ++i)
    original[i] = buf784[i];

  // 3) Apply Zhang–Suen thinning on the mask
  thin_zhang_suen_28(mask);

  // 4) Connected components with neighborhood radius <= 2,
  //    keep the component whose COM is closest to the center.
  int labels[28 * 28];
  for (int i = 0; i < N; ++i)
    labels[i] = -1;

  int comp_count = 0;
  int comp_size[28 * 28];
  double comp_sumx[28 * 28], comp_sumy[28 * 28];

  for (int i = 0; i < N; ++i) {
    if (!mask[i] || labels[i] != -1)
      continue;

    int stack[28 * 28];
    int top = 0;
    labels[i] = comp_count;
    stack[top++] = i;

    comp_size[comp_count] = 0;
    comp_sumx[comp_count] = 0.0;
    comp_sumy[comp_count] = 0.0;

    while (top > 0) {
      int idx = stack[--top];
      int y = idx / W, x = idx % W;

      comp_size[comp_count]++;
      comp_sumx[comp_count] += (double)x;
      comp_sumy[comp_count] += (double)y;

      // Neighborhood up to distance 2 in both x and y
      for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
          if (dx == 0 && dy == 0)
            continue;
          int nx = x + dx, ny = y + dy;
          if (nx < 0 || nx >= W || ny < 0 || ny >= H)
            continue;
          int nidx = ny * W + nx;
          if (mask[nidx] && labels[nidx] == -1) {
            labels[nidx] = comp_count;
            stack[top++] = nidx;
          }
        }
      }
    }
    comp_count++;
    if (comp_count >= N)
      break; // safety
  }

  if (comp_count == 0) {
    for (int i = 0; i < N; ++i)
      buf784[i] = original[i];
    return;
  }

  // Choose the "best" component: closest to center, then largest size
  int best_comp = 0, best_size = 0;
  double best_dist2 = 1e30;

  for (int c = 0; c < comp_count; ++c) {
    if (comp_size[c] == 0)
      continue;
    double mx = comp_sumx[c] / (double)comp_size[c];
    double my = comp_sumy[c] / (double)comp_size[c];
    double dx = mx - cx, dy = my - cy;
    double dist2 = dx * dx + dy * dy;
    if (dist2 < best_dist2 ||
        (dist2 == best_dist2 && comp_size[c] > best_size)) {
      best_dist2 = dist2;
      best_size = comp_size[c];
      best_comp = c;
    }
  }

  unsigned char mask_cc[28 * 28];
  int black_cc = 0;
  for (int i = 0; i < N; ++i) {
    if (labels[i] == best_comp) {
      mask_cc[i] = 1;
      black_cc++;
    } else
      mask_cc[i] = 0;
  }
  if (black_cc == 0) {
    for (int i = 0; i < N; ++i)
      buf784[i] = original[i];
    return;
  }

  // 5) Bounding box of the chosen component
  int bminx = W, bmaxx = -1, bminy = H, bmaxy = -1;
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      int idx = y * W + x;
      if (!mask_cc[idx])
        continue;
      if (x < bminx)
        bminx = x;
      if (x > bmaxx)
        bmaxx = x;
      if (y < bminy)
        bminy = y;
      if (y > bmaxy)
        bmaxy = y;
    }
  }
  if (bmaxx < bminx || bmaxy < bminy) {
    for (int i = 0; i < N; ++i)
      buf784[i] = original[i];
    return;
  }

  int bw = bmaxx - bminx + 1;
  int bh = bmaxy - bminy + 1;
  int S = (bw > bh ? bw : bh);

  // 6) Zoom if the letter is too small
  const int S_MIN = 14, S_TARGET = 20;
  const double SCALE_MAX = 2.0;

  unsigned char zoom_mask[28 * 28];
  for (int i = 0; i < N; ++i)
    zoom_mask[i] = mask_cc[i];

  if (S < S_MIN) {
    double scale = (double)S_TARGET / (double)S;
    if (scale < 1.0)
      scale = 1.0;
    else if (scale > SCALE_MAX)
      scale = SCALE_MAX;

    unsigned char tmp_mask[28 * 28];
    for (int i = 0; i < N; ++i)
      tmp_mask[i] = 0;

    // Scale around the global center (cx, cy)
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        int idx = y * W + x;
        if (!mask_cc[idx])
          continue;

        double dx = (double)x - cx;
        double dy = (double)y - cy;
        double x2 = cx + dx * scale;
        double y2 = cy + dy * scale;
        int xi = (int)lround(x2);
        int yi = (int)lround(y2);
        if (xi >= 0 && xi < W && yi >= 0 && yi < H)
          tmp_mask[yi * W + xi] = 1;
      }
    }

    // Small 3x3 dilation to avoid gaps after scaling
    unsigned char dil_mask[28 * 28];
    for (int i = 0; i < N; ++i)
      dil_mask[i] = 0;

    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        int idx = y * W + x;
        if (!tmp_mask[idx])
          continue;
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= W || ny < 0 || ny >= H)
              continue;
            dil_mask[ny * W + nx] = 1;
          }
        }
      }
    }

    int nb_zoom_black = 0;
    for (int i = 0; i < N; ++i)
      if (dil_mask[i])
        nb_zoom_black++;

    if (nb_zoom_black > 0)
      for (int i = 0; i < N; ++i)
        zoom_mask[i] = dil_mask[i];
  }

  // 7) Recentering after zoom: move COM to the exact center (cx, cy)
  int sumx = 0, sumy = 0, count = 0;
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      int idx = y * W + x;
      if (!zoom_mask[idx])
        continue;
      sumx += x;
      sumy += y;
      count++;
    }
  }

  if (count > 0) {
    double mx = (double)sumx / (double)count;
    double my = (double)sumy / (double)count;

    // shift_x > 0 means we move the pattern to the left, etc.
    int shift_x = (int)lround(mx - cx);
    int shift_y = (int)lround(my - cy);

    if (shift_x != 0 || shift_y != 0) {
      unsigned char rec_mask[28 * 28];
      for (int i = 0; i < N; ++i)
        rec_mask[i] = 0;

      int rec_black = 0;
      for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
          int idx = y * W + x;
          if (!zoom_mask[idx])
            continue;
          int nx = x - shift_x;
          int ny = y - shift_y;
          if (nx < 0 || nx >= W || ny < 0 || ny >= H)
            continue;
          int nidx = ny * W + nx;
          rec_mask[nidx] = 1;
          rec_black++;
        }
      }

      // Only replace if we didn't lose everything
      if (rec_black > 0)
        for (int i = 0; i < N; ++i)
          zoom_mask[i] = rec_mask[i];
    }
  }

  // 8) Final conversion: binary mask -> 0/255 gray image for the NN
  int final_black = 0;
  for (int i = 0; i < N; ++i)
    if (zoom_mask[i])
      final_black++;

  if (final_black == 0) {
    for (int i = 0; i < N; ++i)
      buf784[i] = original[i];
    return;
  }

  for (int i = 0; i < N; ++i)
    buf784[i] = zoom_mask[i] ? 0 : 255;

  // ---- DEBUG: save a few tiles after thinning (max 5) ----
  {
    static int debug_count = 0;
    if (debug_count < 5) {
      SDL_Surface *dbg =
          SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
      if (dbg) {
        if (SDL_LockSurface(dbg) == 0) {
          Uint32 *pix = (Uint32 *)dbg->pixels;
          int pitch32 = dbg->pitch / 4;

          for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
              Uint8 v = buf784[y * W + x];
              Uint32 c = (255u << 24) | ((Uint32)v << 16) | ((Uint32)v << 8) |
                         (Uint32)v;
              pix[y * pitch32 + x] = c;
            }
          }

          SDL_UnlockSurface(dbg);
          char filename[64];
          snprintf(filename, sizeof(filename), "debug_thin_%d.bmp",
                   debug_count);
          SDL_SaveBMP(dbg, filename);
        }
        SDL_FreeSurface(dbg);
      }
      debug_count++;
    }
  }
}

// ========================== LETTER GRID EXTRACTION ==========================
// //

#define CLAMP(v, a, b) ((v) < (a) ? (a) : ((v) > (b) ? (b) : (v)))

// Extract letters in a grid ROI [x1..x2] x [y1..y2] (inclusive).
// out_matrix is an N x M matrix of Uint8[784] (or NULL if the cell is empty).
int extract_letters(SDL_Surface *src, int x1, int y1, int x2, int y2,
                    Uint8 ****out_matrix, int *out_N, int *out_M) {
  if (!src || !out_matrix || !out_N || !out_M)
    return -1;
  if (x2 < x1 || y2 < y1)
    return -2;

  // Convert to ARGB8888 for easier pixel access
  SDL_Surface *s32 = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_ARGB8888, 0);
  if (!s32)
    return -3;
  SDL_SetSurfaceBlendMode(s32, SDL_BLENDMODE_NONE);

  // Clamp ROI inside the surface
  if (x1 < 0)
    x1 = 0;
  if (y1 < 0)
    y1 = 0;
  if (x2 >= s32->w)
    x2 = s32->w - 1;
  if (y2 >= s32->h)
    y2 = s32->h - 1;

  int rw = x2 - x1 + 1;
  int rh = y2 - y1 + 1;

  // Create an ROI surface
  SDL_Surface *roi =
      SDL_CreateRGBSurfaceWithFormat(0, rw, rh, 32, SDL_PIXELFORMAT_ARGB8888);
  if (!roi) {
    SDL_FreeSurface(s32);
    return -4;
  }
  SDL_SetSurfaceBlendMode(roi, SDL_BLENDMODE_NONE);

  SDL_Rect rs = {x1, y1, rw, rh};
  SDL_Rect rd = {0, 0, rw, rh};
  if (SDL_BlitSurface(s32, &rs, roi, &rd) != 0) {
    SDL_FreeSurface(roi);
    SDL_FreeSurface(s32);
    return -5;
  }

  if (SDL_LockSurface(roi) != 0) {
    SDL_FreeSurface(roi);
    SDL_FreeSurface(s32);
    return -6;
  }

  const Uint32 *PR = (const Uint32 *)roi->pixels;
  const SDL_PixelFormat *fmt = roi->format;
  int rpitch = roi->pitch / 4;

  // Build a grayscale buffer G[y * rw + x]
  if (rw <= 0 || rh <= 0) {
    SDL_UnlockSurface(roi);
    SDL_FreeSurface(roi);
    SDL_FreeSurface(s32);
    return -7;
  }

  Uint8 *G = malloc((size_t)rw * (size_t)rh);

  if (!G) {
    SDL_UnlockSurface(roi);
    SDL_FreeSurface(roi);
    SDL_FreeSurface(s32);
    return -7;
  }

  for (int y = 0; y < rh; ++y) {
    const Uint32 *row = PR + y * rpitch;
    for (int x = 0; x < rw; ++x) {
      Uint8 r, g, b, a;
      SDL_GetRGBA(row[x], fmt, &r, &g, &b, &a);
      G[y * rw + x] = (Uint8)(((int)r + (int)g + (int)b) / 3);
    }
  }

  // Global Otsu threshold on ROI
  int T = otsu_threshold_gray(G, rw * rh);
  int BLACK_THR = T + 20;
  if (BLACK_THR > 250)
    BLACK_THR = 250;

  // Projection profiles along x and y
  int *px = (int *)calloc((size_t)rw, sizeof(int));
  int *py = (int *)calloc((size_t)rh, sizeof(int));
  if (!px || !py) {
    free(px);
    free(py);
    free(G);
    SDL_UnlockSurface(roi);
    SDL_FreeSurface(roi);
    SDL_FreeSurface(s32);
    return -8;
  }

  for (int x = 0; x < rw; ++x) {
    int s = 0;
    for (int y = 0; y < rh; ++y)
      s += (G[y * rw + x] < BLACK_THR);
    px[x] = s;
  }
  for (int y = 0; y < rh; ++y) {
    int s = 0;
    for (int x = 0; x < rw; ++x)
      s += (G[y * rw + x] < BLACK_THR);
    py[y] = s;
  }

  // Smoothing window sizes
  int wx = rw / 60;
  if (wx < 5)
    wx = 5;
  if (!(wx & 1))
    wx++;
  int wy = rh / 60;
  if (wy < 5)
    wy = 5;
  if (!(wy & 1))
    wy++;
  int hx = wx / 2, hy = wy / 2;

  // Smoothed projections
  int *sx = (int *)calloc((size_t)rw, sizeof(int));
  int *sy = (int *)calloc((size_t)rh, sizeof(int));
  if (!sx || !sy) {
    free(px);
    free(py);
    free(sx);
    free(sy);
    free(G);
    SDL_UnlockSurface(roi);
    SDL_FreeSurface(roi);
    SDL_FreeSurface(s32);
    return -9;
  }

  for (int i = 0; i < rw; ++i) {
    long long s = 0;
    int a = i - hx;
    if (a < 0)
      a = 0;
    int b = i + hx;
    if (b >= rw)
      b = rw - 1;
    for (int t = a; t <= b; ++t)
      s += px[t];
    sx[i] = (int)(s / (b - a + 1));
  }

  for (int i = 0; i < rh; ++i) {
    long long s = 0;
    int a = i - hy;
    if (a < 0)
      a = 0;
    int b = i + hy;
    if (b >= rh)
      b = rh - 1;
    for (int t = a; t <= b; ++t)
      s += py[t];
    sy[i] = (int)(s / (b - a + 1));
  }

  free(px);
  free(py);

  // Auto-detect horizontal and vertical periods (grid step) by autocorrelation
  int minLagX = rw / 40;
  if (minLagX < 6)
    minLagX = 6;
  int maxLagX = rw / 2;
  if (maxLagX <= minLagX)
    maxLagX = minLagX + 1;

  int minLagY = rh / 40;
  if (minLagY < 6)
    minLagY = 6;
  int maxLagY = rh / 2;
  if (maxLagY <= minLagY)
    maxLagY = minLagY + 1;

  int perX = -1, perY = -1;
  long long bx = -1, by = -1;

  for (int L = minLagX; L <= maxLagX; ++L) {
    long long acc = 0;
    int lim = rw - L;
    for (int i = 0; i < lim; ++i)
      acc += (long long)sx[i] * (long long)sx[i + L];
    if (acc > bx) {
      bx = acc;
      perX = L;
    }
  }

  for (int L = minLagY; L <= maxLagY; ++L) {
    long long acc = 0;
    int lim = rh - L;
    for (int i = 0; i < lim; ++i)
      acc += (long long)sy[i] * (long long)sy[i + L];
    if (acc > by) {
      by = acc;
      perY = L;
    }
  }

  free(sx);
  free(sy);

  if (perX <= 0 || perY <= 0) {
    free(G);
    SDL_UnlockSurface(roi);
    SDL_FreeSurface(roi);
    SDL_FreeSurface(s32);
    return -10;
  }

  // Estimate number of columns (M) and rows (N) in the grid
  int M = (int)lrint((double)rw / (double)perX);
  int N = (int)lrint((double)rh / (double)perY);
  if (M < 1)
    M = 1;
  if (N < 1)
    N = 1;

  double stepX = (double)rw / (double)M;
  double stepY = (double)rh / (double)N;

  // Allocate N x M matrix of Uint8* (each is either NULL or a 28x28 tile)
  Uint8 ***Mat = (Uint8 ***)malloc((size_t)N * sizeof(Uint8 **));
  if (!Mat) {
    free(G);
    SDL_UnlockSurface(roi);
    SDL_FreeSurface(roi);
    SDL_FreeSurface(s32);
    return -11;
  }

  for (int i = 0; i < N; ++i) {
    Mat[i] = (Uint8 **)calloc((size_t)M, sizeof(Uint8 *));
    if (!Mat[i]) {
      for (int t = 0; t < i; ++t)
        free(Mat[t]);
      free(Mat);
      free(G);
      SDL_UnlockSurface(roi);
      SDL_FreeSurface(roi);
      SDL_FreeSurface(s32);
      return -12;
    }
  }

  // -------------- Iterate over each grid cell -------------- //
  for (int i = 0; i < N; ++i) {
    int y_top = (int)floor(i * stepY);
    int y_bot = (int)floor((i + 1) * stepY) - 1;
    if (y_bot >= rh)
      y_bot = rh - 1;

    for (int j = 0; j < M; ++j) {
      int x_left = (int)floor(j * stepX);
      int x_right = (int)floor((j + 1) * stepX) - 1;
      if (x_right >= rw)
        x_right = rw - 1;

      int cw = x_right - x_left + 1;
      int ch = y_bot - y_top + 1;
      if (cw < 2 || ch < 2) {
        Mat[i][j] = NULL;
        continue;
      }

      int xx1 = x_left, xx2 = x_right;
      int yy1 = y_top, yy2 = y_bot;

      // Check if there is any black pixel in the cell
      int black = 0;
      for (int y = yy1; y <= yy2 && !black; ++y) {
        const Uint8 *rowG = G + y * rw;
        for (int x = xx1; x <= xx2; ++x)
          if (rowG[x] < BLACK_THR) {
            black = 1;
            break;
          }
      }
      if (!black) {
        Mat[i][j] = NULL;
        continue;
      }

      // Ignore a small border to avoid catching the grid lines
      int EDGE_IGNORE = CLAMP(cw / 25, 3, 6); // 3..6 px typically
      int bx1 = CLAMP(xx1 + EDGE_IGNORE, xx1, xx2);
      int bx2 = CLAMP(xx2 - EDGE_IGNORE, xx1, xx2);
      int by1 = CLAMP(yy1 + EDGE_IGNORE, yy1, yy2);
      int by2 = CLAMP(yy2 - EDGE_IGNORE, yy1, yy2);

      // Compute bounding box of black pixels inside the "de-bordered" region
      int bminx = 1000000000, bmaxx = -1;
      int bminy = 1000000000, bmaxy = -1;

      for (int y = by1; y <= by2; ++y) {
        const Uint8 *rowG = G + y * rw;
        for (int x = bx1; x <= bx2; ++x) {
          if (rowG[x] < BLACK_THR) {
            if (x < bminx)
              bminx = x;
            if (x > bmaxx)
              bmaxx = x;
            if (y < bminy)
              bminy = y;
            if (y > bmaxy)
              bmaxy = y;
          }
        }
      }

      if (bmaxx < bminx || bmaxy < bminy) {
        Mat[i][j] = NULL;
        continue;
      }

      int bw = bmaxx - bminx + 1;
      int bh = bmaxy - bminy + 1;

      // Center of mass using weights (255 - gray) in the bounding box
      double sxw = 0.0, syw = 0.0, sw = 0.0;
      for (int y = bminy; y <= bmaxy; ++y) {
        const Uint8 *rowG = G + y * rw;
        for (int x = bminx; x <= bmaxx; ++x) {
          Uint8 v = rowG[x];
          int w = (v < BLACK_THR) ? (255 - v) : 0;
          if (w) {
            sxw += (double)x * w;
            syw += (double)y * w;
            sw += (double)w;
          }
        }
      }

      double xbar = sw ? (sxw / sw) : 0.5 * (bminx + bmaxx);
      double ybar = sw ? (syw / sw) : 0.5 * (bminy + bmaxy);

      // Create a square canvas, center letter by COM, and convert to 28x28
      int MARGIN = 4;
      int s = (bw > bh ? bw : bh) + 2 * MARGIN;
      if (s < 8)
        s = 8;

      SDL_Surface *sq =
          SDL_CreateRGBSurfaceWithFormat(0, s, s, 32, SDL_PIXELFORMAT_ARGB8888);
      if (!sq) {
        Mat[i][j] = NULL;
        continue;
      }

      if (SDL_LockSurface(sq) != 0) {
        SDL_FreeSurface(sq);
        Mat[i][j] = NULL;
        continue;
      }

      Uint32 *PS = (Uint32 *)sq->pixels;
      int spitch = sq->pitch / 4;
      Uint32 WHITE = 0xFFFFFFFFu;

      // Fill the square canvas with white
      for (int yy = 0; yy < s; ++yy)
        for (int xx = 0; xx < s; ++xx)
          PS[yy * spitch + xx] = WHITE;

      // Compute offsets so that the letter is centered
      double cx_bbox = 0.5 * (bminx + bmaxx);
      double cy_bbox = 0.5 * (bminy + bmaxy);

      int offx = (s - bw) / 2 + (int)lround(cx_bbox - xbar);
      int offy = (s - bh) / 2 + (int)lround(cy_bbox - ybar);

      offx = CLAMP(offx, 0, s - bw);
      offy = CLAMP(offy, 0, s - bh);

      // Copy the bounding box into the square canvas
      for (int y = 0; y < bh; ++y) {
        const Uint8 *rowG = G + (bminy + y) * rw + bminx;
        Uint32 *rowS = PS + (offy + y) * spitch + offx;
        for (int x = 0; x < bw; ++x) {
          Uint8 v = rowG[x];
          rowS[x] =
              (255u << 24) | ((Uint32)v << 16) | ((Uint32)v << 8) | (Uint32)v;
        }
      }

      // Optional thin white border around the canvas
      for (int x = 0; x < s; ++x) {
        PS[0 * spitch + x] = WHITE;
        PS[1 * spitch + x] = WHITE;
        PS[(s - 1) * spitch + x] = WHITE;
        PS[(s - 2) * spitch + x] = WHITE;
      }
      for (int y = 0; y < s; ++y) {
        PS[y * spitch + 0] = WHITE;
        PS[y * spitch + 1] = WHITE;
        PS[y * spitch + (s - 1)] = WHITE;
        PS[y * spitch + (s - 2)] = WHITE;
      }

      SDL_UnlockSurface(sq);

      // Allocate a 28x28 buffer for the NN
      Uint8 *buf784 = (Uint8 *)malloc(784);
      if (!buf784) {
        SDL_FreeSurface(sq);
        Mat[i][j] = NULL;
        continue;
      }

      // Resize the square surface to 28x28 (in digitalisation.c)
      if (surface_to_28(sq, buf784) != 0) {
        SDL_FreeSurface(sq);
        free(buf784);
        Mat[i][j] = NULL;
        continue;
      }

      SDL_FreeSurface(sq);

      // Final step: optionally thin / zoom / recenter fat letters
      maybe_thin_letter(buf784);

      Mat[i][j] = buf784;
    }
  }

  SDL_UnlockSurface(roi);
  free(G);
  SDL_FreeSurface(roi);
  SDL_FreeSurface(s32);

  *out_matrix = Mat;
  *out_N = N;
  *out_M = M;
  return 0;
}
