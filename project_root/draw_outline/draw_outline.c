// draw_outline.c
#include "draw_outline.h"
#include <math.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ========================= RANDOM COLOR HELPERS ========================= //

// Set a bright random color on the renderer (never black, never white,
// and no yellow). This is used to draw visible outlines on a white image.
static void set_random_color(SDL_Renderer *renderer)
{
    if (!renderer)
        return;

    // Seed the random generator only once
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }

    // Bright color palette, clearly visible on white, no yellow
    static const SDL_Color palette[] = {
        {255,   0,   0, 255},  // bright red
        {255,   0, 255, 255},  // magenta
        {  0, 180, 255, 255},  // saturated sky blue
        {  0,   0, 255, 255},  // bright blue
        {  0, 200,   0, 255},  // bright green
        {255,  80,   0, 255},  // orange (more red than yellow)
        {180,   0, 255, 255},  // violet
        {  0, 255, 200, 255},  // turquoise
    };

    int palette_size = (int)(sizeof(palette) / sizeof(palette[0]));
    int index = rand() % palette_size;

    SDL_Color color = palette[index];
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

// ========================= LINE DRAWING HELPERS ========================= //

// Draw a thick line by drawing several parallel lines around the main one.
// stroke = line thickness in pixels (1, 2, 3, ...).
// This only draws the contour, it does not fill any interior.
static void draw_thick_line(SDL_Renderer *renderer,
                            float x1, float y1,
                            float x2, float y2,
                            int stroke)
{
    if (!renderer)
        return;

    if (stroke < 1)
        stroke = 1;

    // Direction of the segment
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrtf(dx * dx + dy * dy);

    // If the segment is too small, we skip it
    if (length < 1e-3f)
        return;

    // Unit normal vector to the segment (perpendicular direction)
    float nx = -dy / length;
    float ny =  dx / length;

    int half = stroke / 2;

    // Draw several parallel lines shifted along the normal
    for (int offset = -half; offset <= half; ++offset) {
        float offx = nx * (float)offset;
        float offy = ny * (float)offset;

        int ax = (int)lroundf(x1 + offx);
        int ay = (int)lroundf(y1 + offy);
        int bx = (int)lroundf(x2 + offx);
        int by = (int)lroundf(y2 + offy);

        SDL_RenderDrawLine(renderer, ax, ay, bx, by);
    }
}

// ========================= PUBLIC API: WORD OUTLINE ========================= //

// Draw a quad outline around a word.
// (x1, y1) : center of the first letter.
// (x2, y2) : center of the last letter.
// width    : "height" of the tube around the word (distance between top and bottom).
// stroke   : thickness of the outline in pixels.
void draw_outline(SDL_Renderer *renderer,
                  int x1, int y1, int x2, int y2,
                  int width, int stroke)
{
    if (!renderer)
        return;

    if (width <= 0)
        width = 1;

    if (stroke <= 0)
        stroke = 1;

    // Choose a random bright color for this word outline
    set_random_color(renderer);

    // Convert to float for geometric computations
    float fx1 = (float)x1;
    float fy1 = (float)y1;
    float fx2 = (float)x2;
    float fy2 = (float)y2;

    // Vector from first to last letter
    float dx = fx2 - fx1;
    float dy = fy2 - fy1;
    float length = sqrtf(dx * dx + dy * dy);

    // If the word is basically a single point, we skip drawing
    if (length < 1e-3f)
        return;

    // Unit direction of the word (from first to last letter)
    float ux = dx / length;
    float uy = dy / length;

    // Half of the "tube" height around the word
    float half_height = 0.5f * (float)width;

    // We extend the segment a bit on both sides so the outline covers
    // the full word area (letters in grid cells).
    fx1 -= ux * half_height;
    fy1 -= uy * half_height;
    fx2 += ux * half_height;
    fy2 += uy * half_height;

    // Normal vector to the word direction
    float nx = -uy;
    float ny =  ux;

    // Compute the 4 corners of the quad around the extended segment:
    // A --- B
    // |     |
    // D --- C
    float ax = fx1 - nx * half_height;
    float ay = fy1 - ny * half_height;

    float bx = fx2 - nx * half_height;
    float by = fy2 - ny * half_height;

    float cx = fx2 + nx * half_height;
    float cy = fy2 + ny * half_height;

    float dx2 = fx1 + nx * half_height;
    float dy2 = fy1 + ny * half_height;

    // Draw only the outline of the quad, using thick lines
    // bottom side  : A -> B
    draw_thick_line(renderer, ax,  ay,  bx,  by,  stroke);
    // right side   : B -> C
    draw_thick_line(renderer, bx,  by,  cx,  cy,  stroke);
    // top side     : C -> D
    draw_thick_line(renderer, cx,  cy,  dx2, dy2, stroke);
    // left side    : D -> A
    draw_thick_line(renderer, dx2, dy2, ax,  ay,  stroke);
}

// ========================= PUBLIC API: SIMPLE RECTANGLE ========================= //

// Draw an axis-aligned rectangle outline (for grid / list bounding boxes).
// (x1, y1) and (x2, y2) are opposite corners.
// width is ignored here, only stroke is used as the thickness.
void rectangle(SDL_Renderer *renderer,
               int x1, int y1, int x2, int y2,
               int width, int stroke)
{
    // width is not used in this implementation
    (void)width;

    if (!renderer)
        return;

    if (stroke <= 0)
        stroke = 1;

    // Use a random bright color for this rectangle
    set_random_color(renderer);

    // Normalize coordinates so left < right and top < bottom
    int left   = (x1 < x2) ? x1 : x2;
    int right  = (x1 < x2) ? x2 : x1;
    int top    = (y1 < y2) ? y1 : y2;
    int bottom = (y1 < y2) ? y2 : y1;

    // Draw the 4 sides using the same thick line helper
    // top side
    draw_thick_line(renderer,
                    (float)left,  (float)top,
                    (float)right, (float)top,
                    stroke);
    // right side
    draw_thick_line(renderer,
                    (float)right, (float)top,
                    (float)right, (float)bottom,
                    stroke);
    // bottom side
    draw_thick_line(renderer,
                    (float)right, (float)bottom,
                    (float)left,  (float)bottom,
                    stroke);
    // left side
    draw_thick_line(renderer,
                    (float)left,  (float)bottom,
                    (float)left,  (float)top,
                    stroke);
}
