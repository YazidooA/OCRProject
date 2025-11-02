#include <SDL2/SDL.h>
#include <stdlib.h>
#include <math.h>
#include "structure_detection.h"

/* --- utils C purs --- */
static inline int is_black(Uint32 v) {
    Uint8 a = (Uint8)(v >> 24);
    Uint8 r = (Uint8)(v >> 16);          /* binaire: R=G=B */
    return (a >= 128 && r < 128);
}

static int find_dense_band(const Uint32 *P, int pitch, int y1, int y2,
                           int x0, int x1, SDL_Rect *out)
{
    if (x0 > x1 || y1 > y2) { out->x=out->y=out->w=out->h=0; return 0; }

    int Wband = x1 - x0 + 1;
    int Hband = y2 - y1 + 1;

    /* 1) Profil vertical brut (nb de pixels noirs par colonne) */
    int *V = (int*)calloc((size_t)Wband, sizeof(int));
    if (!V) { out->x=out->y=out->w=out->h=0; return 0; }
    int vmax = 0; double vmean = 0.0;
    for (int x = x0; x <= x1; ++x) {
        int c = 0;
        for (int y = y1; y <= y2; ++y) {
            Uint32 v = P[y*pitch + x];
            Uint8 a = (Uint8)(v >> 24);
            Uint8 r = (Uint8)(v >> 16);
            if (a >= 128 && r < 128) c++;
        }
        V[x - x0] = c;
        if (c > vmax) vmax = c;
        vmean += c;
    }
    vmean /= (double)Wband;

    /* 2) Lissage horizontal (moyenne glissante) */
    int wx = Wband / 30; if (wx < 5) wx = 5; if ((wx & 1) == 0) wx++;
    int hx = wx / 2;
    int *Vs = (int*)calloc((size_t)Wband, sizeof(int));
    if (!Vs) { free(V); out->x=out->y=out->w=out->h=0; return 0; }
    for (int i = 0; i < Wband; ++i) {
        int a = i - hx; if (a < 0) a = 0;
        int b = i + hx; if (b >= Wband) b = Wband - 1;
        long long s = 0;
        for (int t = a; t <= b; ++t) s += V[t];
        Vs[i] = (int)(s / (b - a + 1));
    }

    /* 3) Seuil adaptatif “fort” */
    int thr = (int)(vmean + 0.15 * (vmax - vmean)); if (thr < 3) thr = 3;

    /* 4) Closing 1D: comble les petits gaps entre colonnes */
    unsigned char *mask = (unsigned char*)calloc((size_t)Wband, 1);
    if (!mask) { free(V); free(Vs); out->x=out->y=out->w=out->h=0; return 0; }
    for (int i = 0; i < Wband; ++i) mask[i] = (Vs[i] >= thr);

    int gap_max = Wband / 50; if (gap_max < 3) gap_max = 3;
    for (int i = 0; i < Wband; ) {
        if (mask[i]) { i++; continue; }
        int z0 = i; while (i < Wband && !mask[i]) i++; int z1 = i - 1;
        int left1  = (z0 > 0 && mask[z0 - 1]);
        int right1 = (i < Wband && mask[i]);
        if (left1 && right1 && (z1 - z0 + 1) <= gap_max)
            for (int k = z0; k <= z1; ++k) mask[k] = 1;
    }

    /* 5) Bloc initial = [i1..i2] du premier au dernier “1” */
    int i1 = -1, i2 = -1;
    for (int i = 0; i < Wband; ++i) { if (mask[i]) { i1 = i; break; } }
    if (i1 >= 0) for (int i = Wband - 1; i >= 0; --i) { if (mask[i]) { i2 = i; break; } }
    if (i1 < 0 || i2 < i1) {
        free(mask); free(V); free(Vs);
        out->x=out->y=out->w=out->h=0; return 0;
    }
    /* 6) Extension du bloc à gauche et à droite */
    int padX = Wband / 60; if (padX < 2) padX = 2;
    if (i1 - padX < 0) i1 = 0; else i1 -= padX;
    if (i2 + padX >= Wband) i2 = Wband - 1; else i2 += padX;

    int win = 3;
    int gap2 = gap_max; if (gap2 < 3) gap2 = 3;

    /* étend à droite */
    int zeros = 0, j = i2 + 1;
    while (j < Wband) {
        int s = 0;
        for (int t = 0; t < win && j + t < Wband; ++t) s += V[j + t];
        if (s > 0) { i2 = j + win - 1; if (i2 >= Wband) i2 = Wband - 1; j = i2 + 1; zeros = 0; continue; }
        if (++zeros > gap2) break;
        j++;
    }

    /* étend à gauche */
    zeros = 0; j = i1 - 1;
    while (j >= 0) {
        int s = 0;
        for (int t = 0; t < win && j - t >= 0; ++t) s += V[j - t];
        if (s > 0) { i1 = j - win + 1; if (i1 < 0) i1 = 0; j = i1 - 1; zeros = 0; continue; }
        if (++zeros > gap2) break;
        j--;
    }

    out->x = x0 + i1;
    out->w = (i2 - i1 + 1);

    /* 7) Clip vertical (lissé) + petite marge verticale */
    int *Hy = (int*)calloc((size_t)Hband, sizeof(int));
    if (!Hy) { free(mask); free(V); free(Vs); out->x=out->y=out->w=out->h=0; return 0; }
    int hy_max = 0; double hy_mean = 0.0;
    for (int y = y1; y <= y2; ++y) {
        int c = 0;
        for (int x = out->x; x < out->x + out->w; ++x) {
            Uint32 v = P[y*pitch + x];
            Uint8 a = (Uint8)(v >> 24);
            Uint8 r = (Uint8)(v >> 16);
            if (a >= 128 && r < 128) c++;
        }
        Hy[y - y1] = c;
        if (c > hy_max) hy_max = c;
        hy_mean += c;
    }
    hy_mean /= (double)Hband;

    int wy = Hband / 60; if (wy < 5) wy = 5; if ((wy & 1) == 0) wy++;
    int hyh = wy / 2;
    int *HyS = (int*)calloc((size_t)Hband, sizeof(int));
    if (!HyS) { free(Hy); free(mask); free(V); free(Vs); out->x=out->y=out->w=out->h=0; return 0; }
    for (int i = 0; i < Hband; ++i) {
        int a = i - hyh; if (a < 0) a = 0;
        int b = i + hyh; if (b >= Hband) b = Hband - 1;
        long long s = 0;
        for (int t = a; t <= b; ++t) s += Hy[t];
        HyS[i] = (int)(s / (b - a + 1));
    }

    int thh = (int)(hy_mean + 0.15 * (hy_max - hy_mean)); if (thh < 2) thh = 2;
    int top = y1, bot = y2;
    for (int i = 0; i < Hband; ++i) if (HyS[i] >= thh) { top = y1 + i; break; }
    for (int i = Hband - 1; i >= 0; --i) if (HyS[i] >= thh) { bot = y1 + i; break; }

    int padY = Hband / 80; if (padY < 2) padY = 2;
    if (top - padY < y1) top = y1; else top -= padY;
    if (bot + padY > y2) bot = y2; else bot += padY;

    out->y = top; out->h = bot - top + 1;

    free(HyS); free(Hy); free(mask); free(V); free(Vs);

    int pad = 3;
    int nx = out->x - pad; if (nx < x0) nx = x0;
    int ny = out->y - pad; if (ny < y1) ny = y1;
    int nw = out->w + 2*pad; if (nx + nw - 1 > x1) nw = x1 - nx + 1;
    int nh = out->h + 2*pad; if (ny + nh - 1 > y2) nh = y2 - ny + 1;
    out->x = nx; out->y = ny; out->w = nw; out->h = nh;

    return (out->w > 0 && out->h > 0);
}



int detect_grid_and_list(SDL_Surface *src, SDL_Rect *grid, SDL_Rect *list)
{
    if (!src || !grid || !list) return -1;

    SDL_Surface *s32 = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_ARGB8888, 0);
    if (!s32) return -1;
    if (SDL_LockSurface(s32) != 0) { SDL_FreeSurface(s32); return -1; }

    const int W = s32->w, H = s32->h;
    const Uint32 *P = (const Uint32*)s32->pixels;
    const int pitch = s32->pitch / 4;

    /* CCL 8-connexe pour trouver la + grande composante noire (la grille) */
    unsigned char *vis = (unsigned char*)calloc((size_t)W * H, 1);
    int *stack = (int*)malloc(sizeof(int) * (W*H/4 + 1024));
    if (!vis || !stack) { free(vis); free(stack); SDL_UnlockSurface(s32); SDL_FreeSurface(s32); return -1; }

    int bestArea = 0;
    SDL_Rect bestBox = (SDL_Rect){0,0,0,0};

    for (int y = 0; y < H; ++y) {
        const Uint32 *row = P + y*pitch;
        for (int x = 0; x < W; ++x) {
            int id = y*W + x;
            if (vis[id]) continue;
            if (!is_black(row[x])) { vis[id] = 1; continue; }

            int sp = 0; stack[sp++] = id; vis[id] = 1;
            int minx=x, maxx=x, miny=y, maxy=y, area=0;

            while (sp) {
                int idx = stack[--sp];
                int cy = idx / W, cx = idx % W;
                area++;
                if (cx<minx) minx=cx; if (cx>maxx) maxx=cx;
                if (cy<miny) miny=cy; if (cy>maxy) maxy=cy;

                for (int dy=-1; dy<=1; ++dy) {
                    int ny = cy + dy; if ((unsigned)ny >= (unsigned)H) continue;
                    const Uint32 *r2 = P + ny*pitch;
                    for (int dx=-1; dx<=1; ++dx) {
                        if (dx==0 && dy==0) continue;
                        int nx = cx + dx; if ((unsigned)nx >= (unsigned)W) continue;
                        int nid = ny*W + nx;
                        if (vis[nid]) continue;
                        if (!is_black(r2[nx])) { vis[nid]=1; continue; }
                        vis[nid]=1; stack[sp++] = nid;
                    }
                }
            }

            int bw = maxx - minx + 1;
            int bh = maxy - miny + 1;
            if (bw >= W/10 && bh >= H/10 && area > bestArea) {
                bestArea = area;
                bestBox = (SDL_Rect){ minx, miny, bw, bh };
            }
        }
    }

    free(vis); free(stack);

    if (bestArea <= 0) {
        SDL_UnlockSurface(s32); SDL_FreeSurface(s32);
        return -1;
    }

    *grid = bestBox;

    /* Recherche de la liste à droite puis à gauche de la grille */
    SDL_Rect L = {0,0,0,0};
    int margin = W/50; if (margin < 5) margin = 5;
    int y1 = bestBox.y, y2 = bestBox.y + bestBox.h - 1;

    int rx0 = bestBox.x + bestBox.w + margin;
    int rx1 = W - 1 - margin;
    if (rx0 <= rx1) find_dense_band(P, pitch, y1, y2, rx0, rx1, &L);

    if (L.w <= 0 || L.h <= 0) {
        int lx0 = margin;
        int lx1 = bestBox.x - margin - 1;
        if (lx0 <= lx1) find_dense_band(P, pitch, y1, y2, lx0, lx1, &L);
    }

    /* Si la liste ressemble plus à un carré que la grille, on swap par sécurité */
    if (L.w > 0 && L.h > 0) {
        double arG = (double)bestBox.w / (double)bestBox.h;
        double arL = (double)L.w / (double)L.h;
        if (fabs(arL - 1.0) < fabs(arG - 1.0)) {
            SDL_Rect tmp = bestBox; bestBox = L; L = tmp;
        }
    } else {
        L.x = L.y = L.w = L.h = 0;
    }

    *grid = bestBox;
    *list = L;

    SDL_UnlockSurface(s32);
    SDL_FreeSurface(s32);
    return 0;
}
