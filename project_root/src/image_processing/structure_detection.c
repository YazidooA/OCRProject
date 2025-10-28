#include <structure_detection.h>

// Fonction pour compter les transitions blanc->noir dans une ligne
int count_transitions(unsigned char* line, int length, int threshold) 
{
    int transitions = 0;
    int prev_state = line[0] > threshold;    
    for (int i = 1; i < length; i++) 
    {
        int curr_state = line[i] > threshold;
        if (curr_state != prev_state) 
        {
            transitions++;
            prev_state = curr_state;
        }
    }
    
    return transitions;
}

// Calcule la projection horizontale (somme des pixels noirs par ligne)
int* calculate_horizontal_projection(Image* img) 
{
    int* projection = (int*)calloc(img->height, sizeof(int));
    for (int y = 0; y < img->height; y++) 
    {
        for (int x = 0; x < img->width; x++)
        {
            int pixel = img->data[y * img->width + x];
            // Compte les pixels noirs (on suppose une image binaire)
            if (pixel < 128) projection[y]++;
        }
    }
    
    return projection;
}

// Calcule la projection verticale (somme des pixels noirs par colonne)
int* calculate_vertical_projection(Image* img) 
{
    int* projection = (int*)calloc(img->width, sizeof(int));
    
    for (int x = 0; x < img->width; x++) 
    {
        for (int y = 0; y < img->height; y++) 
        {
            int pixel = img->data[y * img->width + x];
            if (pixel < 128) projection[x]++;
        }
    }
    
    return projection;
}

// Détecte la position de la grille de mots cachés
Rectangle detect_grid_area(Image* img)
{
    Rectangle grid = {0, 0, 0, 0};
    
    // Calcul des projections
    int* h_proj = calculate_horizontal_projection(img);
    int* v_proj = calculate_vertical_projection(img);
    
    // Détermination des seuils basés sur la densité moyenne
    int h_threshold = 0, v_threshold = 0;
    for (int i = 0; i < img->height; i++) h_threshold += h_proj[i];
    for (int i = 0; i < img->width; i++) v_threshold += v_proj[i];
    h_threshold = (h_threshold / img->height) * 0.3;
    v_threshold = (v_threshold / img->width) * 0.3;
    
    // Détection des bordures de la grille
    // Recherche du haut de la grille
    int top = 0;
    for (int y = 0; y < img->height / 2; y++) {
        if (h_proj[y] > h_threshold) {
            // Vérifie qu'il y a une densité constante (caractéristique d'une grille)
            int consistent_lines = 0;
            for (int i = y; i < y + 20 && i < img->height; i++) if (h_proj[i] > h_threshold * 0.5) consistent_lines++;
            if (consistent_lines > 15) 
            {
                top = y;
                break;
            }
        }
    }
    
    // Recherche du bas de la grille
    int bottom = img->height - 1;
    for (int y = img->height - 1; y > img->height / 2; y--) {
        if (h_proj[y] > h_threshold) {
            int consistent_lines = 0;
            for (int i = y; i > y - 20 && i >= 0; i--) {
                if (h_proj[i] > h_threshold * 0.5) consistent_lines++;
            }
            if (consistent_lines > 15) {
                bottom = y;
                break;
            }
        }
    }
    
    // Recherche de la gauche de la grille
    int left = 0;
    for (int x = 0; x < img->width / 2; x++) {
        if (v_proj[x] > v_threshold) {
            int consistent_cols = 0;
            for (int i = x; i < x + 20 && i < img->width; i++) {
                if (v_proj[i] > v_threshold * 0.5) consistent_cols++;
            }
            if (consistent_cols > 15) {
                left = x;
                break;
            }
        }
    }
    
    // Recherche de la droite de la grille
    int right = img->width - 1;
    for (int x = img->width - 1; x > img->width / 2; x--) {
        if (v_proj[x] > v_threshold) {
            int consistent_cols = 0;
            for (int i = x; i > x - 20 && i >= 0; i--) {
                if (v_proj[i] > v_threshold * 0.5) consistent_cols++;
            }
            if (consistent_cols > 15) {
                right = x;
                break;
            }
        }
    }
    
    grid.x = left;
    grid.y = top;
    grid.width = right - left;
    grid.height = bottom - top;
    
    free(h_proj);
    free(v_proj);
    
    printf("Grille détectée: x=%d, y=%d, w=%d, h=%d\n", 
           grid.x, grid.y, grid.width, grid.height);
    
    return grid;
}

// Détecte la position de la liste de mots
Rectangle detect_word_list_area(Image* img) {
    Rectangle word_list = {0, 0, 0, 0};
    
    // La liste de mots est généralement à droite ou en bas de la grille
    Rectangle grid = detect_grid_area(img);
    
    // Calcul des projections pour la zone hors grille
    int* h_proj = calculate_horizontal_projection(img);
    int* v_proj = calculate_vertical_projection(img);
    
    // Recherche à droite de la grille
    int right_density = 0;
    int right_start = grid.x + grid.width + 10;
    if (right_start < img->width) {
        for (int x = right_start; x < img->width; x++) {
            right_density += v_proj[x];
        }
    }
    
    // Recherche en bas de la grille
    int bottom_density = 0;
    int bottom_start = grid.y + grid.height + 10;
    if (bottom_start < img->height) {
        for (int y = bottom_start; y < img->height; y++) {
            bottom_density += h_proj[y];
        }
    }
    
    // Détermine si la liste est à droite ou en bas
    if (right_density > bottom_density && right_start < img->width) {
        // Liste à droite
        word_list.x = right_start;
        word_list.y = grid.y;
        word_list.width = img->width - right_start;
        word_list.height = grid.height;
    } else if (bottom_start < img->height) {
        // Liste en bas
        word_list.x = 0;
        word_list.y = bottom_start;
        word_list.width = img->width;
        word_list.height = img->height - bottom_start;
    }
    
    free(h_proj);
    free(v_proj);
    
    printf("Liste de mots détectée: x=%d, y=%d, w=%d, h=%d\n",
           word_list.x, word_list.y, word_list.width, word_list.height);
    
    return word_list;
}

// Détecte les cases individuelles de la grille
Rectangle* detect_grid_cells(Image* img, Rectangle grid_area, int* cell_count) {
    // Extraction de la région de la grille
    int* h_proj = (int*)calloc(grid_area.height, sizeof(int));
    int* v_proj = (int*)calloc(grid_area.width, sizeof(int));
    
    // Calcul des projections dans la zone de la grille
    for (int y = 0; y < grid_area.height; y++) 
    {
        for (int x = 0; x < grid_area.width; x++) 
        {
            int img_x = grid_area.x + x;
            int img_y = grid_area.y + y;
            if (img_x < img->width && img_y < img->height) 
            {
                int pixel = img->data[img_y * img->width + img_x];
                if (pixel < 128) 
                {
                    h_proj[y]++;
                    v_proj[x]++;
                }
            }
        }
    }
    
    // Détection des lignes de séparation horizontales
    int* h_separators = (int*)malloc(grid_area.height * sizeof(int));
    int h_sep_count = 0;
    int h_threshold = grid_area.width * 0.15;
    
    for (int y = 0; y < grid_area.height - 1; y++) {
        // Une ligne de séparation a une densité élevée
        if (h_proj[y] > h_threshold && 
            (y == 0 || h_proj[y-1] < h_threshold)) {
            h_separators[h_sep_count++] = y;
        }
    }
    
    // Détection des lignes de séparation verticales
    int* v_separators = (int*)malloc(grid_area.width * sizeof(int));
    int v_sep_count = 0;
    int v_threshold = grid_area.height * 0.15;
    
    for (int x = 0; x < grid_area.width - 1; x++) {
        if (v_proj[x] > v_threshold && 
            (x == 0 || v_proj[x-1] < v_threshold)) {
            v_separators[v_sep_count++] = x;
        }
    }
    
    printf("Séparateurs détectés: %d horizontaux, %d verticaux\n", 
           h_sep_count, v_sep_count);
    
    // Estimation du nombre de cellules si pas assez de séparateurs détectés
    if (h_sep_count < 2 || v_sep_count < 2) {
        // Estimation basée sur la taille moyenne des grilles (10-20 cases)
        int estimated_rows = 15;
        int estimated_cols = 15;
        
        h_sep_count = estimated_rows + 1;
        v_sep_count = estimated_cols + 1;
        
        for (int i = 0; i < h_sep_count; i++) {
            h_separators[i] = (grid_area.height * i) / estimated_rows;
        }
        for (int i = 0; i < v_sep_count; i++) {
            v_separators[i] = (grid_area.width * i) / estimated_cols;
        }
    }
    
    // Création des rectangles pour chaque cellule
    int rows = h_sep_count - 1;
    int cols = v_sep_count - 1;
    *cell_count = rows * cols;
    
    Rectangle* cells = (Rectangle*)malloc(*cell_count * sizeof(Rectangle));
    int cell_idx = 0;
    
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            cells[cell_idx].x = grid_area.x + v_separators[c];
            cells[cell_idx].y = grid_area.y + h_separators[r];
            cells[cell_idx].width = v_separators[c + 1] - v_separators[c];
            cells[cell_idx].height = h_separators[r + 1] - h_separators[r];
            cell_idx++;
        }
    }
    
    printf("Cellules détectées: %d (%d lignes × %d colonnes)\n", 
           *cell_count, rows, cols);
    
    free(h_proj);
    free(v_proj);
    free(h_separators);
    free(v_separators);
    
    return cells;
}

// Vérifie si un point est à l'intérieur d'un rectangle
int is_inside_rectangle(int x, int y, Rectangle rect) {
    return (x >= rect.x && x < rect.x + rect.width &&
            y >= rect.y && y < rect.y + rect.height);
}

// Dessine un rectangle sur l'image
void draw_rectangle(Image* img, Rectangle rect, unsigned char color) 
{
    // Bord supérieur et inférieur
    for (int x = rect.x; x < rect.x + rect.width; x++) 
    {
        if (x >= 0 && x < img->width) 
        {
            if (rect.y >= 0 && rect.y < img->height) img->data[rect.y * img->width + x] = color;
            if (rect.y + rect.height >= 0 && rect.y + rect.height < img->height) img->data[(rect.y + rect.height) * img->width + x] = color;
        }
    }
    
    // Bord gauche et droit
    for (int y = rect.y; y < rect.y + rect.height; y++) {
        if (y >= 0 && y < img->height) {
            if (rect.x >= 0 && rect.x < img->width) img->data[y * img->width + rect.x] = color;
            if (rect.x + rect.width >= 0 && rect.x + rect.width < img->width) img->data[y * img->width + rect.x + rect.width] = color;
        }
    }
}

// Dessine une ligne sur l'image (algorithme de Bresenham)
void draw_line(Image* img, Position start, Position end, unsigned char color, int thickness) {
    int dx = abs(end.x - start.x);
    int dy = abs(end.y - start.y);
    int sx = start.x < end.x ? 1 : -1;
    int sy = start.y < end.y ? 1 : -1;
    int err = dx - dy;
    
    int x = start.x;
    int y = start.y;
    
    while (1) {
        // Dessine un point avec épaisseur
        for (int ty = -thickness/2; ty <= thickness/2; ty++) {
            for (int tx = -thickness/2; tx <= thickness/2; tx++) {
                int px = x + tx;
                int py = y + ty;
                if (px >= 0 && px < img->width && py >= 0 && py < img->height) {
                    img->data[py * img->width + px] = color;
                }
            }
        }
        
        if (x == end.x && y == end.y) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}