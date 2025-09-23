# ===========================================
# Makefile pour OCR Word Search Solver
# ===========================================

# Compilateur et flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O2
CFLAGS_DEBUG = -Wall -Wextra -std=c99 -g -O0 -DDEBUG -fsanitize=address
CFLAGS_RELEASE = -Wall -Wextra -std=c99 -O3 -DNDEBUG

# Bibliothèques
LIBS = -lm -lsdl2 -lsdl2_image -lpng -ljpeg -lblas -llapack
GTK_FLAGS = `pkg-config --cflags --libs gtk+-3.0`
LDFLAGS = $(LIBS) $(GTK_FLAGS)

# Répertoires
SRCDIR = src
INCDIR = include
OBJDIR = obj
TESTDIR = tests
BINDIR = bin

# Fichiers sources organisés par modules
CORE_SRC = $(SRCDIR)/main.c

IMAGE_SRC = $(wildcard $(SRCDIR)/image_processing/*.c)
NEURAL_SRC = $(wildcard $(SRCDIR)/neural_network/*.c)
SOLVER_SRC = $(wildcard $(SRCDIR)/word_search_solver/*.c)
GUI_SRC = $(wildcard $(SRCDIR)/gui/*.c)
UTILS_SRC = $(wildcard $(SRCDIR)/utils/*.c)

# Tous les fichiers sources (sauf main.c et solver.c)
ALL_SRC = $(IMAGE_SRC) $(NEURAL_SRC) $(SOLVER_SRC) $(GUI_SRC) $(UTILS_SRC)
MAIN_SRC = $(CORE_SRC) $(ALL_SRC)

# Fichiers objets correspondants
CORE_OBJ = $(CORE_SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
IMAGE_OBJ = $(IMAGE_SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
NEURAL_OBJ = $(NEURAL_SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
SOLVER_OBJ = $(SOLVER_SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
GUI_OBJ = $(GUI_SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
UTILS_OBJ = $(UTILS_SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

ALL_OBJ = $(ALL_SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
MAIN_OBJ = $(MAIN_SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Fichiers de test
TEST_SRC = $(wildcard $(TESTDIR)/*.c)
TEST_OBJ = $(TEST_SRC:$(TESTDIR)/%.c=$(OBJDIR)/tests/%.o)

# Exécutables
MAIN_TARGET = $(BINDIR)/word_search_solver
SOLVER_TARGET = $(BINDIR)/solver
TEST_TARGET = $(BINDIR)/run_tests

# Fichiers headers pour dépendances
HEADERS = $(wildcard $(INCDIR)/*.h $(INCDIR)/**/*.h)

# ===========================================
# RÈGLES PRINCIPALES
# ===========================================

.PHONY: all clean debug release test install uninstall help

# Cible par défaut
all: $(MAIN_TARGET) $(SOLVER_TARGET)

# Version debug avec AddressSanitizer
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: $(MAIN_TARGET) $(SOLVER_TARGET)

# Version release optimisée
release: CFLAGS = $(CFLAGS_RELEASE)
release: $(MAIN_TARGET) $(SOLVER_TARGET)

# ===========================================
# CONSTRUCTION DES EXÉCUTABLES
# ===========================================

# Application principale avec GUI
$(MAIN_TARGET): $(MAIN_OBJ) | $(BINDIR)
	$(CC) $(MAIN_OBJ) -o $@ $(LDFLAGS)
	@echo "✅ Application principale créée : $@"

# Programme solver en ligne de commande (obligatoire pour soutenance 1)
$(SOLVER_TARGET): $(OBJDIR)/word_search_solver/solver.o $(OBJDIR)/word_search_solver/solver_core.o $(UTILS_OBJ) | $(BINDIR)
	$(CC) $^ -o $@ -lm
	@echo "✅ Programme solver créé : $@"

# Tests unitaires
$(TEST_TARGET): $(TEST_OBJ) $(ALL_OBJ) | $(BINDIR)
	$(CC) $^ -o $@ $(LDFLAGS)
	@echo "✅ Tests créés : $@"

# ===========================================
# COMPILATION DES OBJETS
# ===========================================

# Règle générale pour tous les fichiers objets
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS) | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) `pkg-config --cflags gtk+-3.0` -I$(INCDIR) -c $< -o $@
	@echo "🔨 Compilé : $<"

# Compilation des tests
$(OBJDIR)/tests/%.o: $(TESTDIR)/%.c $(HEADERS) | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@
	@echo "🧪 Test compilé : $<"

# ===========================================
# CIBLES DE MODULES INDIVIDUELS
# ===========================================

# Module traitement d'image (Membre B)
image: $(IMAGE_OBJ)
	@echo "📷 Module image processing compilé"

# Module réseau de neurones (Membre C)
neural: $(NEURAL_OBJ)
	@echo "🤖 Module neural network compilé"

# Module solver (Membre D)
solver: $(SOLVER_OBJ)
	@echo "🧩 Module word search solver compilé"

# Module GUI (Membre D)
gui: $(GUI_OBJ)
	@echo "🖥️  Module GUI compilé"

# Module utilitaires
utils: $(UTILS_OBJ)
	@echo "🔧 Module utils compilé"

# ===========================================
# TESTS ET VALIDATION
# ===========================================

# Exécuter tous les tests
test: $(TEST_TARGET)
	@echo "🧪 Exécution des tests..."
	./$(TEST_TARGET)

# Test spécifique du solver (obligatoire soutenance 1)
test-solver: $(SOLVER_TARGET)
	@echo "🧩 Test du programme solver..."
	@echo "HELLO" > test_grid.txt
	@echo "WORLD" >> test_grid.txt
	./$(SOLVER_TARGET) test_grid.txt HELLO
	./$(SOLVER_TARGET) test_grid.txt WORLD
	./$(SOLVER_TARGET) test_grid.txt NOTFOUND
	@rm -f test_grid.txt

# Test des fuites mémoire avec valgrind
memcheck: debug
	@echo "🔍 Vérification mémoire avec valgrind..."
	valgrind --leak-check=full --show-leak-kinds=all ./$(MAIN_TARGET)

# Vérification du code avec cppcheck (optionnel)
static-check:
	@echo "🔍 Analyse statique du code..."
	cppcheck --enable=all --inconclusive --std=c99 $(SRCDIR)/

# ===========================================
# INSTALLATION ET DÉPLOIEMENT
# ===========================================

# Installation système (nécessite sudo)
install: release
	@echo "📦 Installation du système..."
	sudo cp $(MAIN_TARGET) /usr/local/bin/
	sudo cp $(SOLVER_TARGET) /usr/local/bin/
	sudo mkdir -p /usr/local/share/word-search-solver
	sudo cp -r images/samples /usr/local/share/word-search-solver/
	@echo "✅ Installation terminée"

# Désinstallation
uninstall:
	@echo "🗑️  Désinstallation..."
	sudo rm -f /usr/local/bin/word_search_solver
	sudo rm -f /usr/local/bin/solver
	sudo rm -rf /usr/local/share/word-search-solver
	@echo "✅ Désinstallation terminée"

# ===========================================
# DOCUMENTATION ET LIVRABILES
# ===========================================

# Génération de la documentation (si Doxygen installé)
doc:
	@if command -v doxygen >/dev/null 2>&1; then \
		echo "📚 Génération de la documentation..."; \
		doxygen Doxyfile; \
	else \
		echo "⚠️  Doxygen non installé. Documentation non générée."; \
	fi

# Préparation pour soutenance (compile tout + tests)
soutenance: clean release test-solver
	@echo "🎯 Préparation soutenance terminée!"
	@echo "📁 Vérifiez que tous les fichiers sont présents:"
	@ls -la $(BINDIR)/
	@echo "📄 N'oubliez pas le rapport et le plan de soutenance!"

# Package pour rendu (sans les binaires)
package:
	@echo "📦 Création du package de rendu..."
	tar -czf ocr_word_search_$(shell date +%Y%m%d).tar.gz \
		--exclude='$(OBJDIR)' \
		--exclude='$(BINDIR)' \
		--exclude='*.tar.gz' \
		--exclude='.git' \
		src/ include/ tests/ images/ Makefile README AUTHORS *.pdf
	@echo "✅ Package créé : ocr_word_search_$(shell date +%Y%m%d).tar.gz"

# ===========================================
# NETTOYAGE
# ===========================================

# Nettoyage des fichiers objets et exécutables
clean:
	@echo "🧹 Nettoyage..."
	rm -rf $(OBJDIR)
	rm -rf $(BINDIR)
	rm -f *.tar.gz
	rm -f test_grid.txt
	@echo "✅ Nettoyage terminé"

# Nettoyage complet (inclut documentation)
clean-all: clean
	rm -rf doc/
	@echo "✅ Nettoyage complet terminé"

# ===========================================
# CRÉATION DES RÉPERTOIRES
# ===========================================

$(OBJDIR):
	mkdir -p $(OBJDIR)
	mkdir -p $(OBJDIR)/image_processing
	mkdir -p $(OBJDIR)/neural_network
	mkdir -p $(OBJDIR)/word_search_solver
	mkdir -p $(OBJDIR)/gui
	mkdir -p $(OBJDIR)/utils
	mkdir -p $(OBJDIR)/tests

$(BINDIR):
	mkdir -p $(BINDIR)

# ===========================================
# INFORMATIONS ET AIDE
# ===========================================

# Affichage de l'aide
help:
	@echo "📋 OCR Word Search Solver - Makefile"
	@echo "======================================"
	@echo ""
	@echo "🎯 CIBLES PRINCIPALES:"
	@echo "  all          - Compile tout (défaut)"
	@echo "  debug        - Compile en mode debug"
	@echo "  release      - Compile en mode release"
	@echo "  clean        - Nettoie les fichiers générés"
	@echo ""
	@echo "🏗️  MODULES:"
	@echo "  image        - Module traitement d'image"
	@echo "  neural       - Module réseau de neurones"
	@echo "  solver       - Module résolution grilles"
	@echo "  gui          - Module interface graphique"
	@echo "  utils        - Module utilitaires"
	@echo ""
	@echo "🧪 TESTS:"
	@echo "  test         - Lance tous les tests"
	@echo "  test-solver  - Test spécifique du solver"
	@echo "  memcheck     - Vérification mémoire (valgrind)"
	@echo ""
	@echo "🎓 SOUTENANCE:"
	@echo "  soutenance   - Préparation complète"
	@echo "  package      - Création package rendu"
	@echo ""
	@echo "📦 SYSTÈME:"
	@echo "  install      - Installation système"
	@echo "  uninstall    - Désinstallation"
	@echo ""

# Informations sur la compilation
info:
	@echo "ℹ️  INFORMATIONS DE COMPILATION"
	@echo "==============================="
	@echo "Compilateur    : $(CC)"
	@echo "Flags C        : $(CFLAGS)"
	@echo "Bibliothèques  : $(LIBS)"
	@echo "GTK Flags      : $(GTK_FLAGS)"
	@echo "Répertoires    : src=$(SRCDIR), obj=$(OBJDIR), bin=$(BINDIR)"
	@echo "Fichiers src   : $(words $(MAIN_SRC)) fichiers"
	@echo "Headers        : $(words $(HEADERS)) fichiers"

# Vérification des dépendances
check-deps:
	@echo "🔍 VÉRIFICATION DES DÉPENDANCES"
	@echo "==============================="
	@echo -n "GCC: "
	@command -v gcc >/dev/null 2>&1 && echo "✅ Installé" || echo "❌ Manquant"
	@echo -n "Make: "
	@command -v make >/dev/null 2>&1 && echo "✅ Installé" || echo "❌ Manquant"
	@echo -n "SDL2: "
	@pkg-config --exists sdl2 2>/dev/null && echo "✅ Installé" || echo "❌ Manquant"
	@echo -n "GTK+3: "
	@pkg-config --exists gtk+-3.0 2>/dev/null && echo "✅ Installé" || echo "❌ Manquant"
	@echo -n "libpng: "
	@pkg-config --exists libpng 2>/dev/null && echo "✅ Installé" || echo "❌ Manquant"
	@echo -n "Valgrind: "
	@command -v valgrind >/dev/null 2>&1 && echo "✅ Installé" || echo "❌ Manquant (optionnel)"

# ===========================================
# DÉPENDANCES AUTOMATIQUES
# ===========================================

# Inclusion des dépendances automatiques (optionnel)
-include $(ALL_OBJ:.o=.d)

# Génération des dépendances
$(OBJDIR)/%.d: $(SRCDIR)/%.c | $(OBJDIR)
	@mkdir -p $(dir $@)
	@$(CC) -MM -I$(INCDIR) $< > $@.tmp
	@sed 's,\($*\)\.o[ :]*,$(OBJDIR)/\1.o $@ : ,g' < $@.tmp > $@
	@rm -f $@.tmp