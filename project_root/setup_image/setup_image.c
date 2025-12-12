// setup_image.c
#include "setup_image.h"

#include <stdio.h>      // printf, snprintf
#include <stdlib.h>     // EXIT_* and friends
#include <string.h>     // strrchr, strlen, strncpy, strcasecmp
#include <err.h>        // errx

#include "../image_cleaner/image_cleaner.h"  // convert_to_grayscale, apply_otsu_thresholding, ...
#include "../rotation/rotation.h"            // auto_deskew_correction, rotate

/* ---------------------------------------------------------------------------
 * image_data helpers
 * -------------------------------------------------------------------------- */

void fill_data(struct image_data *data, char *filepath) {
    if (!data)     errx(1, "fill_data: data is NULL");
    if (!filepath) errx(1, "fill_data: filepath is NULL");

    data->filepath = filepath;            // keep original path

    // Extract base name after last '/'
    const char *slash    = strrchr(filepath, '/');
    const char *basename = slash ? slash + 1 : filepath;

    // Find last '.' to split name / extension
    const char *dot = strrchr(basename, '.');
    if (!dot) dot = basename + strlen(basename); // no extension -> dot at end

    // Copy name into static buffer (single global buffer)
    static char name_buffer[512];
    size_t name_len = (size_t)(dot - basename);
    if (name_len >= sizeof(name_buffer)) name_len = sizeof(name_buffer) - 1;
    strncpy(name_buffer, basename, name_len);
    name_buffer[name_len] = '\0';
    data->name = name_buffer;             // points to static buffer

    // File type (extension without dot) or empty string
    data->filetype = (*dot) ? (char*)(dot + 1) : (char*)"";
}

void print_image_data(const struct image_data *data) {
    if (!data) return;

    printf("Image Path    : %s\n", data->filepath ? data->filepath : "(null)");
    printf("Image Name    : %s\n", data->name     ? data->name     : "(null)");
    printf("Image Filetype: %s\n", data->filetype ? data->filetype : "(null)");
}

/* ---------------------------------------------------------------------------
 * SDL utilities
 * -------------------------------------------------------------------------- */

void get_surface(SDL_Renderer *renderer, SDL_Surface **surface) {
    if (!renderer) errx(1, "get_surface: renderer is NULL");
    if (!surface)  errx(1, "get_surface: surface** is NULL");

    SDL_Window *window = SDL_RenderGetWindow(renderer);   // get window owning the renderer
    if (!window) errx(1, "get_surface: failed to get window from renderer");

    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);                    // query window size

    *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!*surface) errx(1, "get_surface: failed to create surface");

    // Read pixels from the current backbuffer into the surface
    if (SDL_RenderReadPixels(renderer, NULL,
                             SDL_PIXELFORMAT_RGBA8888,
                             (*surface)->pixels,
                             (*surface)->pitch) != 0) {
        errx(1, "get_surface: SDL_RenderReadPixels: %s", SDL_GetError());
    }
}

void actualize_rendering(SDL_Renderer *renderer,
                         SDL_Texture  *texture,
                         SDL_Surface  *surface) {
    if (!renderer) errx(1, "actualize_rendering: renderer is NULL");
    if (!texture)  errx(1, "actualize_rendering: texture is NULL");
    if (!surface)  errx(1, "actualize_rendering: surface is NULL");

    // Create a temporary texture from the surface
    SDL_Texture *temp = SDL_CreateTextureFromSurface(renderer, surface);
    if (!temp) errx(1, "actualize_rendering: SDL_CreateTextureFromSurface: %s", SDL_GetError());

    // Draw onto the off-screen texture (texture is the render target)
    if (SDL_SetRenderTarget(renderer, texture) != 0)
        errx(1, "actualize_rendering: SDL_SetRenderTarget: %s", SDL_GetError());

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);       // clear to black
    SDL_RenderClear(renderer);

    if (SDL_RenderCopy(renderer, temp, NULL, NULL) != 0)
        errx(1, "actualize_rendering: SDL_RenderCopy: %s", SDL_GetError());

    SDL_SetRenderTarget(renderer, NULL);                  // back to default target
    SDL_DestroyTexture(temp);

    // Copy the texture to the window and present
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

/* ---------------------------------------------------------------------------
 * Saving helpers
 * -------------------------------------------------------------------------- */

static void build_filename(const struct image_data *data,
                           const char *suffix,
                           char *out,
                           size_t out_size) {
    // e.g. "name_suffix.ext"
    snprintf(out, out_size, "%s_%s.%s",
             data->name ? data->name : "output",
             suffix     ? suffix     : "out",
             data->filetype && *data->filetype ? data->filetype : "bmp");
}

void save_sketch(struct image_data *data,
                 SDL_Renderer     *renderer,
                 const char       *suffix) {
    if (!data || !renderer || !suffix)
        errx(1, "save_sketch: invalid arguments");

    SDL_Window *window = SDL_RenderGetWindow(renderer);
    if (!window) errx(1, "save_sketch: failed to get window");

    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);                    // use window size

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
        0, w, h, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!surface) errx(1, "save_sketch: failed to create surface");

    // Capture current backbuffer
    if (SDL_RenderReadPixels(renderer, NULL,
                             SDL_PIXELFORMAT_RGBA8888,
                             surface->pixels,
                             w * 4) != 0) {
        errx(1, "save_sketch: SDL_RenderReadPixels: %s", SDL_GetError());
    }

    char filename[512];
    build_filename(data, suffix, filename, sizeof(filename));

    int rc = 0;
    if (data->filetype && strcasecmp(data->filetype, "png") == 0) {
        rc = IMG_SavePNG(surface, filename);
    } else if (data->filetype &&
               (strcasecmp(data->filetype, "jpg")  == 0 ||
                strcasecmp(data->filetype, "jpeg") == 0)) {
        rc = IMG_SaveJPG(surface, filename, 100);
    } else {
        rc = SDL_SaveBMP(surface, filename);
    }

    if (rc != 0)
        errx(1, "save_sketch: failed to save file '%s': %s", filename, SDL_GetError());

    printf("Saved image successfully as: %s\n", filename);

    SDL_FreeSurface(surface);
}

void save_surface(struct image_data *data,
                  SDL_Surface     *surface,
                  const char      *suffix) {
    if (!data || !surface || !suffix)
        errx(1, "save_surface: invalid arguments");

    char filename[512];
    build_filename(data, suffix, filename, sizeof(filename));

    int rc = 0;
    if (data->filetype && strcasecmp(data->filetype, "png") == 0) {
        rc = IMG_SavePNG(surface, filename);
    } else if (data->filetype &&
               (strcasecmp(data->filetype, "jpg")  == 0 ||
                strcasecmp(data->filetype, "jpeg") == 0)) {
        rc = IMG_SaveJPG(surface, filename, 100);
    } else {
        rc = SDL_SaveBMP(surface, filename);
    }

    if (rc != 0)
        errx(1, "save_surface: failed to save file '%s': %s", filename, SDL_GetError());

    printf("Saved image successfully as: %s\n", filename);
}

/* ---------------------------------------------------------------------------
 * Image loading
 * -------------------------------------------------------------------------- */

void load_in_surface(struct image_data *data, SDL_Surface **surface) {
    if (!data)            errx(1, "load_in_surface: data is NULL");
    if (!data->filepath)  errx(1, "load_in_surface: filepath is NULL");
    if (!surface)         errx(1, "load_in_surface: surface** is NULL");

    if (*surface) {
        SDL_FreeSurface(*surface);         // free previous surface if any
        *surface = NULL;
    }

    SDL_Surface *tmp = IMG_Load(data->filepath);  // load from disk
    if (!tmp)
        errx(1, "load_in_surface: IMG_Load '%s': %s", data->filepath, IMG_GetError());

    *surface = SDL_ConvertSurfaceFormat(tmp, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(tmp);

    if (!*surface)
        errx(1, "load_in_surface: SDL_ConvertSurfaceFormat: %s", SDL_GetError());
}

/* ---------------------------------------------------------------------------
 * SDL init / shutdown
 * -------------------------------------------------------------------------- */

void initialize(struct image_data *data,
                SDL_Window      **window,
                SDL_Renderer    **renderer,
                SDL_Texture     **texture,
                SDL_Surface     **surface,
                char            *file) {
    if (!window || !renderer || !texture || !surface)
        errx(1, "initialize: NULL output pointer");

    if (!file)
        errx(1, "initialize: no image file provided");

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        errx(1, "initialize: SDL_Init: %s", SDL_GetError());

    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(img_flags) & img_flags))
        errx(1, "initialize: IMG_Init: %s", IMG_GetError());

    // Basic window settings
    const char *title = "Words Finder";
    int width = 800, height = 600;

    *window = SDL_CreateWindow(title,
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               width, height, 0);
    if (!*window)
        errx(1, "initialize: SDL_CreateWindow: %s", SDL_GetError());

    *renderer = SDL_CreateRenderer(*window, -1,
                                   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!*renderer)
        errx(1, "initialize: SDL_CreateRenderer: %s", SDL_GetError());

    *texture = SDL_CreateTexture(*renderer,
                                 SDL_PIXELFORMAT_RGBA8888,
                                 SDL_TEXTUREACCESS_TARGET,
                                 width, height);
    if (!*texture)
        errx(1, "initialize: SDL_CreateTexture: %s", SDL_GetError());

    // Load image into surface and push to texture
    (void)data; // caller is expected to have called fill_data(data, file) before
    load_in_surface(data, surface);
    actualize_rendering(*renderer, *texture, *surface);
}

void terminate(SDL_Window *window,
               SDL_Renderer *renderer,
               SDL_Texture *texture) {
    if (texture)  SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window)   SDL_DestroyWindow(window);

    IMG_Quit();
    SDL_Quit();
}

/* ---------------------------------------------------------------------------
 * Simple drawing helper
 * -------------------------------------------------------------------------- */

void draw_line(SDL_Renderer *renderer,
               SDL_Texture  *texture,
               SDL_Color     color,
               int x1, int y1, int x2, int y2) {
    if (!renderer || !texture)
        errx(1, "draw_line: renderer/texture is NULL");

    if (SDL_SetRenderTarget(renderer, texture) != 0)
        errx(1, "draw_line: SDL_SetRenderTarget: %s", SDL_GetError());

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    if (SDL_RenderDrawLine(renderer, x1, y1, x2, y2) != 0)
        errx(1, "draw_line: SDL_RenderDrawLine: %s", SDL_GetError());

    SDL_SetRenderTarget(renderer, NULL);   // back to default target
}

/* ---------------------------------------------------------------------------
 * Event handler (test harness)
 * -------------------------------------------------------------------------- */

int event_handler(struct image_data *data,
                  SDL_Renderer     *renderer,
                  SDL_Texture      *texture,
                  SDL_Surface     **surface,
                  int              *start_x,
                  int              *start_y,
                  SDL_Color        *currColor,
                  float            *hue) {
    (void)start_x;    // currently unused
    (void)start_y;    // currently unused
    (void)currColor;  // currently unused
    (void)hue;        // currently unused

    if (!data || !renderer || !texture || !surface)
        errx(1, "event_handler: invalid arguments");

    SDL_Event event;
    int prs_s = 0;
    int prs_ctrl = 0;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            return 0; // exit main loop

        case SDL_MOUSEBUTTONDOWN:
            // placeholder: could trigger some processing on click
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_c:   // reload image from disk
                load_in_surface(data, surface);
                actualize_rendering(renderer, texture, *surface);
                break;

            case SDLK_r: { // auto deskew + rotate + save
                double angle = auto_deskew_correction(*surface);
                SDL_Surface *rot = rotate(*surface, angle);
                if (!rot) break;
                SDL_FreeSurface(*surface);
                *surface = rot;
                save_surface(data, *surface, "rotation");
                actualize_rendering(renderer, texture, *surface);
                break;
            }

            case SDLK_g:   // grayscale
                convert_to_grayscale(*surface);
                save_surface(data, *surface, "grayscale");
                actualize_rendering(renderer, texture, *surface);
                break;

            case SDLK_h:   // Otsu threshold
                apply_otsu_thresholding(*surface);
                save_surface(data, *surface, "otsu_thresholding");
                actualize_rendering(renderer, texture, *surface);
                break;

            case SDLK_j:   // noise removal (radius 2)
                apply_noise_removal(*surface, 2);
                save_surface(data, *surface, "noise_removal");
                actualize_rendering(renderer, texture, *surface);
                break;

            case SDLK_k:   // reserved for future pipeline / NN
                actualize_rendering(renderer, texture, *surface);
                break;

            case SDLK_LCTRL:
                prs_ctrl = 1;
                break;

            case SDLK_s:
                prs_s = 1;
                break;

            default:
                break;
            }
            break;

        default:
            break;
        }
    }

    // Ctrl+S -> save current window content as "output"
    if (prs_s && prs_ctrl)
        save_sketch(data, renderer, "output");

    return 1; // keep running
}
