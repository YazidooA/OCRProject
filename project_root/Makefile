CC = gcc
CFLAGS = -Wall -Wextra -g

# Libs par module
LIBS_SOLVER       =
LIBS_ROTATION     = -lSDL2 -lSDL2_image -lm
LIBS_OUTLINE      = -lSDL2 -lSDL2_image -lm
LIBS_NN           = -lm
LIBS_DIGITALISATION = -lSDL2 -lSDL2_image -lm

OUTDIR = output

# Sources
SRC_SOLVER          = solver/solver.c
SRC_ROTATION        = rotation/rotation.c
SRC_OUTLINE         = draw_outline/draw_outline.c
SRC_NN              = neural_network/nn.c
SRC_DIGITALISATION  = neural_network/digitalisation.c
SRC_DIGITALISATIONCSV  = neural_network/digitalisation_csv.c

# Binaries
BIN_SOLVER          = $(OUTDIR)/solver_compiled
BIN_ROTATION        = $(OUTDIR)/rotation_compiled
BIN_OUTLINE         = $(OUTDIR)/draw_outline_compiled
BIN_NN              = $(OUTDIR)/nn
BIN_DIGITALISATION  = $(OUTDIR)/digitalisation_compiled
BIN_DIGITALISATIONCSV  = $(OUTDIR)/digitalisation_csv_compiled

.PHONY: all solver rotation draw_outline nn digitalisation clean

all: solver rotation draw_outline nn digitalisation

# ----- solver -----
solver: $(BIN_SOLVER)

$(BIN_SOLVER): $(SRC_SOLVER) solver/solver.h | $(OUTDIR)
	$(CC) $(CFLAGS) -I solver $(SRC_SOLVER) -o $(BIN_SOLVER) $(LIBS_SOLVER)

# ----- rotation -----
rotation: $(BIN_ROTATION)

$(BIN_ROTATION): $(SRC_ROTATION) rotation/rotation.h | $(OUTDIR)
	$(CC) $(CFLAGS) -I rotation $(SRC_ROTATION) -o $(BIN_ROTATION) $(LIBS_ROTATION)

# ----- draw_outline -----
draw_outline: $(BIN_OUTLINE)

$(BIN_OUTLINE): $(SRC_OUTLINE) draw_outline/draw_outline.h | $(OUTDIR)
	$(CC) $(CFLAGS) -I draw_outline $(SRC_OUTLINE) -o $(BIN_OUTLINE) $(LIBS_OUTLINE)

# ----- neural network -----
nn: $(BIN_NN)

$(BIN_NN): $(SRC_NN) neural_network/nn.h | $(OUTDIR)
	$(CC) $(CFLAGS) -I neural_network $(SRC_NN) -o $(BIN_NN) $(LIBS_NN)

# ----- digitalisation (image -> CSV 28x28) -----
digitalisation: $(BIN_DIGITALISATIONCSV)

$(BIN_DIGITALISATIONCSV): $(SRC_DIGITALISATIONCSV) neural_network/nn.h | $(OUTDIR)
	$(CC) $(CFLAGS) -I neural_network $(SRC_DIGITALISATIONCSV) -o $(BIN_DIGITALISATIONCSV) $(LIBS_DIGITALISATION)

# ----- out dir -----
$(OUTDIR):
	mkdir -p $(OUTDIR)

# ----- clean -----
clean:
	rm -f $(BIN_SOLVER) $(BIN_ROTATION) $(BIN_OUTLINE) $(BIN_NN) $(BIN_DIGITALISATION)
	rm -rf $(OUTDIR)
