// ui.c - Simple UI display functions
#include "ui.h"
#include <stdio.h>

extern char filepath[512];

/* UI rectangles (if needed for future UI elements) */
SDL_Rect textbox_rect = {50, 50, 400, 40};
SDL_Rect browse_btn_rect = {470, 50, 150, 40};

void ui_draw(SDL_Renderer *renderer, SDL_Surface *surface) {
  if (!renderer)
    return;

  SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
  SDL_RenderClear(renderer);

  /* Draw image if surface is provided */
  if (surface) {
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (tex) {
      SDL_Rect r = {50, 120, surface->w, surface->h};
      SDL_RenderCopy(renderer, tex, NULL, &r);
      SDL_DestroyTexture(tex);
    }
  }

  SDL_RenderPresent(renderer);
}

void ui_handle_events(int *running) {
  if (!running)
    return;

  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) {
      *running = 0;
    }

    if (e.type == SDL_MOUSEBUTTONDOWN) {
      int mx = e.button.x;
      int my = e.button.y;

      /* Check if browse button area clicked */
      if (mx >= browse_btn_rect.x &&
          mx < browse_btn_rect.x + browse_btn_rect.w &&
          my >= browse_btn_rect.y &&
          my < browse_btn_rect.y + browse_btn_rect.h) {
        printf("File picker not implemented.\n");
        printf("Usage: ./ui_app <image.png>\n");
      }
    }
  }
}
