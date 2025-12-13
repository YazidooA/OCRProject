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

char filepath[512] = "input.png";

/* Action IDs for buttons */
typedef enum {
  ACTION_RESET,
  ACTION_ROTATE,
  ACTION_GRAYSCALE,
  ACTION_OTSU,
  ACTION_DENOISE,
  ACTION_SAVE,
  ACTION_AUTO_PROCESS,
  ACTION_SOLVE_GRID,
#ifdef USE_FILE_PICKER
  ACTION_OPEN_FILE
#endif
} ActionType;

/* Button structure */
typedef struct {
  SDL_Rect rect;
  const char *label;
  SDL_Color color;
  SDL_Color hover_color;
  ActionType action;
} Button;

/* Check if point is inside rectangle */
int point_in_rect(int x, int y, SDL_Rect *rect) {
  return (x >= rect->x && x < rect->x + rect->w && y >= rect->y &&
          y < rect->y + rect->h);
}

/* Render button with text */
void render_button(SDL_Renderer *renderer, Button *btn, TTF_Font *font,
                   int is_hover) {
  if (!renderer || !btn)
    return;

  /* Button background */
  SDL_Color *color = is_hover ? &btn->hover_color : &btn->color;
  SDL_SetRenderDrawColor(renderer, color->r, color->g, color->b, color->a);
  SDL_RenderFillRect(renderer, &btn->rect);

  /* Button border */
  SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
  SDL_RenderDrawRect(renderer, &btn->rect);

  /* Button text */
  if (font && btn->label) {
    SDL_Color text_color = {255, 255, 255, 255};
    SDL_Surface *text_surface =
        TTF_RenderText_Blended(font, btn->label, text_color);

    if (!text_surface) {
      text_surface = TTF_RenderText_Solid(font, btn->label, text_color);
    }

    if (text_surface) {
      SDL_Texture *text_texture =
          SDL_CreateTextureFromSurface(renderer, text_surface);
      if (text_texture) {
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

int main(int argc, char **argv) {
#ifdef USE_FILE_PICKER
  /* Si pas d'argument, ouvrir le sélecteur de fichiers */
  if (argc < 2) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      printf("SDL Init error: %s\n", SDL_GetError());
      return 1;
    }

    if (TTF_Init() < 0) {
      printf("TTF Init error: %s\n", TTF_GetError());
      SDL_Quit();
      return 1;
    }

    printf("Opening file picker...\n");
    if (show_file_picker(filepath, sizeof(filepath))) {
      printf("File selected: %s\n", filepath);
    } else {
      printf("No file selected, using default: %s\n", filepath);
    }

    TTF_Quit();
    SDL_Quit();
  } else {
    strncpy(filepath, argv[1], sizeof(filepath));
    filepath[sizeof(filepath) - 1] = '\0';
  }
#else
  if (argc >= 2) {
    strncpy(filepath, argv[1], sizeof(filepath));
    filepath[sizeof(filepath) - 1] = '\0';
  }
#endif

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL Init error: %s\n", SDL_GetError());
    return 1;
  }

  if (TTF_Init() < 0) {
    printf("TTF Init error: %s\n", TTF_GetError());
    SDL_Quit();
    return 1;
  }

  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);

  SDL_Window *win =
      SDL_CreateWindow("OCR Image Processor", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1100, 800, SDL_WINDOW_SHOWN);

  SDL_Renderer *renderer =
      SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

  /* Load font - Fedora paths */
  const char *font_paths[] = {
      "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf",
      "/usr/share/fonts/google-droid-sans-fonts/DroidSans.ttf",
      "/usr/share/fonts/open-sans/OpenSans-Regular.ttf",
      "/usr/share/fonts/adwaita-sans-fonts/AdwaitaSans-Regular.ttf", NULL};

  TTF_Font *font = NULL;
  for (int i = 0; font_paths[i] != NULL; i++) {
    font = TTF_OpenFont(font_paths[i], 16);
    if (font) {
      printf("✓ Loaded font: %s\n", font_paths[i]);
      break;
    }
  }

  if (!font) {
    printf("ERROR: Could not load any font!\n");
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
  }

  /* Initialize image_data */
  struct image_data data;
  fill_data(&data, filepath);

  /* Load surface */
  SDL_Surface *surface = NULL;
  load_in_surface(&data, &surface);

  if (!surface) {
    printf("Error loading image\n");
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
  }

  /* Create texture from surface */
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

  /* Define buttons (right side panel) */
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
       "Otsu (H)",
       {184, 134, 11, 255},
       {204, 154, 31, 255},
       ACTION_OTSU},
      {{850, 320, 200, 50},
       "Grayscale (G)",
       {105, 105, 105, 255},
       {125, 125, 125, 255},
       ACTION_GRAYSCALE},
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
       "Otsu (H)",
       {184, 134, 11, 255},
       {204, 154, 31, 255},
       ACTION_OTSU},
      {{850, 260, 200, 50},
       "Grayscale (G)",
       {105, 105, 105, 255},
       {125, 125, 125, 255},
       ACTION_GRAYSCALE},
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
         "(Grayscale→Otsu→Rotate→Denoise)\n");
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
            printf("[1/4] Converting to grayscale...\n");
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
            printf("[2/4] Applying Otsu thresholding...\n");
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
            printf("[3/4] Auto-rotating image...\n");
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
            printf("[4/4] Applying noise removal...\n");
            apply_noise_removal(surface, 2);
            save_surface(&data, surface, "auto_4_denoise_FINAL");
            SDL_DestroyTexture(texture);
            texture = SDL_CreateTextureFromSurface(renderer, surface);

            printf("=== Auto Processing Complete! ===\n");
            printf("Final image saved as: auto_4_denoise_FINAL.png\n\n");
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

          printf("[1/4] Converting to grayscale...\n");
          convert_to_grayscale(surface);
          save_surface(&data, surface, "auto_1_grayscale");
          SDL_DestroyTexture(texture);
          texture = SDL_CreateTextureFromSurface(renderer, surface);

          printf("[2/4] Applying Otsu thresholding...\n");
          apply_otsu_thresholding(surface);
          save_surface(&data, surface, "auto_2_otsu");
          SDL_DestroyTexture(texture);
          texture = SDL_CreateTextureFromSurface(renderer, surface);

          printf("[3/4] Auto-rotating image...\n");
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

          printf("[4/4] Applying noise removal...\n");
          apply_noise_removal(surface, 2);
          save_surface(&data, surface, "auto_4_denoise_FINAL");
          SDL_DestroyTexture(texture);
          texture = SDL_CreateTextureFromSurface(renderer, surface);

          printf("=== Auto Processing Complete! ===\n");
          printf("Final image saved as: auto_4_denoise_FINAL.png\n\n");
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