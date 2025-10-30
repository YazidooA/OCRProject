

struct image_data{
    char* filepath;
    char* name;
    char* filetype;
};

void fill_data(struct image_data *data, char* filepath);
void print_image_data(struct image_data *data);

void get_surface(SDL_Renderer *renderer, SDL_Surface **surface);
void actualize_rendering(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Surface *surface);

void save_sketch(struct image_data *data, SDL_Renderer *renderer, const char *suffix);
void save_surface(struct image_data *data, SDL_Surface *surface, const char *suffix);
void load_in_surface(struct image_data *data, SDL_Surface **surface);
void load_in_texture(struct image_data *data, SDL_Renderer *renderer, SDL_Texture **texture, SDL_Surface **surface);

void initialize(struct image_data *data, SDL_Window **window, SDL_Renderer **renderer, SDL_Texture **texture, SDL_Surface **surface, char* file);
void terminate(SDL_Window *window, SDL_Renderer *renderer, SDL_Texture *texture);