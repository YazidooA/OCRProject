
// ===========================================
// include/gui/callbacks.h
// ===========================================
#ifndef CALLBACKS_H
#define CALLBACKS_H

#include "../common.h"
#include "main_window.h"
#include <gtk/gtk.h>

// Callbacks principaux
void on_load_image_clicked(GtkButton* button, AppGUI* gui);
void on_solve_clicked(GtkButton* button, AppGUI* gui);
void on_save_result_clicked(GtkButton* button, AppGUI* gui);
void on_preprocess_clicked(GtkButton* button, AppGUI* gui);

// Callbacks de menu
void on_file_open(GtkMenuItem* item, AppGUI* gui);
void on_file_save(GtkMenuItem* item, AppGUI* gui);
void on_file_quit(GtkMenuItem* item, AppGUI* gui);
void on_help_about(GtkMenuItem* item, AppGUI* gui);

// Callbacks d'événements
void on_window_destroy(GtkWidget* widget, AppGUI* gui);
gboolean on_key_press(GtkWidget* widget, GdkEventKey* event, AppGUI* gui);
gboolean on_image_clicked(GtkWidget* widget, GdkEventButton* event, AppGUI* gui);

// Callbacks de prétraitement
void on_rotate_manual(GtkButton* button, AppGUI* gui);
void on_rotate_auto(GtkButton* button, AppGUI* gui);
void on_enhance_contrast(GtkButton* button, AppGUI* gui);
void on_remove_noise(GtkButton* button, AppGUI* gui);

#endif // CALLBACKS_H