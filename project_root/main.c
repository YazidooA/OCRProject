/*
 * main.c - OCR Image Processor Application
 *
 * This is the main application file for an interactive OCR (Optical Character
 * Recognition) image processing tool. It provides a graphical interface with
 * buttons for various image processing operations including grayscale
 * conversion, Otsu thresholding, rotation correction, noise removal, and
 * crossword grid solving.
 *
 * Key Features:
 * - Interactive UI with SDL2
 * - File picker for image selection (when compiled with USE_FILE_PICKER)
 * - Multiple image processing operations
 * - Automatic processing pipeline
 * - Real-time preview of processing results
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "image_cleaner/image_cleaner.h"
#include "pipeline_interface/pipeline_interface.h"
#include "rotation/rotation.h"
#include "setup_image/setup_image.h"

#ifdef USE_FILE_PICKER
#include "file_picker/file_picker.h"
#endif

/* Global variable storing the current image file path */
char filepath[512] = "input.png";

/**
 * Enumeration of all available button actions in the UI
 */
typedef enum {
  ACTION_RESET,        /* Reload the original image */
  ACTION_ROTATE,       /* Auto-rotate/deskew the image */
  ACTION_GRAYSCALE,    /* Convert to grayscale */
  ACTION_OTSU,         /* Apply Otsu thresholding */
  ACTION_DENOISE,      /* Remove noise from the image */
  ACTION_SAVE,         /* Save the current image */
  ACTION_AUTO_PROCESS, /* Run the full processing pipeline */
  ACTION_SOLVE_GRID,   /* Detect and solve crossword grid */
#ifdef USE_FILE_PICKER
  ACTION_OPEN_FILE /* Open file picker dialog */
#endif
} ActionType;

/**
 * Structure representing a clickable button in the UI
 */
typedef struct {
  SDL_Rect rect;         /* Position and size of the button */
  const char *label;     /* Text label to display */
  SDL_Color color;       /* Normal button color */
  SDL_Color hover_color; /* Color when mouse hovers over button */
  ActionType action;     /* Action to perform when clicked */
} Button;

/**
 * Check if a point (x, y) is inside a rectangle
 *
 * Used for hit testing to determine if the mouse cursor is over a button.
 *
 * @param x    X coordinate of the point
 * @param y    Y coordinate of the point
 * @param rect Pointer to the rectangle to test
 * @return     1 if point is inside rectangle, 0 otherwise
 */
int point_in_rect(int x, int y, SDL_Rect *rect) {
  return (x >= rect->x && x < rect->x + rect->w && y >= rect->y &&
          y < rect->y + rect->h);
}

/**
 * Render a button on the screen with its label
 *
 * Draws a colored rectangle with text centered inside. The button color changes
 * when the mouse hovers over it to provide visual feedback.
 *
 * @param renderer SDL renderer to draw with
 * @param btn      Pointer to the button structure  to render
 * @param font     TTF font to use for the button label
 * @param is_hover 1 if mouse is hovering over button, 0 otherwise
 */
void render_button(SDL_Renderer *renderer, Button *btn, TTF_Font *font,
                   int is_hover) {
  /* Safety check */
  if (!renderer || !btn)
    return;

  /* Draw button background (use hover color if mouse is over button) */
  SDL_Color *color = is_hover ? &btn->hover_color : &btn->color;
  SDL_SetRenderDrawColor(renderer, color->r, color->g, color->b, color->a);
  SDL_RenderFillRect(renderer, &btn->rect);

  /* Draw button border for better visual separation */
  SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
  SDL_RenderDrawRect(renderer, &btn->rect);

  /* Render the button label text */
  if (font && btn->label) {
    SDL_Color text_color = {255, 255, 255, 255}; /* White text */

    /* Try anti-aliased blended rendering first (higher quality) */
    SDL_Surface *text_surface =
        TTF_RenderText_Blended(font, btn->label, text_color);

    /* Fall back to solid rendering if blended fails */
    if (!text_surface) {
      text_surface = TTF_RenderText_Solid(font, btn->label, text_color);
    }

    if (text_surface) {
      /* Convert text surface to texture for rendering */
      SDL_Texture *text_texture =
          SDL_CreateTextureFromSurface(renderer, text_surface);
      if (text_texture) {
        /* Center the text within the button */
        SDL_Rect text_rect = {btn->rect.x + (btn->rect.w - text_surface->w) / 2,
                              btn->rect.y + (btn->rect.h - text_surface->h) / 2,
                              text_surface->w, text_surface->h};
        SDL_RenderCopy(renderer, text_texture, NULL, &text_rect);
        SDL_DestroyTexture(text_texture);
      }
      SDL_FreeSurface(text_surface);
    }
  }
}

/**
 * Main application entry point
 *
 * Initializes SDL, creates the main window, loads the image, and runs the main
 * event loop. Handles user interactions through buttons and keyboard shortcuts
 * to perform various image processing operations.
 *
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return     0 on success, 1 on error
 */
int main(int argc, char **argv) {
#ifdef USE_FILE_PICKER
  /* If no command-line argument provided, open file picker */
  if (argc < 2) {
    /* Initialize SDL for file picker */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      printf("SDL Init error: %s\n", SDL_GetError());
      return 1;
    }

    /* Initialize SDL_ttf for text rendering in file picker */
    if (TTF_Init() < 0) {
      printf("TTF Init error: %s\n", TTF_GetError());
      SDL_Quit();
      return 1;
    }

    /* Open file picker dialog to select an image */
    printf("Opening file picker...\n");
    if (show_file_picker(filepath, sizeof(filepath))) {
      printf("File selected: %s\n", filepath);
    } else {
      printf("No file selected, using default: %s\n", filepath);
    }

    /* Clean up file picker resources */
    TTF_Quit();
    SDL_Quit();
  } else {
    /* Use image path from command-line argument */
    strncpy(filepath, argv[1], sizeof(filepath));
    filepath[sizeof(filepath) - 1] = '\0'; /* Ensure null termination */
  }
#else
  /* Without file picker, only accept command-line argument */
  if (argc >= 2) {
    strncpy(filepath, argv[1], sizeof(filepath));
    filepath[sizeof(filepath) - 1] = '\0';
  }
#endif

  /* ===== Initialize SDL and create main window ===== */

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL Init error: %s\n", SDL_GetError());
    return 1;
  }

  /* Initialize SDL_ttf for text rendering */
  if (TTF_Init() < 0) {
    printf("TTF Init error: %s\n", TTF_GetError());
    SDL_Quit();
    return 1;
  }

  /* Initialize SDL_image for PNG and JPG loading */
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);

  /* Create the main application window (1100x800 pixels) */
  SDL_Window *win =
      SDL_CreateWindow("OCR Image Processor", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1100, 800, SDL_WINDOW_SHOWN);

  /* Create hardware-accelerated renderer for drawing */
  SDL_Renderer *renderer =
      SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

  /* Try to load a font from common Linux font paths */
  const char *font_paths[] = {
      "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf", /* Fedora
                                                                            */
      "/usr/share/fonts/google-droid-sans-fonts/DroidSans.ttf",
      "/usr/share/fonts/open-sans/OpenSans-Regular.ttf",
      "/usr/share/fonts/adwaita-sans-fonts/AdwaitaSans-Regular.ttf", NULL};

  TTF_Font *font = NULL;
  for (int i = 0; font_paths[i] != NULL; i++) {
    font = TTF_OpenFont(font_paths[i], 16); /* 16-point font size */
    if (font) {
      printf("✓ Loaded font: %s\n", font_paths[i]);
      break;
    }
  }

  /* Error handling if no font could be loaded */
  if (!font) {
    printf("ERROR: Could not load any font!\n");
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
  }

  /* ===== Load the input image ===== */

  /* Initialize image data structure with file path */
  struct image_data data;
  fill_data(&data, filepath);

  /* Load the image file into an SDL surface */
  SDL_Surface *surface = NULL;
  load_in_surface(&data, &surface);

  /* Error handling if image failed to load */
  if (!surface) {
    printf("Error loading image\n");
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
  }

  /* Create a GPU texture from the loaded surface for faster rendering */
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

  /* ===== Define UI Buttons ===== */
  /* Button layout differs based on whether file picker is enabled */
  Button buttons[] = {
#ifdef USE_FILE_PICKER
      {{850, 30, 200, 50},
       "Open File (O)",
       {100, 149, 237, 255},
       {120, 169, 255, 255},
       ACTION_OPEN_FILE},
      {{850, 100, 200, 60},
       "Auto Process (A)",
       {255, 69, 0, 255},
       {255, 99, 30, 255},
       ACTION_AUTO_PROCESS},
      {{850, 180, 200, 50},
       "Reset (C)",
       {70, 130, 180, 255},
       {90, 150, 200, 255},
       ACTION_RESET},
      {{850, 250, 200, 50},
       "Grayscale (G)",
       {105, 105, 105, 255},
       {125, 125, 125, 255},
       ACTION_GRAYSCALE},
      {{850, 320, 200, 50},
       "Otsu (H)",
       {184, 134, 11, 255},
       {204, 154, 31, 255},
       ACTION_OTSU},
      {{850, 390, 200, 50},
       "Rotate (R)",
       {34, 139, 34, 255},
       {54, 159, 54, 255},
       ACTION_ROTATE},
      {{850, 460, 200, 50},
       "Denoise (J)",
       {128, 0, 128, 255},
       {148, 20, 148, 255},
       ACTION_DENOISE},
      {{850, 530, 200, 50},
       "Save (Ctrl+S)",
       {220, 20, 60, 255},
       {240, 40, 80, 255},
       ACTION_SAVE},
      {{850, 600, 200, 50},
       "Solve Grid (V)",
       {0, 128, 128, 255},
       {0, 148, 148, 255},
       ACTION_SOLVE_GRID},
#else
      {{850, 30, 200, 70},
       "Auto Process (A)",
       {255, 69, 0, 255},
       {255, 99, 30, 255},
       ACTION_AUTO_PROCESS},
      {{850, 120, 200, 50},
       "Reset (C)",
       {70, 130, 180, 255},
       {90, 150, 200, 255},
       ACTION_RESET},
      {{850, 190, 200, 50},
       "Grayscale (G)",
       {105, 105, 105, 255},
       {125, 125, 125, 255},
       ACTION_GRAYSCALE},
      {{850, 260, 200, 50},
       "Otf//su (H)",
       {184, 134, 11, 255},
       {204, 154, 31, 255},
       ACTION_OTSU},
      {{850, 330, 200, 50},
       "Rotate (R)",
       {34, 139, 34, 255},
       {54, 159, 54, 255},
       ACTION_ROTATE},
      {{850, 400, 200, 50},
       "Denoise (J)",
       {128, 0, 128, 255},
       {148, 20, 148, 255},
       ACTION_DENOISE},
      {{850, 470, 200, 50},
       "Save (Ctrl+S)",
       {220, 20, 60, 255},
       {240, 40, 80, 255},
       ACTION_SAVE},
      {{850, 540, 200, 60},
       "Solve Grid (V)",
       {0, 128, 128, 255},
       {0, 148, 148, 255},
       ACTION_SOLVE_GRID},
#endif
  };
  int num_buttons = sizeof(buttons) / sizeof(buttons[0]);

  /* Print usage instructions */
  printf("\n=== OCR Image Processor ===\n");
  printf("Image loaded: %s\n", filepath);
  printf("\nClick buttons or use keyboard shortcuts:\n");
#ifdef USE_FILE_PICKER
  printf("  O / Open File         - Select a new file\n");
#endif
  printf("  A / Auto Process      - Apply all steps "
         "(Grayscale→Otsu→Rotate→Denoise→Solve Grid)\n");
  printf("  C / Reset button      - Reload original image\n");
  printf("  H / Otsu button       - Apply Otsu thresholding\n");
  printf("  G / Grayscale button  - Convert to grayscale\n");
  printf("  R / Rotate button     - Auto-rotate/deskew\n");
  printf("  J / Denoise button    - Remove noise\n");
  printf("  Ctrl+S / Save button  - Save current image\n");
  printf("  V / Solve Grid        - Detect and solve crossword grid\n");
  printf("  ESC/Q                 - Quit\n");
  printf("=============================\n\n");

  /* Event loop variables */
  int running = 1;
  SDL_Event e;
  int mouse_x = 0, mouse_y = 0;
  int hover_button = -1;
  int ctrl_pressed = 0;

  while (running) {
    /* Get mouse position for hover effect */
    SDL_GetMouseState(&mouse_x, &mouse_y);
    hover_button = -1;
    for (int i = 0; i < num_buttons; i++) {
      if (point_in_rect(mouse_x, mouse_y, &buttons[i].rect)) {
        hover_button = i;
        break;
      }
    }

    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        running = 0;
      }
      /* Mouse clicks */
      else if (e.type == SDL_MOUSEBUTTONDOWN &&
               e.button.button == SDL_BUTTON_LEFT) {
        int clicked_button = -1;
        for (int i = 0; i < num_buttons; i++) {
          if (point_in_rect(e.button.x, e.button.y, &buttons[i].rect)) {
            clicked_button = i;
            break;
          }
        }

        /* Handle button actions */
        if (clicked_button >= 0) {
          ActionType action = buttons[clicked_button].action;

          switch (action) {
#ifdef USE_FILE_PICKER
          case ACTION_OPEN_FILE: {
            printf("Opening file picker...\n");
            char new_filepath[512];
            if (show_file_picker(new_filepath, sizeof(new_filepath))) {
              printf("New file selected: %s\n", new_filepath);
              strncpy(filepath, new_filepath, sizeof(filepath));
              filepath[sizeof(filepath) - 1] = '\0';

              /* Reload image */
              SDL_FreeSurface(surface);
              fill_data(&data, filepath);
              surface = NULL;
              load_in_surface(&data, &surface);

              if (surface) {
                SDL_DestroyTexture(texture);
                texture = SDL_CreateTextureFromSurface(renderer, surface);
              }
            } else {
              printf("File selection cancelled\n");
            }
            break;
          }
#endif
          case ACTION_AUTO_PROCESS:
            printf("\n=== Starting Auto Processing ===\n");

            // Step 1: Grayscale
            printf("[1/5] Converting to grayscale...\n");
            convert_to_grayscale(surface);
            save_surface(&data, surface, "auto_1_grayscale");
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_RenderClear(renderer);
            SDL_Rect img_rect = {10, 10, 820, 780};
            SDL_RenderCopy(renderer, texture, NULL, &img_rect);
            for (int i = 0; i < num_buttons; i++) {
              render_button(renderer, &buttons[i], font, (i == hover_button));
            }
            SDL_RenderPresent(renderer);
            SDL_Delay(300);

            // Step 2: Otsu Thresholding
            printf("[2/5] Applying Otsu thresholding...\n");
            apply_otsu_thresholding(surface);
            save_surface(&data, surface, "auto_2_otsu");
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, &img_rect);
            for (int i = 0; i < num_buttons; i++) {
              render_button(renderer, &buttons[i], font, (i == hover_button));
            }
            SDL_RenderPresent(renderer);
            SDL_Delay(300);

            // Step 3: Rotate
            printf("[3/5] Auto-rotating image...\n");
            double angle = auto_deskew_correction(surface);
            printf("        Detected angle: %.2f degrees\n", angle);
            SDL_Surface *rot = rotate(surface, angle);
            if (rot) {
              SDL_FreeSurface(surface);
              surface = rot;
              save_surface(&data, surface, "auto_3_rotation");
              SDL_DestroyTexture(texture);
              texture = SDL_CreateTextureFromSurface(renderer, surface);
              SDL_RenderClear(renderer);
              SDL_RenderCopy(renderer, texture, NULL, &img_rect);
              for (int i = 0; i < num_buttons; i++) {
                render_button(renderer, &buttons[i], font, (i == hover_button));
              }
              SDL_RenderPresent(renderer);
              SDL_Delay(300);
            }

            // Step 4: Denoise
            printf("[4/5] Applying noise removal...\n");
            apply_noise_removal(surface, 2);
            save_surface(&data, surface, "auto_4_denoise_FINAL");
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, &img_rect);
            for (int i = 0; i < num_buttons; i++) {
              render_button(renderer, &buttons[i], font, (i == hover_button));
            }
            SDL_RenderPresent(renderer);
            SDL_Delay(300);

            // Step 5: Solve Grid
            printf("[5/5] Solving crossword grid...\n");

            // Display "Résolution en cours..." message
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
            SDL_Rect overlay = {200, 350, 450, 80};
            SDL_RenderFillRect(renderer, &overlay);

            SDL_Color white = {255, 255, 255, 255};
            SDL_Surface *text_surf =
                TTF_RenderUTF8_Blended(font, "Résolution en cours...", white);
            if (text_surf) {
              SDL_Texture *text_tex =
                  SDL_CreateTextureFromSurface(renderer, text_surf);
              if (text_tex) {
                SDL_Rect text_rect = {425 - text_surf->w / 2, 370, text_surf->w,
                                      text_surf->h};
                SDL_RenderCopy(renderer, text_tex, NULL, &text_rect);
                SDL_DestroyTexture(text_tex);
              }
              SDL_FreeSurface(text_surf);
            }
            SDL_RenderPresent(renderer);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            SDL_Surface *result = pipeline(surface, renderer);
            if (result) {
              printf("Pipeline completed successfully!\n");
              SDL_Surface *result_image = IMG_Load("result.png");
              if (result_image) {
                SDL_FreeSurface(surface);
                surface = result_image;
                SDL_DestroyTexture(texture);
                texture = SDL_CreateTextureFromSurface(renderer, surface);
                printf("✓ Annotated image loaded!\n");
              }
            } else {
              printf("Warning: Pipeline failed\n");
            }

            printf("=== Auto Processing Complete! ===\n");
            printf("Final result saved as: result.png\n\n");
            break;

          case ACTION_RESET:
            printf("Resetting to original image...\n");
            SDL_FreeSurface(surface);
            surface = NULL;
            load_in_surface(&data, &surface);
            if (surface) {
              SDL_DestroyTexture(texture);
              texture = SDL_CreateTextureFromSurface(renderer, surface);
            }
            break;

          case ACTION_ROTATE: {
            printf("Auto-rotating image...\n");
            double angle = auto_deskew_correction(surface);
            printf("Detected angle: %.2f degrees\n", angle);
            SDL_Surface *rot = rotate(surface, angle);
            if (rot) {
              SDL_FreeSurface(surface);
              surface = rot;
              save_surface(&data, surface, "rotation");
              SDL_DestroyTexture(texture);
              texture = SDL_CreateTextureFromSurface(renderer, surface);
            }
            break;
          }

          case ACTION_GRAYSCALE:
            printf("Converting to grayscale...\n");
            convert_to_grayscale(surface);
            save_surface(&data, surface, "grayscale");
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            break;

          case ACTION_OTSU:
            printf("Applying Otsu thresholding...\n");
            apply_otsu_thresholding(surface);
            save_surface(&data, surface, "otsu_thresholding");
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            break;

          case ACTION_DENOISE:
            printf("Applying noise removal...\n");
            apply_noise_removal(surface, 2);
            save_surface(&data, surface, "noise_removal");
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            break;

          case ACTION_SAVE:
            printf("Saving output...\n");
            save_surface(&data, surface, "output");
            break;

          case ACTION_SOLVE_GRID: {
            printf("\n=== Starting Grid Resolution with Pipeline ===\n");

            // Afficher un message "Résolution en cours..." à l'écran
            SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
            SDL_RenderClear(renderer);

            // Afficher l'image actuelle
            if (texture) {
              SDL_Rect img_rect = {10, 10, 820, 780};
              SDL_RenderCopy(renderer, texture, NULL, &img_rect);
            }

            // Dessiner un rectangle semi-transparent
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
            SDL_Rect overlay = {200, 350, 450, 80};
            SDL_RenderFillRect(renderer, &overlay);

            // Texte "Résolution en cours..."
            SDL_Color white = {255, 255, 255, 255};
            SDL_Surface *text_surf =
                TTF_RenderUTF8_Blended(font, "Résolution en cours...", white);
            if (text_surf) {
              SDL_Texture *text_tex =
                  SDL_CreateTextureFromSurface(renderer, text_surf);
              if (text_tex) {
                SDL_Rect text_rect = {425 - text_surf->w / 2, 370, text_surf->w,
                                      text_surf->h};
                SDL_RenderCopy(renderer, text_tex, NULL, &text_rect);
                SDL_DestroyTexture(text_tex);
              }
              SDL_FreeSurface(text_surf);
            }

            SDL_RenderPresent(renderer);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            SDL_Surface *result = pipeline(surface, renderer);

            if (result) {
              printf("Pipeline completed successfully!\n");
              printf("=== Grid Resolution Complete! ===\n");
              printf("Results saved to:\n");
              printf("  - result.png (annotated image)\n");
              printf("  - grid (text file with grid + words)\n");
              printf("  - tile_debug.bmp (debug tile)\n\n");

              SDL_Surface *result_image = IMG_Load("result.png");
              if (result_image) {
                SDL_FreeSurface(surface);
                surface = result_image;
                SDL_DestroyTexture(texture);
                texture = SDL_CreateTextureFromSurface(renderer, surface);
                printf("✓ Annotated image loaded into interface!\n");
              } else {
                fprintf(stderr, "Warning: Could not load result.png: %s\n",
                        IMG_GetError());
                SDL_DestroyTexture(texture);
                texture = SDL_CreateTextureFromSurface(renderer, surface);
              }
            } else {
              printf("ERROR: Pipeline failed\n");
            }
            break;
          }
          }
        }
      }
      /* Keyboard events */
      else if (e.type == SDL_KEYDOWN) {
        /* Track Ctrl key */
        if (e.key.keysym.sym == SDLK_LCTRL || e.key.keysym.sym == SDLK_RCTRL) {
          ctrl_pressed = 1;
        }

        switch (e.key.keysym.sym) {
        case SDLK_ESCAPE:
        case SDLK_q:
          running = 0;
          break;

#ifdef USE_FILE_PICKER
        case SDLK_o: {
          printf("Opening file picker...\n");
          char new_filepath[512];
          if (show_file_picker(new_filepath, sizeof(new_filepath))) {
            printf("New file selected: %s\n", new_filepath);
            strncpy(filepath, new_filepath, sizeof(filepath));
            filepath[sizeof(filepath) - 1] = '\0';

            SDL_FreeSurface(surface);
            fill_data(&data, filepath);
            surface = NULL;
            load_in_surface(&data, &surface);

            if (surface) {
              SDL_DestroyTexture(texture);
              texture = SDL_CreateTextureFromSurface(renderer, surface);
            }
          } else {
            printf("File selection cancelled\n");
          }
          break;
        }
#endif

        case SDLK_a: {
          printf("\n=== Starting Auto Processing ===\n");

          printf("[1/5] Converting to grayscale...\n");
          convert_to_grayscale(surface);
          save_surface(&data, surface, "auto_1_grayscale");
          SDL_DestroyTexture(texture);
          texture = SDL_CreateTextureFromSurface(renderer, surface);

          printf("[2/5] Applying Otsu thresholding...\n");
          apply_otsu_thresholding(surface);
          save_surface(&data, surface, "auto_2_otsu");
          SDL_DestroyTexture(texture);
          texture = SDL_CreateTextureFromSurface(renderer, surface);

          printf("[3/5] Auto-rotating image...\n");
          double angle = auto_deskew_correction(surface);
          printf("        Detected angle: %.2f degrees\n", angle);
          SDL_Surface *rot = rotate(surface, angle);
          if (rot) {
            SDL_FreeSurface(surface);
            surface = rot;
            save_surface(&data, surface, "auto_3_rotation");
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
          }

          printf("[4/5] Applying noise removal...\n");
          apply_noise_removal(surface, 2);
          save_surface(&data, surface, "auto_4_denoise_FINAL");
          SDL_DestroyTexture(texture);
          texture = SDL_CreateTextureFromSurface(renderer, surface);

          printf("[5/5] Solving crossword grid...\n");

          // Display "Résolution en cours..." message
          SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
          SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
          SDL_Rect overlay = {200, 350, 450, 80};
          SDL_RenderFillRect(renderer, &overlay);

          SDL_Color white = {255, 255, 255, 255};
          SDL_Surface *text_surf =
              TTF_RenderUTF8_Blended(font, "Résolution en cours...", white);
          if (text_surf) {
            SDL_Texture *text_tex =
                SDL_CreateTextureFromSurface(renderer, text_surf);
            if (text_tex) {
              SDL_Rect text_rect = {425 - text_surf->w / 2, 370, text_surf->w,
                                    text_surf->h};
              SDL_RenderCopy(renderer, text_tex, NULL, &text_rect);
              SDL_DestroyTexture(text_tex);
            }
            SDL_FreeSurface(text_surf);
          }
          SDL_RenderPresent(renderer);
          SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

          SDL_Surface *result = pipeline(surface, renderer);
          if (result) {
            printf("Pipeline completed successfully!\n");
            SDL_Surface *result_image = IMG_Load("result.png");
            if (result_image) {
              SDL_FreeSurface(surface);
              surface = result_image;
              SDL_DestroyTexture(texture);
              texture = SDL_CreateTextureFromSurface(renderer, surface);
              printf("✓ Annotated image loaded!\n");
            }
          } else {
            printf("Warning: Pipeline failed\n");
          }

          printf("=== Auto Processing Complete! ===\n");
          printf("Final result saved as: result.png\n\n");
          break;
        }

        case SDLK_c:
          printf("Resetting to original image...\n");
          SDL_FreeSurface(surface);
          surface = NULL;
          load_in_surface(&data, &surface);
          if (surface) {
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
          }
          break;

        case SDLK_r: {
          printf("Auto-rotating image...\n");
          double angle = auto_deskew_correction(surface);
          printf("Detected angle: %.2f degrees\n", angle);
          SDL_Surface *rot = rotate(surface, angle);
          if (rot) {
            SDL_FreeSurface(surface);
            surface = rot;
            save_surface(&data, surface, "rotation");
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTextureFromSurface(renderer, surface);
          }
          break;
        }

        case SDLK_g:
          printf("Applying grayscale...\n");
          convert_to_grayscale(surface);
          save_surface(&data, surface, "grayscale");
          SDL_DestroyTexture(texture);
          texture = SDL_CreateTextureFromSurface(renderer, surface);
          break;

        case SDLK_h:
          printf("Applying Otsu thresholding...\n");
          apply_otsu_thresholding(surface);
          save_surface(&data, surface, "otsu_thresholding");
          SDL_DestroyTexture(texture);
          texture = SDL_CreateTextureFromSurface(renderer, surface);
          break;

        case SDLK_j:
          printf("Applying noise removal...\n");
          apply_noise_removal(surface, 2);
          save_surface(&data, surface, "noise_removal");
          SDL_DestroyTexture(texture);
          texture = SDL_CreateTextureFromSurface(renderer, surface);
          break;

        case SDLK_v: {
          printf("\n=== Starting Grid Resolution with Pipeline ===\n");

          // Afficher un message "Résolution en cours..." à l'écran
          SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
          SDL_RenderClear(renderer);

          if (texture) {
            SDL_Rect img_rect = {10, 10, 820, 780};
            SDL_RenderCopy(renderer, texture, NULL, &img_rect);
          }

          SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
          SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
          SDL_Rect overlay = {200, 350, 450, 80};
          SDL_RenderFillRect(renderer, &overlay);

          SDL_Color white = {255, 255, 255, 255};
          SDL_Surface *text_surf =
              TTF_RenderUTF8_Blended(font, "Résolution en cours...", white);
          if (text_surf) {
            SDL_Texture *text_tex =
                SDL_CreateTextureFromSurface(renderer, text_surf);
            if (text_tex) {
              SDL_Rect text_rect = {425 - text_surf->w / 2, 370, text_surf->w,
                                    text_surf->h};
              SDL_RenderCopy(renderer, text_tex, NULL, &text_rect);
              SDL_DestroyTexture(text_tex);
            }
            SDL_FreeSurface(text_surf);
          }

          SDL_RenderPresent(renderer);
          SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

          SDL_Surface *result = pipeline(surface, renderer);

          if (result) {
            printf("Pipeline completed successfully!\n");
            printf("=== Grid Resolution Complete! ===\n");
            printf("Results saved to:\n");
            printf("  - result.png (annotated image)\n");
            printf("  - grid (text file with grid + words)\n");
            printf("  - tile_debug.bmp (debug tile)\n\n");

            SDL_Surface *result_image = IMG_Load("result.png");
            if (result_image) {
              SDL_FreeSurface(surface);
              surface = result_image;
              SDL_DestroyTexture(texture);
              texture = SDL_CreateTextureFromSurface(renderer, surface);
              printf("✓ Annotated image loaded into interface!\n");
            } else {
              fprintf(stderr, "Warning: Could not load result.png: %s\n",
                      IMG_GetError());
              SDL_DestroyTexture(texture);
              texture = SDL_CreateTextureFromSurface(renderer, surface);
            }
          } else {
            printf("ERROR: Pipeline failed\n");
          }
          break;
        }

        case SDLK_s:
          if (ctrl_pressed) {
            printf("Saving output...\n");
            save_surface(&data, surface, "output");
          }
          break;

        default:
          break;
        }
      } else if (e.type == SDL_KEYUP) {
        if (e.key.keysym.sym == SDLK_LCTRL || e.key.keysym.sym == SDLK_RCTRL) {
          ctrl_pressed = 0;
        }
      }
    }

    /* Render */
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_RenderClear(renderer);

    /* Render image (left side, scaled to fit) */
    if (texture) {
      SDL_Rect img_rect = {10, 10, 820, 780};
      SDL_RenderCopy(renderer, texture, NULL, &img_rect);
    }

    /* Render buttons */
    for (int i = 0; i < num_buttons; i++) {
      render_button(renderer, &buttons[i], font, (i == hover_button));
    }

    SDL_RenderPresent(renderer);
    SDL_Delay(16);
  }

  /* Cleanup */
  SDL_DestroyTexture(texture);
  SDL_FreeSurface(surface);
  TTF_CloseFont(font);
  TTF_Quit();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(win);
  IMG_Quit();
  SDL_Quit();
  return 0;
}