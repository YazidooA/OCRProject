/*
 * ui.c - Simple UI display and event handling functions
 *
 * This file provides basic user interface functionality for rendering images
 * and handling SDL events in a window.
 */

#include "ui.h"
#include <stdio.h>

/* External global variable for the current image file path */
extern char filepath[512];

/* UI rectangles (reserved for future UI elements if needed) */
SDL_Rect textbox_rect = {50, 50, 400, 40};
SDL_Rect browse_btn_rect = {470, 50, 150, 40};

/**
 * Draw the UI interface with the current image
 *
 * This function clears the screen and renders the provided image surface
 * in the center of the window. The image is automatically scaled to fit
 * within the available display area while maintaining its aspect ratio.
 *
 * @param renderer SDL renderer to draw with
 * @param surface  Image surface to display (can be NULL)
 */
void ui_draw(SDL_Renderer *renderer, SDL_Surface *surface) {
  /* Safety check: ensure renderer exists */
  if (!renderer)
    return;

  /* Clear the screen with a dark gray background */
  SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
  SDL_RenderClear(renderer);

  /* Draw image if surface is provided */
  if (surface) {
    /* Create a temporary texture from the surface for rendering */
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
    if (tex) {
      /* Define available display area (800x600 window with margins) */
      int max_width = 700;  /* 800 - 100 pixels for left/right margins */
      int max_height = 460; /* 600 - 140 pixels (120 top + 20 bottom margins) */

      /* Calculate scale factors for both dimensions to fit the image */
      float scale_x = (float)max_width / surface->w;
      float scale_y = (float)max_height / surface->h;

      /* Use the smaller scale to ensure the image fits in both dimensions */
      float scale = (scale_x < scale_y) ? scale_x : scale_y;

      /* Don't scale up small images, only scale down large ones */
      if (scale > 1.0f)
        scale = 1.0f;

      /* Calculate final display dimensions */
      int display_width = (int)(surface->w * scale);
      int display_height = (int)(surface->h * scale);

      /* Center the image horizontally within the available area */
      int x_pos = 50 + (max_width - display_width) / 2;

      /* Render the image at the calculated position and size */
      SDL_Rect r = {x_pos, 120, display_width, display_height};
      SDL_RenderCopy(renderer, tex, NULL, &r);

      /* Clean up the temporary texture */
      SDL_DestroyTexture(tex);
    }
  }

  /* Present the rendered content to the screen */
  SDL_RenderPresent(renderer);
}

/**
 * Handle SDL events (quit, mouse clicks, etc.)
 *
 * This function processes all pending SDL events, including window close
 * events and mouse button clicks. Currently implements a placeholder
 * file picker response.
 *
 * @param running Pointer to the running flag (set to 0 to quit)
 */
void ui_handle_events(int *running) {
  /* Safety check */
  if (!running)
    return;

  SDL_Event e;

  /* Process all pending events in the queue */
  while (SDL_PollEvent(&e)) {
    /* Handle window close button */
    if (e.type == SDL_QUIT) {
      *running = 0;
    }

    /* Handle mouse button clicks */
    if (e.type == SDL_MOUSEBUTTONDOWN) {
      int mx = e.button.x;
      int my = e.button.y;

      /* Check if the browse button area was clicked */
      if (mx >= browse_btn_rect.x &&
          mx < browse_btn_rect.x + browse_btn_rect.w &&
          my >= browse_btn_rect.y &&
          my < browse_btn_rect.y + browse_btn_rect.h) {
        /* File picker is not implemented in this version */
        printf("File picker not implemented.\n");
        printf("Usage: ./ui_app <image.png>\n");
      }
    }
  }
}
