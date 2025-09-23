#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "../common.h"
#include <gtk/gtk.h>

typedef struct {
    GtkWidget* main_window;
    GtkWidget* image_view;
    GtkWidget* result_view;
    GtkWidget* progress_bar;
    GtkWidget* status_label;
    
    // Menus et boutons
    GtkWidget* load_button;
    GtkWidget* solve_button;
    GtkWidget* save_button;
    GtkWidget* preprocess_button;
    
    // Données
    Image* current_image;
    Image* processed_image;
    WordGrid* solved_grid;
    SearchResult* results;
    int result_count;
} AppGUI;

// Création de l'interface
AppGUI* create_main_gui(void);
void setup_main_window(AppGUI* gui);
void setup_menu_bar(AppGUI* gui);
void setup_toolbar(AppGUI* gui);
void setup_status_bar(AppGUI* gui);

// Gestion de l'interface
void show_main_window(AppGUI* gui);
void update_status(AppGUI* gui, const char* message);
void update_progress(AppGUI* gui, double fraction);

// Nettoyage
void destroy_gui(AppGUI* gui);

#endif // MAIN_WINDOW_H