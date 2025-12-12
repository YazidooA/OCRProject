#ifndef PIPELINE_INTERFACE_H
#define PIPELINE_INTERFACE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../structure_detection/structure_detection.h"
#include "../solver/solver.h"
#include "../draw_outline/draw_outline.h"
#include "../neural_network/nn.h"
#include "../neural_network/digitalisation.h"
#include "../letter_extractor/letter_extractor.h"

SDL_Surface* pipeline(SDL_Surface* surface, SDL_Renderer* render);

#endif