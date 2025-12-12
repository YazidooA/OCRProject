#include <SDL2/SDL.h>
#include <stdlib.h>
#include <math.h>
#include "structure_detection.h"

/* ============================================================================
 *  Helper : test "pixel noir" (texte / traits)
 * ============================================================================
 */
static inline int is_black(Uint32 v)
{
    Uint8 a = (Uint8)(v >> 24);
    Uint8 r = (Uint8)(v >> 16);
    return (a >= 128 && r < 128);
}

/* ============================================================================
 *  Bande horizontale dense (pour la LISTE à droite/gauche)
 * ============================================================================
 */
static int find_dense_band(const Uint32 *P, int pitch,
                           int y1, int y2, int x0, int x1,
                           SDL_Rect *out)
{
    if (x0 > x1 || y1 > y2) {
        out->x = out->y = out->w = out->h = 0;
        return 0;
    }

    int Wband = x1 - x0 + 1;
    int Hband = y2 - y1 + 1;

    /* --- borne horizontale : premières / dernières colonnes contenant du noir --- */
    int first_x = -1;
    int last_x  = -1;

    for (int x = x0; x <= x1; ++x) {
        int has_black = 0;
        for (int y = y1; y <= y2; ++y) {
            Uint32 v = P[y * pitch + x];
            if (is_black(v)) {
                has_black = 1;
                break;
            }
        }
        if (has_black) {
            if (first_x < 0)
                first_x = x;
            last_x = x;
        }
    }

    if (first_x < 0 || last_x < first_x) {
        out->x = out->y = out->w = out->h = 0;
        return 0;
    }

    int padX = Wband / 20;
    if (padX < 4) padX = 4;

    int left  = first_x - padX;
    int right = last_x  + padX;

    if (left  < x0) left  = x0;
    if (right > x1) right = x1;

    /* --- borne verticale : premières / dernières lignes contenant du noir --- */
    int first_y = -1;
    int last_y  = -1;

    for (int y = y1; y <= y2; ++y) {
        int has_black = 0;
        for (int x = left; x <= right; ++x) {
            Uint32 v = P[y * pitch + x];
            if (is_black(v)) {
                has_black = 1;
                break;
            }
        }
        if (has_black) {
            if (first_y < 0)
                first_y = y;
            last_y = y;
        }
    }

    if (first_y < 0 || last_y < first_y) {
        out->x = out->y = out->w = out->h = 0;
        return 0;
    }

    int padY = Hband / 20;
    if (padY < 4) padY = 4;

    int top    = first_y - padY;
    int bottom = last_y  + padY;

    if (top    < y1) top    = y1;
    if (bottom > y2) bottom = y2;

    out->x = left;
    out->y = top;
    out->w = right  - left   + 1;
    out->h = bottom - top    + 1;

    return (out->w > 0 && out->h > 0);
}

/* ============================================================================
 *  Structures pour le fallback “lettres seules”
 * ============================================================================
 */
typedef struct {
    float cx, cy;   /* centre (en float) */
    int minx, maxx; /* bounding box */
    int miny, maxy;
    int area;       /* nombre de pixels */
} Comp;

typedef struct {
    int count;
    int minx, maxx;
    int miny, maxy;
    long long sum_area;
} ClusterStats;

static int cmp_comp_cx(const void *a, const void *b)
{
    const Comp *A = (const Comp *)a;
    const Comp *B = (const Comp *)b;
    if (A->cx < B->cx) return -1;
    if (A->cx > B->cx) return 1;
    return 0;
}

static void stats_range(Comp *c, int n, int start, int end,
                        ClusterStats *S, int W, int H)
{
    (void)n; (void)W; (void)H;

    S->count = 0;
    S->minx =  1000000000;
    S->maxx = -1000000000;
    S->miny =  1000000000;
    S->maxy = -1000000000;
    S->sum_area = 0;

    for (int i = start; i <= end; ++i) {
        if (c[i].minx < S->minx) S->minx = c[i].minx;
        if (c[i].maxx > S->maxx) S->maxx = c[i].maxx;
        if (c[i].miny < S->miny) S->miny = c[i].miny;
        if (c[i].maxy > S->maxy) S->maxy = c[i].maxy;
        S->sum_area += c[i].area;
        S->count++;
    }
}

/* ============================================================================
 *  Helper 1 : flood-fill → grosse composante + petites composantes (lettres)
 * ============================================================================
 *
 * - Parcourt toute l'image ARGB8888.
 * - Flood-fill sur les pixels noirs.
 * - Retourne :
 *     *bestBox  / *bestArea : plus grande composante "massive" (candidat grille)
 *     *comps_out / *ncomp_out : tableau des petites composantes (lettres)
 *     gmin/gmax : bounding box globale des lettres
 */
static int flood_fill_components(SDL_Surface *s32,
                                 SDL_Rect *bestBox, int *bestArea,
                                 Comp **comps_out, int *ncomp_out,
                                 int *gminx, int *gmaxx, int *gminy, int *gmaxy)
{
    int W = s32->w;
    int H = s32->h;
    const Uint32 *P = (const Uint32 *)s32->pixels;
    int pitch = s32->pitch / 4;

    unsigned char *vis = (unsigned char *)calloc((size_t)W * H, 1);
    int *stack = (int *)malloc(sizeof(int) * (W * H / 4 + 1024));
    if (!vis || !stack) {
        free(vis);
        free(stack);
        return -1;
    }

    int max_comp = 8192;
    Comp *comps = (Comp *)malloc((size_t)max_comp * sizeof(Comp));
    if (!comps) {
        free(vis);
        free(stack);
        return -1;
    }

    int ncomp = 0;
    int bestA = 0;
    SDL_Rect best = (SDL_Rect){0,0,0,0};
    int minLetterArea = 10;

    int ggminx = W, ggmaxx = -1, ggminy = H, ggmaxy = -1;

    for (int y = 0; y < H; ++y) {
        const Uint32 *row = P + y * pitch;
        for (int x = 0; x < W; ++x) {
            int id = y * W + x;
            if (vis[id]) continue;

            if (!is_black(row[x])) {
                vis[id] = 1;
                continue;
            }

            int sp = 0;
            vis[id] = 1;
            stack[sp++] = id;

            int minx = x, maxx = x, miny = y, maxy = y;
            int area = 0;

            while (sp) {
                int idx = stack[--sp];
                int cy = idx / W;
                int cx = idx % W;
                area++;
                if (cx < minx) minx = cx;
                if (cx > maxx) maxx = cx;
                if (cy < miny) miny = cy;
                if (cy > maxy) maxy = cy;

                for (int dy = -1; dy <= 1; ++dy) {
                    int ny = cy + dy;
                    if ((unsigned)ny >= (unsigned)H)
                        continue;
                    const Uint32 *r2 = P + ny * pitch;
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0)
                            continue;
                        int nx = cx + dx;
                        if ((unsigned)nx >= (unsigned)W)
                            continue;
                        int nid = ny * W + nx;
                        if (vis[nid])
                            continue;
                        if (!is_black(r2[nx])) {
                            vis[nid] = 1;
                            continue;
                        }
                        vis[nid] = 1;
                        stack[sp++] = nid;
                    }
                }
            }

            int bw = maxx - minx + 1;
            int bh = maxy - miny + 1;

            /* Candidat "grille avec traits" si composante assez grosse */
            if (bw >= W / 10 && bh >= H / 10 && area > bestA) {
                bestA     = area;
                best.x    = minx;
                best.y    = miny;
                best.w    = bw;
                best.h    = bh;
            }

            /* Stockage des petites composantes (lettres) pour fallback CAS 2 */
            if (area >= minLetterArea && ncomp < max_comp) {
                comps[ncomp].minx = minx;
                comps[ncomp].maxx = maxx;
                comps[ncomp].miny = miny;
                comps[ncomp].maxy = maxy;
                comps[ncomp].area = area;
                comps[ncomp].cx   = 0.5f * (float)(minx + maxx);
                comps[ncomp].cy   = 0.5f * (float)(miny + maxy);
                ncomp++;

                if (minx < ggminx) ggminx = minx;
                if (maxx > ggmaxx) ggmaxx = maxx;
                if (miny < ggminy) ggminy = miny;
                if (maxy > ggmaxy) ggmaxy = maxy;
            }
        }
    }

    free(vis);
    free(stack);

    *bestBox   = best;
    *bestArea  = bestA;
    *comps_out = comps;
    *ncomp_out = ncomp;
    *gminx     = ggminx;
    *gmaxx     = ggmaxx;
    *gminy     = ggminy;
    *gmaxy     = ggmaxy;

    return 0;
}

/* ============================================================================
 *  Helper 2 : CAS 1 – grande grille avec traits → liste à droite/gauche
 * ============================================================================
 */
static void detect_case1_grid_list(const Uint32 *P, int pitch, int W, int H,
                                   const SDL_Rect *bestBox,
                                   SDL_Rect *grid, SDL_Rect *list)
{
    SDL_Rect G = *bestBox;
    SDL_Rect L = (SDL_Rect){0, 0, 0, 0};

    int margin = W / 50;
    if (margin < 5) margin = 5;

    int y1 = G.y;
    int y2 = G.y + G.h - 1;

    /* Cherche d'abord une bande dense à droite de la grille */
    int rx0 = G.x + G.w + margin;
    int rx1 = W - 1 - margin;
    if (rx0 <= rx1)
        find_dense_band(P, pitch, y1, y2, rx0, rx1, &L);

    /* Si rien à droite, tente à gauche */
    if (L.w <= 0 || L.h <= 0) {
        int lx0 = margin;
        int lx1 = G.x - margin - 1;
        if (lx0 <= lx1)
            find_dense_band(P, pitch, y1, y2, lx0, lx1, &L);
    }

    if (L.w > 0 && L.h > 0) {
        /* On décide qui ressemble le plus à un carré : G ou L */
        double arG = (double)G.w / (double)G.h;
        double arL = (double)L.w / (double)L.h;
        if (fabs(arL - 1.0) < fabs(arG - 1.0)) {
            SDL_Rect tmp = G;
            G = L;
            L = tmp;
        }

        /* Élargit légèrement la liste vers l’extérieur (pour ne pas couper la fin) */
        int extraR = W / 40;
        if (extraR < 8) extraR = 8;
        int r = L.x + L.w - 1 + extraR;
        if (r > W - 1) r = W - 1;
        L.w = r - L.x + 1;
    } else {
        /* pas de liste trouvée */
        L.x = L.y = L.w = L.h = 0;
    }

    *grid = G;
    *list = L;
}

/* ============================================================================
 *  Helper 3 : CAS 2 – pas de grande grille, lettres seules (split grid | list)
 * ============================================================================
 *
 * Reprend exactement tes heuristiques :
 *   - bounding global
 *   - si globalW petit → tout = grille
 *   - tri par cx, gap max, cluster gauche/droite
 *   - choix grille vs liste selon aspect ratio
 *   - checks de fiabilité (taille, overlap vertical, largeur relative)
 *   - ajustements horizontaux + extension de la grille
 */
static void detect_case2_letters_only(const Uint32 *P, int pitch, int W, int H,
                                      Comp *comps, int ncomp,
                                      int gminx, int gmaxx, int gminy, int gmaxy,
                                      SDL_Rect *grid, SDL_Rect *list)
{
    /* Si bizarre → tout le bloc */
    if (gminx > gmaxx || gminy > gmaxy) {
        gminx = 0;
        gminy = 0;
        gmaxx = W - 1;
        gmaxy = H - 1;
    }

    int globalW = gmaxx - gminx + 1;
    int globalH = gmaxy - gminy + 1;

    /* Lettres toutes dans une petite zone → tout = grille, pas de liste. */
    if (globalW < (int)(0.2 * W)) {
        grid->x = gminx;
        grid->y = gminy;
        grid->w = globalW;
        grid->h = globalH;
        list->x = list->y = list->w = list->h = 0;
        return;
    }

    /* Trie par centre horizontal (cx) : gauche→droite */
    qsort(comps, (size_t)ncomp, sizeof(Comp), cmp_comp_cx);

    /* Cherche le plus gros "gap" entre deux comp. consécutives */
    int   bestSplit = -1;
    float bestGap   = 0.0f;

    for (int i = 0; i < ncomp - 1; ++i) {
        float gap = comps[i + 1].cx - comps[i].cx;
        if (gap > bestGap) {
            bestGap   = gap;
            bestSplit = i;
        }
    }

    if (bestSplit < 0) {
        /* pas de split exploitable : tout = grille */
        grid->x = gminx;
        grid->y = gminy;
        grid->w = globalW;
        grid->h = globalH;
        list->x = list->y = list->w = list->h = 0;
        return;
    }

    double gapNorm = bestGap / (double)globalW;
    if (gapNorm < 0.03) {
        /* pas de vrai "couloir vide" → tout = grille */
        grid->x = gminx;
        grid->y = gminy;
        grid->w = globalW;
        grid->h = globalH;
        list->x = list->y = list->w = list->h = 0;
        return;
    }

    /* Stats sur chaque côté du split */
    ClusterStats left, right;
    stats_range(comps, ncomp, 0,           bestSplit,   &left,  W, H);
    stats_range(comps, ncomp, bestSplit+1, ncomp - 1,   &right, W, H);

    if (left.count < 4 || right.count < 2) {
        /* pas assez de lettres d’un côté → on ne sépare pas */
        grid->x = gminx;
        grid->y = gminy;
        grid->w = globalW;
        grid->h = globalH;
        list->x = list->y = list->w = list->h = 0;
        return;
    }

    double wL = (double)(left.maxx  - left.minx  + 1);
    double hL = (double)(left.maxy  - left.miny  + 1);
    double wR = (double)(right.maxx - right.minx + 1);
    double hR = (double)(right.maxy - right.miny + 1);

    if (hL <= 0.0) hL = 1.0;
    if (hR <= 0.0) hR = 1.0;

    double arL = wL / hL;
    double arR = wR / hR;

    double scoreL = fabs(arL - 1.0);  /* proximité d’un carré */
    double scoreR = fabs(arR - 1.0);

    ClusterStats *G  = &left;
    ClusterStats *Li = &right;

    if (scoreR < scoreL) {
        /* bloc droit plus "carré" → grille à droite, liste à gauche */
        G  = &right;
        Li = &left;
    }

    double wG  = (double)(G->maxx  - G->minx  + 1);
    double wLi = (double)(Li->maxx - Li->minx + 1);
    double relWL = wLi / (double)globalW;
    if (relWL < 0.05 || relWL > 0.6) {
        /* liste trop petite ou trop large → séparation pas crédible */
        grid->x = gminx;
        grid->y = gminy;
        grid->w = globalW;
        grid->h = globalH;
        list->x = list->y = list->w = list->h = 0;
        return;
    }

    /* Forte superposition verticale entre G et Li */
    int Gy0 = G->miny;
    int Gy1 = G->maxy;
    int Ly0 = Li->miny;
    int Ly1 = Li->maxy;
    int oy0 = (Gy0 > Ly0) ? Gy0 : Ly0;
    int oy1 = (Gy1 < Ly1) ? Gy1 : Ly1;
    int oH  = oy1 - oy0 + 1;
    int minH = (Gy1 - Gy0 + 1 < Ly1 - Ly0 + 1) ?
               (Gy1 - Gy0 + 1) : (Ly1 - Ly0 + 1);
    if (oH <= 0 || oH < (int)(0.4 * (double)minH)) {
        grid->x = gminx;
        grid->y = gminy;
        grid->w = globalW;
        grid->h = globalH;
        list->x = list->y = list->w = list->h = 0;
        return;
    }

    /* Coupure à mi-distance dans le gap */
    float cxLeft  = comps[bestSplit].cx;
    float cxRight = comps[bestSplit + 1].cx;
    float midx_f  = 0.5f * (cxLeft + cxRight);
    int   midx    = (int)floorf(midx_f);

    SDL_Rect Grect, Lrect;

    if (G->minx < Li->minx) {
        /* grille à gauche, liste à droite */
        int gx0 = G->minx;
        int gx1 = (midx < G->maxx ? G->maxx : midx);
        if (gx1 > gmaxx) gx1 = gmaxx;

        int lx0 = (midx + 1 > Li->minx ? midx + 1 : Li->minx);
        int lx1 = Li->maxx;
        if (lx0 > lx1) {
            grid->x = gminx;
            grid->y = gminy;
            grid->w = globalW;
            grid->h = globalH;
            list->x = list->y = list->w = list->h = 0;
            return;
        }

        Grect.x = gx0;
        Grect.y = G->miny;
        Grect.w = gx1 - gx0 + 1;
        Grect.h = G->maxy - G->miny + 1;

        Lrect.x = lx0;
        Lrect.y = Li->miny;
        Lrect.w = lx1 - lx0 + 1;
        Lrect.h = Li->maxy - Li->miny + 1;
    } else {
        /* grille à droite, liste à gauche */
        int lx0 = Li->minx;
        int lx1 = (midx < Li->maxx ? midx : Li->maxx);
        if (lx1 < lx0) lx1 = lx0;

        int gx0 = (midx + 1 > G->minx ? midx + 1 : G->minx);
        int gx1 = G->maxx;
        if (gx0 > gx1) {
            grid->x = gminx;
            grid->y = gminy;
            grid->w = globalW;
            grid->h = globalH;
            list->x = list->y = list->w = list->h = 0;
            return;
        }

        Grect.x = gx0;
        Grect.y = G->miny;
        Grect.w = gx1 - gx0 + 1;
        Grect.h = G->maxy - G->miny + 1;

        Lrect.x = lx0;
        Lrect.y = Li->miny;
        Lrect.w = lx1 - lx0 + 1;
        Lrect.h = Li->maxy - Li->miny + 1;
    }

    /* Petit padding vertical, mais PAS horizontal */
    int padY = H / 80;
    if (padY < 2) padY = 2;

    int gy0 = Grect.y - padY;
    if (gy0 < 0) gy0 = 0;
    int gy1 = Grect.y + Grect.h - 1 + padY;
    if (gy1 > H - 1) gy1 = H - 1;
    Grect.y = gy0;
    Grect.h = gy1 - gy0 + 1;

    int ly0 = Lrect.y - padY;
    if (ly0 < 0) ly0 = 0;
    int ly1 = Lrect.y + Lrect.h - 1 + padY;
    if (ly1 > H - 1) ly1 = H - 1;
    Lrect.y = ly0;
    Lrect.h = ly1 - ly0 + 1;

    /* ===== Affinage horizontal de la grille sur les colonnes vraiment noires ===== */
    if (Grect.w > 0 && Grect.h > 0) {
        int x0 = Grect.x;
        int x1 = Grect.x + Grect.w - 1;
        int y0 = Grect.y;
        int y1 = Grect.y + Grect.h - 1;

        if (x0 < 0) x0 = 0;
        if (x1 > W - 1) x1 = W - 1;
        if (y0 < 0) y0 = 0;
        if (y1 > H - 1) y1 = H - 1;

        int Wg = x1 - x0 + 1;
        int Hg = y1 - y0 + 1;

        if (Wg > 2 && Hg > 2) {
            int *col = (int *)calloc((size_t)Wg, sizeof(int));
            if (col) {
                int cmax = 0;
                for (int ix = 0; ix < Wg; ++ix) {
                    int x = x0 + ix;
                    int c = 0;
                    for (int y = y0; y <= y1; ++y) {
                        Uint32 v = P[y * pitch + x];
                        if (is_black(v))
                            c++;
                    }
                    col[ix] = c;
                    if (c > cmax) cmax = c;
                }

                if (cmax > 0) {
                    int thr = cmax / 5;
                    if (thr < 2) thr = 2;

                    int iL = -1, iR = -1;
                    for (int ix = 0; ix < Wg; ++ix) {
                        if (col[ix] >= thr) { iL = ix; break; }
                    }
                    for (int ix = Wg - 1; ix >= 0; --ix) {
                        if (col[ix] >= thr) { iR = ix; break; }
                    }

                    if (iL >= 0 && iR >= iL) {
                        int nx0 = x0 + iL;
                        int nx1 = x0 + iR;
                        if (nx0 < Grect.x)
                            nx0 = Grect.x; /* pas d’agrandissement */
                        if (nx1 > Grect.x + Grect.w - 1)
                            nx1 = Grect.x + Grect.w - 1;

                        Grect.x = nx0;
                        Grect.w = nx1 - nx0 + 1;
                    }
                }

                free(col);
            }
        }
    }

    /* ===== Extension horizontale de la grille de ±½ espacement moyen ===== */
    if (Grect.w > 0 && Grect.h > 0) {
        int gx0 = Grect.x;
        int gx1 = Grect.x + Grect.w - 1;
        int gy0 = Grect.y;
        int gy1 = Grect.y + Grect.h - 1;

        int    have_prev = 0;
        float  prev_cx   = 0.0f;
        double sum_dx    = 0.0;
        int    cnt_dx    = 0;

        for (int i = 0; i < ncomp; ++i) {
            float cx = comps[i].cx;
            float cy = comps[i].cy;
            if (cx < gx0 || cx > gx1) continue;
            if (cy < gy0 || cy > gy1) continue;

            if (!have_prev) {
                prev_cx   = cx;
                have_prev = 1;
            } else {
                double d = (double)(cx - prev_cx);
                if (d > 0.5) {
                    sum_dx += d;
                    cnt_dx++;
                }
                prev_cx = cx;
            }
        }

        if (cnt_dx > 0) {
            double avg_dx = sum_dx / (double)cnt_dx;
            int half = (int)(0.5 * avg_dx + 0.5);
            if (half < 2)     half = 2;
            if (half > W / 8) half = W / 8;

            int new_gx0 = gx0 - half;
            int new_gx1 = gx1 + half;

            if (new_gx0 < 0)      new_gx0 = 0;
            if (new_gx1 > W - 1)  new_gx1 = W - 1;

            if (Lrect.w > 0) {
                if (Grect.x < Lrect.x) {
                    /* grille à gauche, liste à droite */
                    if (new_gx1 > midx)
                        new_gx1 = midx;
                } else if (Grect.x > Lrect.x) {
                    /* grille à droite, liste à gauche */
                    if (new_gx0 <= midx)
                        new_gx0 = midx + 1;
                }
            }

            if (new_gx0 < new_gx1) {
                Grect.x = new_gx0;
                Grect.w = new_gx1 - new_gx0 + 1;
            }
        }
    }

    *grid = Grect;
    *list = Lrect;
}

/* ============================================================================
 *  API principale : detect_grid_and_list
 * ============================================================================
 */
int detect_grid_and_list(SDL_Surface *src, SDL_Rect *grid, SDL_Rect *list)
{
    if (!src || !grid || !list)
        return -1;

    SDL_Surface *s32 = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_ARGB8888, 0);
    if (!s32)
        return -1;
    if (SDL_LockSurface(s32) != 0) {
        SDL_FreeSurface(s32);
        return -1;
    }

    const Uint32 *P = (const Uint32 *)s32->pixels;
    int pitch = s32->pitch / 4;
    int W = s32->w;
    int H = s32->h;

    SDL_Rect bestBox;
    int bestArea = 0;
    Comp *comps = NULL;
    int ncomp = 0;
    int gminx, gmaxx, gminy, gmaxy;

    if (flood_fill_components(s32,
                              &bestBox, &bestArea,
                              &comps, &ncomp,
                              &gminx, &gmaxx, &gminy, &gmaxy) != 0) {
        SDL_UnlockSurface(s32);
        SDL_FreeSurface(s32);
        return -1;
    }

    int ret = 0;

    if (bestArea > 0) {
        /* ===================== CAS 1 : grande grille avec traits ===================== */
        detect_case1_grid_list(P, pitch, W, H, &bestBox, grid, list);
    } else {
        /* ===================== CAS 2 : fallback lettres seules ===================== */

        if (ncomp == 0) {
            /* aucune lettre → erreur */
            grid->x = grid->y = grid->w = grid->h = 0;
            list->x = list->y = list->w = list->h = 0;
            ret = -1;
        } else {
            detect_case2_letters_only(P, pitch, W, H,
                                      comps, ncomp,
                                      gminx, gmaxx, gminy, gmaxy,
                                      grid, list);
        }
    }

    free(comps);
    SDL_UnlockSurface(s32);
    SDL_FreeSurface(s32);

    return ret;
}
