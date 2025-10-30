#include <stdio.h>
#include <err.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_image.h>
#include "../image_cleaner/image_cleaner.h"
#include "../rotation/rotation.h"

#pragma region Image Data

struct image_data{
    char* filepath;
    char* name;
    char* filetype;
};

void fill_data(struct image_data *data, char* filepath){
    if (!data) errx(1, "Data pointer is NULL");
    if (!filepath) errx(1, "Filepath is NULL");

    //FILE PATH
    data->filepath = filepath;

    
    const char* slash = strrchr(filepath, '/');
    const char* basename = (slash) ? slash + 1 : filepath;

    // Prepare for file name
    const char* dot = strrchr(basename, '.');
    if (!dot) dot = basename + strlen(basename);

    //FILE NAME
    static char name_buffer[512];
    size_t name_len = dot - basename;
    if (name_len >= sizeof(name_buffer)) name_len = sizeof(name_buffer) - 1;
    strncpy(name_buffer, basename, name_len);
    name_buffer[name_len] = '\0';
    data->name = name_buffer;

    // FILE TYPE
    data->filetype = (*dot) ? (char*)(dot + 1) : (char*)"";
}

void print_image_data(struct image_data *data){
    if(data == NULL) return;

    printf("Image Path: %s\n", data->filepath);
    printf("Image Name: %s\n", data->name);
    printf("Image Filetype: %s\n", data->filetype);
}
#pragma endregion Image Data


#pragma region SDL Utilities

void get_surface(SDL_Renderer *renderer, SDL_Surface **surface){
    SDL_Window* window = SDL_RenderGetWindow(renderer);
    if (!window)
    {
        errx(1, "Failed to get window from renderer");
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);

    *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!*surface)
    {
        errx(1, "Failed to create surface");
    }

    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGBA8888, (*surface)->pixels, (*surface)->pitch) != 0)
    {
        errx(1, "Failed to read pixels from renderer: %s", SDL_GetError());
    }
}



void actualize_rendering(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Surface *surface) {
    if (!renderer) errx(1, "Actualization Failed: Renderer NULL");
    if (!texture)  errx(1, "Actualization Failed: Texture NULL");
    if (!surface)  errx(1, "Actualization Failed: Surface NULL");

    SDL_Texture *temp_texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!temp_texture) errx(1, "SDL Texture Creation Failed: %s", SDL_GetError());

    if (SDL_SetRenderTarget(renderer, texture) != 0)
    {
        errx(1, "Failed to set render target: %s", SDL_GetError());
    }


    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (SDL_RenderCopy(renderer, temp_texture, NULL, NULL) != 0)
    {
        errx(1, "Failed to Render Copy: %s", SDL_GetError());
    }

        //clear render target
    SDL_SetRenderTarget(renderer, NULL);
    SDL_DestroyTexture(temp_texture);

    // draw on window
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}


/*
void actualize_rendering(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Surface *surface){

    if (!renderer) errx(1, "Actualization Failed: Renderer NULL");
    if (!texture)  errx(1, "Actualization Failed: Texture NULL");
    if (!surface)  errx(1, "Actualization Failed: Surface NULL");

    SDL_Surface* formattedSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA8888, 0);
    if (!formattedSurface) errx(1, "Surface format conversion failed: %s", SDL_GetError());

    SDL_Texture *temp_texture = SDL_CreateTextureFromSurface(renderer, formattedSurface);
    SDL_FreeSurface(formattedSurface);

    if (!temp_texture) errx(1, "SDL Texture Creation Failed: %s", SDL_GetError());

    if (SDL_SetRenderTarget(renderer, texture) != 0)
        errx(1, "Failed to set render target: %s", SDL_GetError());

    if (SDL_RenderCopy(renderer, temp_texture, NULL, NULL) != 0)
        errx(1, "Failed to Render Copy: %s", SDL_GetError());

    SDL_SetRenderTarget(renderer, NULL);
    SDL_DestroyTexture(temp_texture);

    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}
*/




void save_sketch(struct image_data *data, SDL_Renderer *renderer, const char *suffix){

    if (!data || !renderer || !suffix) {
        errx(1, "Invalid arguments to save_sketch");
    }

    int width, height;
    SDL_Window *window = SDL_RenderGetWindow(renderer);
    if (!window) errx(1, "Failed to get window from renderer");

    SDL_GetWindowSize(window, &width, &height);

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!surface) errx(1, "Failed to create surface");

    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGBA8888, surface->pixels, width * 4) != 0)
        errx(1, "Failed to read pixels: %s", SDL_GetError());

    char new_filename[512];
    snprintf(new_filename, sizeof(new_filename), "%s%s%s.%s", data->name, "_", suffix, data->filetype);

    int success = 0;
    if (strcasecmp(data->filetype, "png") == 0) {
        success = IMG_SavePNG(surface, new_filename);
    } else if (strcasecmp(data->filetype, "jpg") == 0 || strcasecmp(data->filetype, "jpeg") == 0) {
        success = IMG_SaveJPG(surface, new_filename, 100);
    } else {
        success = SDL_SaveBMP(surface, new_filename);
    }

    if (success != 0)
        errx(1, "Failed to save file: %s", SDL_GetError());

    printf("Saved image successfully as: %s\n", new_filename);

    SDL_FreeSurface(surface); 
}


void save_surface(struct image_data *data, SDL_Surface *surface, const char *suffix){

    if (!data || !surface || !suffix) {
        errx(1, "Invalid arguments to save_surface");
    }

    char new_filename[512];
    snprintf(new_filename, sizeof(new_filename), "%s%s%s.%s", data->name, "_", suffix, data->filetype);

    int success = 0;
    if (strcasecmp(data->filetype, "png") == 0) {
        success = IMG_SavePNG(surface, new_filename);
    } else if (strcasecmp(data->filetype, "jpg") == 0 || strcasecmp(data->filetype, "jpeg") == 0) {
        success = IMG_SaveJPG(surface, new_filename, 100);
    } else {
        success = SDL_SaveBMP(surface, new_filename);
    }

    if (success != 0)
        errx(1, "Failed to save file: %s", SDL_GetError());

    printf("Saved image successfully as: %s\n", new_filename);
}


void load_in_surface(struct image_data *data, SDL_Surface **surface)
{
    if (!data)
    {
        errx(1, "load_in_surface: data is NULL");
    }

    if (!data->filepath)
    {
        errx(1, "load_in_surface: data->filepath is NULL");
    }

    if(!surface)
    {
        errx(1, "load_in_surface: surface is NULL");
    }

    if (*surface)
    {
        SDL_FreeSurface(*surface);
    }
    


    SDL_Surface *temp_surface = IMG_Load(data->filepath);
    if (!temp_surface)
    {
        errx(1, "Failed to load image '%s': %s", data->filepath, IMG_GetError());
    }

    *surface = SDL_ConvertSurfaceFormat(temp_surface, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(temp_surface);

    if (!*surface)
    {
        errx(1, "Surface format conversion failed: %s", SDL_GetError());
    }
}
#pragma endregion SDL Utilities






void initialize(struct image_data *data, SDL_Window **window, SDL_Renderer **renderer, SDL_Texture **texture, SDL_Surface **surface, char* file){
    
    int init_success = SDL_Init(SDL_INIT_VIDEO);
    if (init_success < 0)
    {
        errx(1, "SDL Init failed: %s", SDL_GetError());
    }

    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(imgFlags) & imgFlags))
    {
        errx(1, "SDL_image Init failed: %s", IMG_GetError());
    }

    const char* title = "Words Finder";
    int width = 800;
    int height = 600;

    *window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, 0);
    if (!*window) errx(1, "SDL Window creation failed: %s", SDL_GetError());

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!*renderer) errx(1, "SDL Renderer creation failed: %s", SDL_GetError());

    *texture = SDL_CreateTexture(*renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
    if (!*texture) errx(1, "SDL Texture creation failed: %s", SDL_GetError());

    if (file != NULL) {
        load_in_surface(data, surface);
        actualize_rendering(*renderer, *texture, *surface);
        
    } else {
        errx(1, "No image file provided");
    }
}


void terminate(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *texture){
        SDL_DestroyWindow(window);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyTexture(texture);
        IMG_Quit();
        SDL_Quit();
}



void draw_line(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Color color, int x1, int y1, int x2, int y2){
        SDL_SetRenderTarget(renderer, texture);
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        int success = SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        if(success != 0) errx(1, "Failed To draw line !");

        SDL_SetRenderTarget(renderer, NULL);
}




#pragma region Test Funcs 

int event_handler(struct image_data *data, SDL_Renderer *renderer, SDL_Texture *texture, SDL_Surface **surface, int *start_x, int *start_y, SDL_Color *currColor, float *hue){
        SDL_Event event;

        int prs_s = 0;
        int prs_ctrl = 0;

        while (SDL_PollEvent(&event)){
                  switch (event.type) {
                        case SDL_QUIT:
                                  return 0;
                                break;
                        case SDL_MOUSEBUTTONDOWN:
                                //convert_to_grayscale(surface);
                                //save_surface(data, surface, "grayscale");
                                //actualize_rendering(renderer, texture, surface);
                                break;
                        case SDL_KEYDOWN:
                                switch(event.key.keysym.sym){
                                        case SDLK_c: // Reload Image from disk
                                                load_in_surface(data, surface);
                                                actualize_rendering(renderer, texture, *surface);
                                                break;
                                        case SDLK_r: // Rotate Surface properly
                                                double value = auto_deskew_correction(*surface);
                                                *surface = rotate(*surface, value);
                                                save_surface(data, *surface, "rotation");
                                                actualize_rendering(renderer, texture, *surface);
                                                break;
                                        case SDLK_g: // Apply grayscaling to surface
                                                convert_to_grayscale(*surface);
                                                save_surface(data, *surface, "grayscale");
                                                actualize_rendering(renderer, texture, *surface);
                                                break;
                                        case SDLK_h: // apply otsu thresholding to surface
                                                apply_otsu_thresholding(*surface);
                                                save_surface(data, *surface, "otsu_thresholding");
                                                actualize_rendering(renderer, texture, *surface);
                                                break;
                                        case SDLK_j: // apply noise removal to surface
                                                apply_noise_removal(*surface, 2);
                                                save_surface(data, *surface, "noise_removal");
                                                actualize_rendering(renderer, texture, *surface);
                                                break;
                                        case SDLK_k: // Resolve and send to neuronal network
                                                // TODO
                                                actualize_rendering(renderer, texture, *surface);
                                                break;

                                        case SDLK_LCTRL:
                                                prs_ctrl = 1;
                                                printf("Pressed Ctrl\n");
                                                break;
                                        case SDLK_s:
                                                prs_s = 1;
                                                printf("Pressed S\n");
                                                break;
                                }
                                break;
                  }
        }


        if(prs_s && prs_ctrl) save_sketch(data, renderer, "output");


        return 1;
}

int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    SDL_Surface *surface = NULL;
    char* file = NULL;
    if (argc >1)
        file = argv[1];

    struct image_data data;
    fill_data(&data, file);
    print_image_data(&data);    

    initialize(&data, &window, &renderer, &texture, &surface, file);

    SDL_Color currColor = {255,0,0,255};
    float hue = 0.0f;
        int running = 1;
        int start_x = 0;
        int start_y = 0;

    while (running) {
        running = event_handler(&data, renderer, texture, &surface, &start_x, &start_y, &currColor, &hue);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    terminate(window, renderer, texture);
    return EXIT_SUCCESS;
}

#pragma endregion Test Funcs 
