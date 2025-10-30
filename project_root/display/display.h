// ===========================================
// include/gui/display.h
// ===========================================
#ifndef DISPLAY_H
#define DISPLAY_H

#include "../common.h"
#include "main_window.h"
#include <gtk/gtk.h>

// Affichage d'images
void display_image(GtkWidget* image_widget, const Image* img);
GdkPixbuf* image_to_pixbuf(const Image* img);
void update_image_display(AppGUI* gui);

// Affichage des résultats
void display_solved_grid(AppGUI* gui, const WordGrid* grid, const SearchResult* results, int result_count);
void highlight_found_words(GtkWidget* widget, const SearchResult* results, int result_count);
void draw_word_highlight(cairo_t* cr, const SearchResult* result);

// Utilitaires d'affichage
void set_image_scale(GtkWidget* image_widget, double scale);
void center_image_view(GtkWidget* scrolled_window);
void update_zoom_controls(AppGUI* gui, double current_scale);

// Superpositions graphiques
void draw_grid_overlay(cairo_t* cr, const WordGrid* grid);
void draw_character_boxes(cairo_t* cr, const Character* chars, int count);
void draw_detection_results(cairo_t* cr, const Rectangle* detections, int count);

#endif // DISPLAY_H