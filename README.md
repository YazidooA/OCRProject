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

How to use Makefile:

make yes => It will build the program with file picker support
make all/no => It will build the program without file picker support
./ui_app => It will run the program
./ui_app test.png => It will run the program with test.png as input
make clean => It will remove all files created by the Makefile


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

Comment utiliser Makefile :

make yes => Il compile le programme avec la prise en charge du sélecteur de fichiers.
make all/no => Il compile le programme sans la prise en charge du sélecteur de fichiers.
./ui_app => Il exécute le programme.
./ui_app test.png => Il exécute le programme avec test.png comme entrée.
make clean => Il supprime tous les fichiers créés par Makefile.

