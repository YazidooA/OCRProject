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

What does which button : 
Click buttons or use keyboard shortcuts:
  O / Open File         - Select a new file
  A / Auto Process      - Apply all steps (Grayscale→Otsu→Rotate→Denoise→Solve Grid)
  C / Reset button      - Reload original image
  H / Otsu button       - Apply Otsu thresholding
  G / Grayscale button  - Convert to grayscale
  R / Rotate button     - Auto-rotate/deskew
  J / Denoise button    - Remove noise
  Ctrl+S / Save button  - Save current image
  V / Solve Grid        - Detect and solve crossword grid
  ESC/Q                 - Quit


À quoi sert chaque bouton : 
Cliquez sur les boutons ou utilisez les raccourcis clavier :
  O / Ouvrir le fichier         - Sélectionnez un nouveau fichier.
  A / Traitement automatique      - Appliquez toutes les étapes (Niveaux de gris→Otsu→Rotation→Débruitage→Résolution de la grille).
  C / Bouton Réinitialiser      - Rechargez l'image d'origine.
  H / Bouton Otsu       - Appliquez le seuillage Otsu.
  G / Bouton Niveaux de gris  - Convertir en niveaux de gris
  R / Bouton Rotation     - Rotation/redressement automatique
  J / Bouton Débruitage    - Supprimer le bruit
  Ctrl+S / Bouton Enregistrer  - Enregistrer l'image actuelle
  V / Résoudre la grille        - Détecter et résoudre la grille de mots croisés
  ESC/Q                 - Quitter

How to use Makefile:

make yes => It will build the program with file picker support
make no => It will build the program without file picker support
./ui_app => It will run the program
./ui_app test.png => It will run the program with test.png as input
make clean => It will remove all files created by the Makefile

Comment utiliser Makefile :

make yes => Il compilera le programme avec la prise en charge du sélecteur de fichiers.
make no => Il compilera le programme sans la prise en charge du sélecteur de fichiers.
./ui_app => Il exécutera le programme.
./ui_app test.png => Il exécutera le programme avec test.png comme entrée.
make clean => Il supprimera tous les fichiers créés par Makefile.

