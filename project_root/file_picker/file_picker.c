#include "file_picker.h"
#include <SDL2/SDL_ttf.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define MAX_FILES 100
#define MAX_PATH 512

typedef struct {
  char name[256];
  int is_directory;
} FileEntry;

typedef struct {
  FileEntry files[MAX_FILES];
  int count;
  int scroll_offset;
  int selected_index;
  char current_path[MAX_PATH];
} FileBrowser;

static void list_directory(FileBrowser *browser, const char *path) {
  DIR *dir;
  struct dirent *entry;
  struct stat file_stat;
  char full_path[MAX_PATH];

  browser->count = 0;
  browser->scroll_offset = 0;
  browser->selected_index = 0;
  strncpy(browser->current_path, path, MAX_PATH - 1);

  // Ajouter l'entrée parent ".."
  if (strcmp(path, "/") != 0) {
    strcpy(browser->files[browser->count].name, "..");
    browser->files[browser->count].is_directory = 1;
    browser->count++;
  }

  dir = opendir(path);
  if (!dir)
    return;

  while ((entry = readdir(dir)) != NULL && browser->count < MAX_FILES) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    snprintf(full_path, MAX_PATH, "%s/%s", path, entry->d_name);

    if (stat(full_path, &file_stat) == 0) {
      strncpy(browser->files[browser->count].name, entry->d_name, 255);
      browser->files[browser->count].is_directory = S_ISDIR(file_stat.st_mode);
      browser->count++;
    }
  }

  closedir(dir);
}

static void navigate_to(FileBrowser *browser, const char *name) {
  char new_path[MAX_PATH];

  if (strcmp(name, "..") == 0) {
    // Remonter d'un niveau
    char *last_slash = strrchr(browser->current_path, '/');
    if (last_slash && last_slash != browser->current_path) {
      *last_slash = '\0';
    } else {
      strcpy(browser->current_path, "/");
    }
    list_directory(browser, browser->current_path);
  } else {
    snprintf(new_path, MAX_PATH, "%s/%s", browser->current_path, name);
    list_directory(browser, new_path);
  }
}

static void draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text,
                      int x, int y, SDL_Color color) {
  SDL_Surface *surface = TTF_RenderText_Blended(font, text, color);
  if (!surface)
    return;

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (texture) {
    SDL_Rect dest = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
  }
  SDL_FreeSurface(surface);
}

int show_file_picker(char *selected_path, size_t path_size) {
  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;
  FileBrowser browser = {0};
  int result = 0;

  window = SDL_CreateWindow("Sélectionner un fichier", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT,
                            SDL_WINDOW_SHOWN);

  if (!window) {
    fprintf(stderr, "Erreur création fenêtre: %s\n", SDL_GetError());
    return 0;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    fprintf(stderr, "Erreur création renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    return 0;
  }

  // Chemins de polices pour Fedora/NixOS/Linux
  const char *
      font_paths[] = {"/usr/share/fonts/liberation-sans-fonts/"
                      "LiberationSans-Regular.ttf", // Fedora
                      "/usr/share/fonts/liberation-sans/"
                      "LiberationSans-Regular.ttf", // Ubuntu
                      "/run/current-system/sw/share/X11/fonts/TTF/"
                      "DejaVuSans.ttf", // NixOS
                      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", // Debian
                      "/usr/share/fonts/TTF/DejaVuSans.ttf",             // Arch
                      NULL};

  TTF_Font *font = NULL;
  for (int i = 0; font_paths[i] != NULL; i++) {
    font = TTF_OpenFont(font_paths[i], 16);
    if (font)
      break;
  }

  if (!font) {
    fprintf(stderr, "Impossible de charger une police\n");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return 0;
  }

  // Démarrer dans le répertoire courant
  char cwd[MAX_PATH];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    list_directory(&browser, cwd);
  } else {
    list_directory(&browser, ".");
  }

  SDL_Event event;
  int running = 1;
  int visible_items = 20;

  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = 0;
        result = 0;
      } else if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
          running = 0;
          result = 0;
          break;
        case SDLK_UP:
          if (browser.selected_index > 0) {
            browser.selected_index--;
            if (browser.selected_index < browser.scroll_offset) {
              browser.scroll_offset = browser.selected_index;
            }
          }
          break;
        case SDLK_DOWN:
          if (browser.selected_index < browser.count - 1) {
            browser.selected_index++;
            if (browser.selected_index >=
                browser.scroll_offset + visible_items) {
              browser.scroll_offset =
                  browser.selected_index - visible_items + 1;
            }
          }
          break;
        case SDLK_RETURN:
          if (browser.files[browser.selected_index].is_directory) {
            navigate_to(&browser, browser.files[browser.selected_index].name);
          } else {
            snprintf(selected_path, path_size, "%s/%s", browser.current_path,
                     browser.files[browser.selected_index].name);
            running = 0;
            result = 1;
          }
          break;
        }
      } else if (event.type == SDL_MOUSEWHEEL) {
        if (event.wheel.y > 0) {
          if (browser.scroll_offset > 0)
            browser.scroll_offset--;
        } else if (event.wheel.y < 0) {
          if (browser.scroll_offset < browser.count - visible_items) {
            browser.scroll_offset++;
          }
        }
      } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        int mouse_y = event.button.y;
        if (mouse_y > 50 && mouse_y < 550) {
          int clicked_index = (mouse_y - 50) / 25 + browser.scroll_offset;
          if (clicked_index < browser.count) {
            if (clicked_index == browser.selected_index) {
              // Double-clic simulé
              if (browser.files[clicked_index].is_directory) {
                navigate_to(&browser, browser.files[clicked_index].name);
              } else {
                snprintf(selected_path, path_size, "%s/%s",
                         browser.current_path,
                         browser.files[clicked_index].name);
                running = 0;
                result = 1;
              }
            }
            browser.selected_index = clicked_index;
          }
        }
      }
    }

    // Rendu
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    // En-tête avec chemin courant
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_Rect header = {10, 10, SCREEN_WIDTH - 20, 30};
    SDL_RenderFillRect(renderer, &header);

    SDL_Color white = {255, 255, 255, 255};
    draw_text(renderer, font, browser.current_path, 15, 15, white);

    // Liste des fichiers
    for (int i = 0;
         i < visible_items && (i + browser.scroll_offset) < browser.count;
         i++) {
      int index = i + browser.scroll_offset;
      int y = 50 + i * 25;

      // Fond de sélection
      if (index == browser.selected_index) {
        SDL_SetRenderDrawColor(renderer, 70, 120, 200, 255);
        SDL_Rect select_rect = {10, y, SCREEN_WIDTH - 20, 23};
        SDL_RenderFillRect(renderer, &select_rect);
      }

      // Nom du fichier/dossier
      char display_name[300];
      if (browser.files[index].is_directory) {
        snprintf(display_name, 300, "[DIR] %s", browser.files[index].name);
      } else {
        strncpy(display_name, browser.files[index].name, 299);
      }

      SDL_Color text_color = {200, 200, 200, 255};
      draw_text(renderer, font, display_name, 15, y + 3, text_color);
    }

    // Instructions en bas
    SDL_Color gray = {150, 150, 150, 255};
    draw_text(renderer, font,
              "Flèches: naviguer | Entrée: sélectionner | Échap: annuler", 15,
              SCREEN_HEIGHT - 25, gray);

    SDL_RenderPresent(renderer);
    SDL_Delay(16);
  }

  TTF_CloseFont(font);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);

  return result;
}