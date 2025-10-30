#include "file_saver.h"

typedef char bstring[50];

void give_text(bstring* text, char* namefile) 
{
    FILE *pFile = fopen(namefile, "w");
    if (pFile == NULL)
    {
        printf("Erreur lors de l'ouverture du fichier\n");
        return;
    }
    for (int i = 0; i < 50; i++) 
    {
        if (text[i][0] == '\0') break; 
        fprintf(pFile, "%s\n", text[i]);
    }
    
    printf("Fichier écrit avec succès!\n");
    fclose(pFile);
}

int myvim(char* file, char* text_array[]) 
{
    bstring* text = malloc(sizeof(bstring) * 50);
    if (text == NULL) 
    {
        printf("Erreur d'allocation mémoire\n");
        return -1;
    }
    
    for (int i = 0; i < 50; i++) text[i][0] = '\0';
    
    int i = 0;
    while (text_array[i] != NULL && i < 50) 
    {
        strncpy(text[i], text_array[i], 49);
        text[i][49] = '\0'; 
        i++;
    }
    
    give_text(text, file);
    
    for (int j = 0; j < i; j++) printf("%s\n", text[j]);
    
    free(text);
    return 0;
}

// Save solved grid with highlighted words
void save_solved_grid(Image* img, SearchResult* results, int result_count, const char* output_path) {
    if (!img || !img->data || !results || !output_path) {
        fprintf(stderr, "Erreur: paramètres invalides dans save_solved_grid\n");
        return;
    }

    // Create a copy of the image to avoid modifying the original
    Image* output = (Image*)malloc(sizeof(Image));
    if (!output) {
        fprintf(stderr, "Erreur: échec d'allocation mémoire pour l'image de sortie\n");
        return;
    }
    
    output->width = img->width;
    output->height = img->height;
    output->channels = img->channels;
    output->data = (unsigned char*)malloc(img->width * img->height * img->channels);
    
    if (!output->data) {
        fprintf(stderr, "Erreur: échec d'allocation mémoire pour les données de l'image\n");
        free(output);
        return;
    }
    
    memcpy(output->data, img->data, img->width * img->height * img->channels);
    
    // Colors for highlighting (grayscale: 200 = light gray)
    unsigned char highlight_color = 200;
    int line_thickness = 3;
    
    // Prepare text array for myvim
    char** text_array = (char**)malloc((result_count + 1) * sizeof(char*));
    for (int i = 0; i < result_count; i++) {
        text_array[i] = (char*)malloc(50 * sizeof(char));
    }
    text_array[result_count] = NULL; // Terminator
    
    // Draw each found word and prepare text results
    for (int i = 0; i < result_count; i++) {
        if (results[i].found) {
            // Validate coordinates are within image bounds
            if (results[i].start.x < 0 || results[i].start.x >= output->width ||
                results[i].start.y < 0 || results[i].start.y >= output->height ||
                results[i].end.x < 0 || results[i].end.x >= output->width ||
                results[i].end.y < 0 || results[i].end.y >= output->height) {
                fprintf(stderr, "Warning: Invalid coordinates for word '%s', skipping\n", results[i].word);
                continue;
            }
            
            printf("Dessin du mot '%s' de (%d,%d) à (%d,%d)\n",
                   results[i].word,
                   results[i].start.x, results[i].start.y,
                   results[i].end.x, results[i].end.y);
            
            draw_line(output, results[i].start, results[i].end, 
                     highlight_color, line_thickness);
            
            // Format result line
            char coord_str[100];
            snprintf(coord_str, sizeof(coord_str), "%s: (%d,%d) -> (%d,%d)",
                    results[i].word,
                    results[i].start.x, results[i].start.y,
                    results[i].end.x, results[i].end.y);
            strncpy(text_array[i], coord_str, 49);
            text_array[i][49] = '\0';
        } else {
            char not_found_str[100];
            snprintf(not_found_str, sizeof(not_found_str), "%s: Not found", results[i].word);
            strncpy(text_array[i], not_found_str, 49);
            text_array[i][49] = '\0';
        }
    }

    // Save image in PGM format
    FILE* fp = fopen(output_path, "wb");
    if (fp) {
        fprintf(fp, "P5\n%d %d\n255\n", output->width, output->height);
        fwrite(output->data, 1, output->width * output->height, fp);
        fclose(fp);
        printf("Grille résolue sauvegardée: %s\n", output_path);
    } else {
        fprintf(stderr, "Erreur: impossible de sauvegarder l'image\n");
    }
    
    // Create text output path by replacing extension
    char text_output_path[256];
    strncpy(text_output_path, output_path, 255);
    text_output_path[255] = '\0';
    
    char* ext = strrchr(text_output_path, '.');
    if (ext != NULL) {
        strcpy(ext, ".txt");
    } else {
        strcat(text_output_path, ".txt");
    }
    
    printf("Sauvegarde des résultats textuels via myvim...\n");
    myvim(text_output_path, text_array);
    
    // Cleanup
    for (int i = 0; i < result_count; i++) {
        free(text_array[i]);
    }
    free(text_array);
    free(output->data);
    free(output);
}