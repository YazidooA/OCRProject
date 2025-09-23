// ===========================================
// include/gui/file_dialogs.h
// ===========================================
#ifndef FILE_DIALOGS_H
#define FILE_DIALOGS_H

#include "../common.h"
#include <gtk/gtk.h>

// Dialogs de fichiers
char* show_open_image_dialog(GtkWindow* parent);
char* show_save_result_dialog(GtkWindow* parent);
char* show_save_image_dialog(GtkWindow* parent);

// Filtres de fichiers
void add_image_filters(GtkFileChooser* chooser);
void add_result_filters(GtkFileChooser* chooser);

// Validation de fichiers
bool is_valid_image_file(const char* filename);
bool is_writable_location(const char* path);

// Utilitaires
char* get_file_extension(const char* filename);
void set_default_filename(GtkFileChooser* chooser, const char* base_name, const char* extension);

#endif // FILE_DIALOGS_H