#ifndef UI_H
#define UI_H

#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

/* Dessine l’interface utilisateur + l’image */
void ui_draw(SDL_Renderer* renderer, SDL_Surface* surface);

/* Gère les événements (clic bouton, fermeture fenêtre, etc.) */
void ui_handle_events(int* running);

#endif
