// setup_image.h
#ifndef SETUP_IMAGE_H
#define SETUP_IMAGE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

struct image_data {
    char *filepath;   // full path to the image file
    char *name;       // base file name without extension
    char *filetype;   // extension without dot ("png", "jpg", ...)
};

/* Fill image_data from a file path ("path/to/img.png"). */
void fill_data(struct image_data *data, char *filepath);

/* Debug print of image_data. */
void print_image_data(const struct image_data *data);

/* Capture the current window content of `renderer` into a new surface (RGBA8888). */
void get_surface(SDL_Renderer *renderer, SDL_Surface **surface);

/* Copy `surface` into `texture`, clear it to black, then present it to the window. */
void actualize_rendering(SDL_Renderer *renderer,
                         SDL_Texture  *texture,
                         SDL_Surface  *surface);

/* Save current renderer window content to disk with `suffix` appended to base name. */
void save_sketch(struct image_data *data,
                 SDL_Renderer     *renderer,
                 const char       *suffix);

/* Save a given SDL_Surface to disk with `suffix` appended to base name. */
void save_surface(struct image_data *data,
                  SDL_Surface     *surface,
                  const char      *suffix);

/* Load the image from data->filepath into *surface (RGBA8888). Frees previous *surface if non-NULL. */
void load_in_surface(struct image_data *data, SDL_Surface **surface);

/* SDL + window + renderer + texture init, then load image & push it to the texture. */
void initialize(struct image_data *data,
                SDL_Window      **window,
                SDL_Renderer    **renderer,
                SDL_Texture     **texture,
                SDL_Surface     **surface,
                char            *file);

/* Destroy window, renderer, texture and shut down SDL/SDL_image. */
void terminate(SDL_Window *window,
               SDL_Renderer *renderer,
               SDL_Texture *texture);

/* Draw a line on the texture with the given color (does NOT present to window). */
void draw_line(SDL_Renderer *renderer,
               SDL_Texture  *texture,
               SDL_Color     color,
               int x1, int y1, int x2, int y2);

/* Simple event loop handler for tests:
 * - C : reload image
 * - R : auto deskew + rotate + save
 * - G : grayscale + save
 * - H : Otsu threshold + save
 * - J : noise removal + save
 * - Ctrl+S : save current window as "output"
 * Returns 0 to quit, 1 to keep running.
 */
int event_handler(struct image_data *data,
                  SDL_Renderer     *renderer,
                  SDL_Texture      *texture,
                  SDL_Surface     **surface,
                  int              *start_x,
                  int              *start_y,
                  SDL_Color        *currColor,
                  float            *hue);

#endif