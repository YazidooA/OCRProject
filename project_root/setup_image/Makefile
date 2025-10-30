# Makefile pour le projet RDN complet (avec code d'Ibrahim)

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -I.
LDFLAGS = -lSDL2 -lSDL2_image -lm

# ============================================
# Fichiers objets
# ============================================

# Core RDN
CORE_OBJS = structure_detection.o letter_extractor.o file_saver.o

# Utilitaires
UTIL_OBJS = solver.o draw_outline.o rotation.o

# Code Ibrahim
IBRAHIM_OBJS = image_cleaner.o setup_image.o

# Pipeline
PIPELINE_OBJS = pipeline_interface.o pipeline_integration.o

# Réseau de neurones
NN_OBJS = neural_network.o

# ============================================
# Cibles principales
# ============================================

.PHONY: all clean help test

all: rdn_main ibrahim_interface solver nn digitalisation draw_outline rotation

# Programme principal (exemple autonome)
rdn_main: example_rdn_usage.o $(CORE_OBJS) $(UTIL_OBJS) $(NN_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Interface d'Ibrahim (programme complet avec GUI)
ibrahim_interface: $(IBRAHIM_OBJS) $(CORE_OBJS) $(UTIL_OBJS) $(PIPELINE_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "✓ Programme principal compilé : ibrahim_interface"
	@echo "  Usage : ./ibrahim_interface <image.png>"
	@echo "  Touches :"
	@echo "    C - Recharger l'image"
	@echo "    R - Rotation automatique"
	@echo "    G - Niveaux de gris"
	@echo "    H - Seuillage Otsu"
	@echo "    J - Suppression bruit"
	@echo "    K - PIPELINE COMPLET (extraction + IA + résolution)"
	@echo ""

# Utilitaires individuels
solver: solver.o
	$(CC) -o $@ $^ $(LDFLAGS)

nn: neural_network.o
	$(CC) -o $@ $^ $(LDFLAGS)

digitalisation: digitalisation.o
	$(CC) -o $@ $^ $(LDFLAGS)

draw_outline: draw_outline.o
	$(CC) -o $@ $^ $(LDFLAGS)

rotation: rotation.o
	$(CC) -o $@ $^ $(LDFLAGS)

# ============================================
# Règles de compilation
# ============================================

# Core RDN
structure_detection.o: structure_detection.c structure_detection.h
	$(CC) $(CFLAGS) -c $<

letter_extractor.o: letter_extractor.c letter_extractor.h structure_detection.h
	$(CC) $(CFLAGS) -c $<

file_saver.o: file_saver.c file_saver.h structure_detection.h letter_extractor.h draw_outline.h
	$(CC) $(CFLAGS) -c $<

# Utilitaires
solver.o: solver.c solver.h
	$(CC) $(CFLAGS) -c $<

draw_outline.o: draw_outline.c draw_outline.h
	$(CC) $(CFLAGS) -c $<

rotation.o: rotation.c rotation.h
	$(CC) $(CFLAGS) -c $<

neural_network.o: neural_network.c neural_network.h
	$(CC) $(CFLAGS) -c $<

digitalisation.o: digitalisation.c
	$(CC) $(CFLAGS) -c $<

# Code Ibrahim
image_cleaner.o: image_cleaner.c image_cleaner.h
	$(CC) $(CFLAGS) -c $<

setup_image.o: setup_image.c setup_image.h image_cleaner.h rotation.h
	$(CC) $(CFLAGS) -c $<

# Pipeline
pipeline_interface.o: pipeline_interface.c pipeline_interface.h structure_detection.h letter_extractor.h solver.h draw_outline.h
	$(CC) $(CFLAGS) -c $<

pipeline_integration.o: pipeline_integration.c pipeline_interface.h image_cleaner.h setup_image.h
	$(CC) $(CFLAGS) -c $<

# Exemples
example_rdn_usage.o: example_rdn_usage.c structure_detection.h letter_extractor.h file_saver.h solver.h neural_network.h
	$(CC) $(CFLAGS) -c $<

# ============================================
# Tests
# ============================================

test: test_pipeline test_solver test_nn

test_pipeline: ibrahim_interface
	@echo "\n=== Test du pipeline ==="
	@if [ -f test_grid.png ]; then \
		./ibrahim_interface test_grid.png; \
	else \
		echo "⚠ Fichier test_grid.png non trouvé"; \
	fi

test_solver: solver
	@echo "\n=== Test du solver ==="
	@if [ -f test_grid.txt ]; then \
		./solver test_grid.txt HELLO; \
	else \
		echo "⚠ Fichier test_grid.txt non trouvé"; \
	fi

test_nn: nn
	@echo "\n=== Test du réseau de neurones ==="
	@if [ -f training_data.csv ]; then \
		./nn test training_data.csv model.bin; \
	else \
		echo "⚠ Fichier training_data.csv non trouvé"; \
	fi

# ============================================
# Nettoyage
# ============================================

clean:
	rm -f *.o
	rm -f rdn_main ibrahim_interface solver nn digitalisation draw_outline rotation
	rm -f *_solved.png *_results.txt *_grid.txt *_preprocessed.png
	rm -f grid_for_ia.csv grid_for_solver.txt
	rm -f *.bmp rotated.bmp
	@echo "✓ Nettoyage terminé"

clean_outputs:
	rm -f *_solved.png *_results.txt *_grid.txt *_preprocessed.png
	rm -f grid_for_ia.csv grid_for_solver.txt
	@echo "✓ Fichiers de sortie supprimés"

# ============================================
# Aide
# ============================================

help:
	@echo "╔══════════════════════════════════════════════════════════╗"
	@echo "║  Makefile - Projet RDN (Reconnaissance de Documents)    ║"
	@echo "╚══════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "Cibles disponibles :"
	@echo ""
	@echo "  all              - Compile tous les programmes"
	@echo "  ibrahim_interface - Programme principal avec GUI (RECOMMANDÉ)"
	@echo "  rdn_main         - Exemple autonome"
	@echo "  solver           - Résolveur de grilles"
	@echo "  nn               - Réseau de neurones"
	@echo "  digitalisation   - Création de datasets"
	@echo "  draw_outline     - Utilitaire de dessin"
	@echo "  rotation         - Utilitaire de rotation"
	@echo ""
	@echo "Tests :"
	@echo ""
	@echo "  test             - Lance tous les tests"
	@echo "  test_pipeline    - Test du pipeline complet"
	@echo "  test_solver      - Test du solver"
	@echo "  test_nn          - Test du réseau de neurones"
	@echo ""
	@echo "Nettoyage :"
	@echo ""
	@echo "  clean            - Supprime tous les fichiers compilés"
	@echo "  clean_outputs    - Supprime les fichiers de sortie"
	@echo ""
	@echo "═══════════════════════════════════════════════════════════"
	@echo ""
	@echo "UTILISATION PRINCIPALE (Ibrahim) :"
	@echo ""
	@echo "  1. Compiler :"
	@echo "     make ibrahim_interface"
	@echo ""
	@echo "  2. Lancer :"
	@echo "     ./ibrahim_interface image_grille.png"
	@echo ""
	@echo "  3. Touches dans l'interface :"
	@echo "     C - Recharger l'image"
	@echo "     R - Rotation automatique"
	@echo "     G - Niveaux de gris"
	@echo "     H - Seuillage Otsu"
	@echo "     J - Suppression bruit"
	@echo "     K - PIPELINE COMPLET 🎯"
	@echo ""
	@echo "═══════════════════════════════════════════════════════════"
	@echo ""
	@echo "WORKFLOW COMPLET :"
	@echo ""
	@echo "  1. Entraîner le réseau de neurones :"
	@echo "     ./digitalisation dataset.csv images/*.png"
	@echo "     ./nn train dataset.csv model.bin"
	@echo ""
	@echo "  2. Utiliser l'interface :"
	@echo "     ./ibrahim_interface grille.png"
	@echo "     Appuyer sur K pour lancer le pipeline"
	@echo ""
	@echo "  3. Le pipeline génère automatiquement :"
	@echo "     - grid_for_ia.csv (pour votre IA)"
	@echo "     - grid_for_solver.txt (pour le solver)"
	@echo "     - *_solved.png (image annotée)"
	@echo "     - *_preprocessed.png (image nettoyée)"
	@echo ""
	@echo "═══════════════════════════════════════════════════════════"

# ============================================
# Installation des dépendances
# ============================================

install_deps:
	@echo "Installation des dépendances (Debian/Ubuntu)..."
	sudo apt-get update
	sudo apt-get install -y libsdl2-dev libsdl2-image-dev
	@echo "✓ Dépendances installées"

check_deps:
	@echo "Vérification des dépendances..."
	@pkg-config --exists sdl2 && echo "✓ SDL2 installé" || echo "✗ SDL2 manquant"
	@pkg-config --exists SDL2_image && echo "✓ SDL2_image installé" || echo "✗ SDL2_image manquant"
	@which gcc > /dev/null && echo "✓ GCC installé" || echo "✗ GCC manquant"

# ============================================
# Exemples de commandes
# ============================================

.PHONY: example

example:
	@echo ""
	@echo "╔══════════════════════════════════════════════════════════╗"
	@echo "║  EXEMPLES DE COMMANDES                                   ║"
	@echo "╚══════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "# Compiler le projet complet"
	@echo "make all"
	@echo ""
	@echo "# Lancer l'interface d'Ibrahim"
	@echo "./ibrahim_interface grille.png"
	@echo ""
	@echo "# Créer un dataset d'entraînement"
	@echo "./digitalisation dataset.csv images/A_*.png images/B_*.png"
	@echo ""
	@echo "# Entraîner le réseau de neurones"
	@echo "./nn train dataset.csv model.bin"
	@echo ""
	@echo "# Tester le réseau de neurones"
	@echo "./nn test test_data.csv model.bin"
	@echo ""
	@echo "# Résoudre une grille manuellement"
	@echo "./solver grid.txt HELLO"
	@echo ""
	@echo "# Dessiner un contour sur une image"
	@echo "./draw_outline input.png output.png"
	@echo ""
	@echo "# Faire pivoter une image"
	@echo "./rotation image.png 45"
	@echo ""