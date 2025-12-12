#ifndef STRUCTURE_DETECTION_H
#define STRUCTURE_DETECTION_H

#include <SDL2/SDL.h>

/*
 * Détection de la zone GRILLE (mots croisés) et de la zone LISTE (mots à trouver)
 * à partir d'une image couleur SDL.
 *
 * Entrée :
 *   - src  : surface SDL (n'importe quel format, sera convertie en ARGB8888)
 *
 * Sortie :
 *   - grid : rectangle contenant la grille (en pixels, coordonnées dans src)
 *   - list : rectangle contenant la liste de mots (peut être vide si non trouvée)
 *
 * Retour :
 *   0  : succès (grid est toujours non vide ; list peut être vide)
 *  -1  : erreur (SDL, mémoire, etc.)
 *
 * Remarques :
 *   - Cas 1 : si une grande composante noire type "grille avec traits" est détectée,
 *             on considère que c'est la grille et on cherche la liste à droite/gauche
 *             via une bande dense horizontale.
 *
 *   - Cas 2 : sinon (pas de grande composante), on regarde les composantes "lettres"
 *             seules, on détecte les clusters gauche/droite (grid | gap | list),
 *             et on déduit ainsi grid + list.
 */
int detect_grid_and_list(SDL_Surface *src, SDL_Rect *grid, SDL_Rect *list);

#endif
