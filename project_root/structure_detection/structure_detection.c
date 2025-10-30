#include "structure_detection.h"

// Calculate horizontal projection (sum of black pixels per row)
int* calculate_horizontal_projection(Image* img) 
{
    int* projection = (int*)calloc(img->height, sizeof(int));
    for (int y = 0; y < img->height; y++) 
    {
        for (int x = 0; x < img->width; x++)
        {
            int pixel = img->data[y * img->width + x];
            if (pixel < 128) projection[y]++;
        }
    }
    return projection;
}

// Calculate vertical projection (sum of black pixels per column)
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

// Detect the word search grid position
Rectangle detect_grid_area(Image* img)
{
    Rectangle grid = {0, 0, 0, 0};
    
    int* h_proj = calculate_horizontal_projection(img);
    int* v_proj = calculate_vertical_projection(img);
    
    if (!h_proj || !v_proj) {
        if (h_proj) free(h_proj);
        if (v_proj) free(v_proj);
        return (Rectangle){0, 0, 0, 0};
    }
    
    int h_threshold = 0, v_threshold = 0;
    for (int i = 0; i < img->height; i++) h_threshold += h_proj[i];
    for (int i = 0; i < img->width; i++) v_threshold += v_proj[i];
    h_threshold = (h_threshold / img->height) * 0.3;
    v_threshold = (v_threshold / img->width) * 0.3;
    
    // Grid border detection
    int top = 0;
    for (int y = 0; y < img->height / 2; y++) {
        if (h_proj[y] > h_threshold) {
            int consistent_lines = 0;
            for (int i = y; i < y + 20 && i < img->height; i++) 
                if (h_proj[i] > h_threshold * 0.5) consistent_lines++;
            if (consistent_lines > 15) {
                top = y;
                break;
            }
        }
    }
    
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
    
    printf("Grid detected: x=%d, y=%d, w=%d, h=%d\n", 
           grid.x, grid.y, grid.width, grid.height);
    
    return grid;
}

// Detect word list position
Rectangle detect_word_list_area(Image* img) {
    Rectangle word_list = {0, 0, 0, 0};
    Rectangle grid = detect_grid_area(img);
    
    int* h_proj = calculate_horizontal_projection(img);
    int* v_proj = calculate_vertical_projection(img);
    
    int right_density = 0;
    int right_start = grid.x + grid.width + 10;
    if (right_start < img->width) {
        for (int x = right_start; x < img->width; x++) {
            right_density += v_proj[x];
        }
    }
    
    int bottom_density = 0;
    int bottom_start = grid.y + grid.height + 10;
    if (bottom_start < img->height) {
        for (int y = bottom_start; y < img->height; y++) {
            bottom_density += h_proj[y];
        }
    }
    
    if (right_density > bottom_density && right_start < img->width) {
        word_list.x = right_start;
        word_list.y = grid.y;
        word_list.width = img->width - right_start;
        word_list.height = grid.height;
    } else if (bottom_start < img->height) {
        word_list.x = 0;
        word_list.y = bottom_start;
        word_list.width = img->width;
        word_list.height = img->height - bottom_start;
    }
    
    free(h_proj);
    free(v_proj);
    
    printf("Word list detected: x=%d, y=%d, w=%d, h=%d\n",
           word_list.x, word_list.y, word_list.width, word_list.height);
    
    return word_list;
}

// Detect individual grid cells
Rectangle* detect_grid_cells(Image* img, Rectangle grid_area, int* cell_count, int* rows, int* cols) 
{
    int* h_proj = (int*)calloc(grid_area.height, sizeof(int));
    int* v_proj = (int*)calloc(grid_area.width, sizeof(int));
    
    if (!h_proj || !v_proj) {
        if (h_proj) free(h_proj);
        if (v_proj) free(v_proj);
        *cell_count = 0;
        *rows = 0;
        *cols = 0;
        return NULL;
    }
    
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
    
    int* h_separators = (int*)malloc(grid_area.height * sizeof(int));
    int h_sep_count = 0;
    int h_threshold = grid_area.width * 0.15;
    
    for (int y = 0; y < grid_area.height - 1; y++) {
        if (h_proj[y] > h_threshold && 
            (y == 0 || h_proj[y-1] < h_threshold)) {
            h_separators[h_sep_count++] = y;
        }
    }
    
    int* v_separators = (int*)malloc(grid_area.width * sizeof(int));
    int v_sep_count = 0;
    int v_threshold = grid_area.height * 0.15;
    
    for (int x = 0; x < grid_area.width - 1; x++) {
        if (v_proj[x] > v_threshold && 
            (x == 0 || v_proj[x-1] < v_threshold)) {
            v_separators[v_sep_count++] = x;
        }
    }
    
    printf("Separators detected: %d horizontal, %d vertical\n", 
           h_sep_count, v_sep_count);
    
    if (h_sep_count < 2 || v_sep_count < 2) {
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
    
    *rows = h_sep_count - 1;
    *cols = v_sep_count - 1;
    *cell_count = (*rows) * (*cols);
    
    Rectangle* cells = (Rectangle*)malloc(*cell_count * sizeof(Rectangle));
    int cell_idx = 0;
    
    for (int r = 0; r < *rows; r++) {
        for (int c = 0; c < *cols; c++) {
            cells[cell_idx].x = grid_area.x + v_separators[c];
            cells[cell_idx].y = grid_area.y + h_separators[r];
            cells[cell_idx].width = v_separators[c + 1] - v_separators[c];
            cells[cell_idx].height = h_separators[r + 1] - h_separators[r];
            cell_idx++;
        }
    }
    
    printf("Cells detected: %d (%d rows × %d columns)\n", 
           *cell_count, *rows, *cols);
    
    free(h_proj);
    free(v_proj);
    free(h_separators);
    free(v_separators);
    
    return cells;
}

// Utility function: check if point is inside rectangle
int is_inside_rectangle(int x, int y, Rectangle rect) {
    return (x >= rect.x && x < rect.x + rect.width &&
            y >= rect.y && y < rect.y + rect.height);
}

// NOTE: draw_line() has been removed
// Use draw_outline() from draw_outline.c for drawing on images instead