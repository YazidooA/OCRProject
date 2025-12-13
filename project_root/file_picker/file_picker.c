/*
 * file_picker.c - Interactive file browser dialog
 *
 * This file implements a graphical file picker that allows users to navigate
 * the filesystem and select image files. It uses SDL2 for rendering and
 * keyboard/mouse input handling.
 */

#include "file_picker.h"
#include <SDL2/SDL_ttf.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Window dimensions for the file picker dialog */
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

/* Maximum number of files to display in the browser */
#define MAX_FILES 100

/* Maximum path length */
#define MAX_PATH 512

/**
 * Structure representing a single file or directory entry
 */
typedef struct {
  char name[256];   /* File or directory name */
  int is_directory; /* 1 if directory, 0 if file */
} FileEntry;

/**
 * Structure containing the state of the file browser
 */
typedef struct {
  FileEntry files[MAX_FILES];  /* Array of file/directory entries */
  int count;                   /* Number of entries in the list */
  int scroll_offset;           /* Current scroll position for navigation */
  int selected_index;          /* Index of the currently selected item */
  char current_path[MAX_PATH]; /* Current directory path */
} FileBrowser;

/**
 * List all files and directories in the specified path
 *
 * This function reads the contents of a directory and populates the browser
 * with file and directory entries. It also resets the scroll position and
 * selection to the top of the list.
 *
 * @param browser Pointer to the file browser state
 * @param path    Directory path to list
 */
static void list_directory(FileBrowser *browser, const char *path) {
  DIR *dir;
  struct dirent *entry;
  struct stat file_stat;
  char full_path[MAX_PATH];

  /* Reset browser state */
  browser->count = 0;
  browser->scroll_offset = 0;
  browser->selected_index = 0;
  strncpy(browser->current_path, path, MAX_PATH - 1);

  /* Add parent directory ".." entry (except for root directory) */
  if (strcmp(path, "/") != 0) {
    strcpy(browser->files[browser->count].name, "..");
    browser->files[browser->count].is_directory = 1;
    browser->count++;
  }

  /* Open the directory */
  dir = opendir(path);
  if (!dir)
    return;

  /* Read all entries in the directory */
  while ((entry = readdir(dir)) != NULL && browser->count < MAX_FILES) {
    /* Skip current (.) and parent (..) directory entries */
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    /* Build full path for stat() call */
    snprintf(full_path, MAX_PATH, "%s/%s", path, entry->d_name);

    /* Get file information to determine if it's a directory */
    if (stat(full_path, &file_stat) == 0) {
      strncpy(browser->files[browser->count].name, entry->d_name, 255);
      browser->files[browser->count].is_directory = S_ISDIR(file_stat.st_mode);
      browser->count++;
    }
  }

  closedir(dir);
}

/**
 * Navigate to a directory (either subdirectory or parent)
 *
 * Handles navigation when a directory is selected. If ".." is selected,
 * moves up one level in the directory hierarchy. Otherwise, enters the
 * selected subdirectory.
 *
 * @param browser Pointer to the file browser state
 * @param name    Name of the directory to navigate to (or ".." for parent)
 */
static void navigate_to(FileBrowser *browser, const char *name) {
  char new_path[MAX_PATH];

  if (strcmp(name, "..") == 0) {
    /* Navigate up one directory level */
    char *last_slash = strrchr(browser->current_path, '/');
    if (last_slash && last_slash != browser->current_path) {
      /* Remove the last path component */
      *last_slash = '\0';
    } else {
      /* Already at top level, go to root */
      strcpy(browser->current_path, "/");
    }
    list_directory(browser, browser->current_path);
  } else {
    /* Navigate into the selected subdirectory */
    snprintf(new_path, MAX_PATH, "%s/%s", browser->current_path, name);
    list_directory(browser, new_path);
  }
}

/**
 * Helper function to draw text on the screen
 *
 * Renders a text string using the specified font and color at the given
 * position. Uses SDL_ttf for text rendering.
 *
 * @param renderer SDL renderer to draw with
 * @param font     TTF font to use for rendering
 * @param text     Text string to display
 * @param x        X coordinate for text position
 * @param y        Y coordinate for text position
 * @param color    Color to render the text
 */
static void draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text,
                      int x, int y, SDL_Color color) {
  /* Render text to a surface using anti-aliased blending */
  SDL_Surface *surface = TTF_RenderText_Blended(font, text, color);
  if (!surface)
    return;

  /* Convert surface to texture for GPU rendering */
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (texture) {
    SDL_Rect dest = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
  }
  SDL_FreeSurface(surface);
}

/**
 * Display an interactive file picker dialog
 *
 * Opens a modal file browser window that allows the user to navigate the
 * filesystem and select a file. The user can use arrow keys or mouse to
 * navigate, Enter to select, and Escape to cancel.
 *
 * @param selected_path Buffer to store the selected file path
 * @param path_size     Size of the selected_path buffer
 * @return              1 if a file was selected, 0 if cancelled
 */
int show_file_picker(char *selected_path, size_t path_size) {
  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;
  FileBrowser browser = {0};
  int result = 0; /* Return value: 0 = cancelled, 1 = file selected */

  /* Create the file picker window */
  window = SDL_CreateWindow("Sélectionner un fichier", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT,
                            SDL_WINDOW_SHOWN);

  /* Error handling for window creation */
  if (!window) {
    fprintf(stderr, "Error creating window: %s\n", SDL_GetError());
    return 0;
  }

  /* Create hardware-accelerated renderer */
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    fprintf(stderr, "Error creating renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    return 0;
  }

  /* Font paths for various Linux distributions */
  const char *font_paths[] = {
      "/usr/share/fonts/liberation-sans-fonts/"
      "LiberationSans-Regular.ttf", // Fedora
      "/usr/share/fonts/liberation-sans/"
      "LiberationSans-Regular.ttf", // Ubuntu
      "/run/current-system/sw/share/X11/fonts/TTF/"
      "DejaVuSans.ttf",                                  // NixOS
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", // Debian
      "/usr/share/fonts/TTF/DejaVuSans.ttf",             // Arch
      NULL};

  /* Try to load a font from the available paths */
  TTF_Font *font = NULL;
  for (int i = 0; font_paths[i] != NULL; i++) {
    font = TTF_OpenFont(font_paths[i], 16);
    if (font)
      break;
  }

  /* Error handling if no font could be loaded */
  if (!font) {
    fprintf(stderr, "Unable to load any font\n");
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return 0;
  }

  /* Start in the current working directory */
  char cwd[MAX_PATH];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    list_directory(&browser, cwd);
  } else {
    /* Fallback to current directory if getcwd fails */
    list_directory(&browser, ".");
  }

  SDL_Event event;
  int running = 1;        /* Main loop control flag */
  int visible_items = 20; /* Number of file entries visible at once */

  /* Main event loop */
  while (running) {
    /* Process all pending events */
    while (SDL_PollEvent(&event)) {
      /* Handle window close button */
      if (event.type == SDL_QUIT) {
        running = 0;
        result = 0;
      }
      /* Handle keyboard input */
      else if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
          /* Cancel file selection */
          running = 0;
          result = 0;
          break;

        case SDLK_UP:
          /* Move selection up */
          if (browser.selected_index > 0) {
            browser.selected_index--;
            /* Adjust scroll if selection goes above visible area */
            if (browser.selected_index < browser.scroll_offset) {
              browser.scroll_offset = browser.selected_index;
            }
          }
          break;

        case SDLK_DOWN:
          /* Move selection down */
          if (browser.selected_index < browser.count - 1) {
            browser.selected_index++;
            /* Adjust scroll if selection goes below visible area */
            if (browser.selected_index >=
                browser.scroll_offset + visible_items) {
              browser.scroll_offset =
                  browser.selected_index - visible_items + 1;
            }
          }
          break;

        case SDLK_RETURN:
          /* Enter key: navigate into directory or select file */
          if (browser.files[browser.selected_index].is_directory) {
            /* Navigate into the selected directory */
            navigate_to(&browser, browser.files[browser.selected_index].name);
          } else {
            /* Select the file and return */
            snprintf(selected_path, path_size, "%s/%s", browser.current_path,
                     browser.files[browser.selected_index].name);
            running = 0;
            result = 1;
          }
          break;
        }
      }
      /* Handle mouse wheel scrolling */
      else if (event.type == SDL_MOUSEWHEEL) {
        if (event.wheel.y > 0) {
          /* Scroll up */
          if (browser.scroll_offset > 0)
            browser.scroll_offset--;
        } else if (event.wheel.y < 0) {
          /* Scroll down */
          if (browser.scroll_offset < browser.count - visible_items) {
            browser.scroll_offset++;
          }
        }
      }
      /* Handle mouse clicks */
      else if (event.type == SDL_MOUSEBUTTONDOWN) {
        int mouse_y = event.button.y;
        /* Check if click is within the file list area */
        if (mouse_y > 50 && mouse_y < 550) {
          /* Calculate which item was clicked based on y position */
          int clicked_index = (mouse_y - 50) / 25 + browser.scroll_offset;
          if (clicked_index < browser.count) {
            if (clicked_index == browser.selected_index) {
              /* Double-click simulation: activate the item */
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
            /* Update selection to clicked item */
            browser.selected_index = clicked_index;
          }
        }
      }
    }

    /* ===== Rendering section ===== */

    /* Clear screen with dark gray background */
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    /* Draw header bar with current path */
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
    SDL_Rect header = {10, 10, SCREEN_WIDTH - 20, 30};
    SDL_RenderFillRect(renderer, &header);

    /* Display current directory path in header */
    SDL_Color white = {255, 255, 255, 255};
    draw_text(renderer, font, browser.current_path, 15, 15, white);

    /* Render the list of files and directories */
    for (int i = 0;
         i < visible_items && (i + browser.scroll_offset) < browser.count;
         i++) {
      int index = i + browser.scroll_offset;
      int y = 50 + i * 25; /* Each item is 25 pixels tall */

      /* Highlight the selected item with a blue background */
      if (index == browser.selected_index) {
        SDL_SetRenderDrawColor(renderer, 70, 120, 200, 255);
        SDL_Rect select_rect = {10, y, SCREEN_WIDTH - 20, 23};
        SDL_RenderFillRect(renderer, &select_rect);
      }

      /* Format the display name (add [DIR] prefix for directories) */
      char display_name[300];
      if (browser.files[index].is_directory) {
        snprintf(display_name, 300, "[DIR] %s", browser.files[index].name);
      } else {
        strncpy(display_name, browser.files[index].name, 299);
      }

      /* Draw the file/directory name */
      SDL_Color text_color = {200, 200, 200, 255};
      draw_text(renderer, font, display_name, 15, y + 3, text_color);
    }

    /* Display usage instructions at the bottom of the window */
    SDL_Color gray = {150, 150, 150, 255};
    draw_text(renderer, font,
              "Arrows: navigate | Enter: select | Escape: cancel", 15,
              SCREEN_HEIGHT - 25, gray);

    /* Present the rendered frame to the screen */
    SDL_RenderPresent(renderer);

    /* Small delay to limit frame rate (~60 FPS) */
    SDL_Delay(16);
  }

  /* Cleanup resources before exit */
  TTF_CloseFont(font);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);

  return result;
}