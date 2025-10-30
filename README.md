rotation:
./output/rotation_compiled test.png 90

solved:
./output/solver grid APPLE

draw_outline:
./output/draw_outline test.png test_result.(png/bmp)

Dans structure_detection.c : 

Rectangle detect_grid_area(Image* img)  => Localise automatiquement la zone de la grille dans l'image complète.
Elle doit être utiliser Juste après avoir chargé l'image, avant l'extraction des lettres.

void draw_line(Image* img, Position start, Position end,  unsigned char color, int thickness) =>  Dessine des lignes sur l'image (pour surligner les mots trouvés).
Elle doit être utiliser  Après avoir résolu la grille, pour visualiser les mots.

Dans letter_extractor.c :

LetterGrid* extract_letters_from_grid(Image* img, Rectangle grid_area, int rows, int cols); =>elle écoupe la grille en cellules, Extrait chaque lettre, Applique normalize_letter() automatiquement (28×28, binarisé, centré) et retourne TOUTES les lettres prêtes pour le RdN
Elle doit etre utiliser apres le detect_grid_area.

void save_letter_grid(LetterGrid* grid, const char* output_dir) => Sauvegarde toutes les lettres extraites en fichiers PGM individuels.
Elle doit etre utiliser si le RdN prédit mal, pour visualiser ce qu'il reçoit réellement.

Dans file_saver.c

void save_solved_grid(Image* img, SearchResult* results, int result_count, const char* output_path); = > Sauvegarde l'image avec les mots surlignés + fichier texte des coordonnées.
Elle doit être utiliser À la toute fin, après avoir trouvé tous les mots.
