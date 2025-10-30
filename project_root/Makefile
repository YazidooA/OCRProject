CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS_SOLVER =
LIBS_ROTATION = -lSDL2 -lSDL2_image -lm
LIBS_OUTLINE = -lSDL2 -lSDL2_image -lm

OUTDIR = output

SRC_SOLVER = solver/solver.c
SRC_ROTATION = rotation/rotation.c
SRC_OUTLINE = draw_outline/draw_outline.c

BIN_SOLVER = $(OUTDIR)/solver_compiled
BIN_ROTATION = $(OUTDIR)/rotation_compiled
BIN_OUTLINE = $(OUTDIR)/draw_outline_compiled

all: solver rotation draw_outline

solver: $(BIN_SOLVER)

$(BIN_SOLVER): $(SRC_SOLVER) solver/solver.h | $(OUTDIR)
	$(CC) $(CFLAGS) $(SRC_SOLVER) -o $(BIN_SOLVER) $(LIBS_SOLVER)

rotation: $(BIN_ROTATION)

$(BIN_ROTATION): $(SRC_ROTATION) rotation/rotation.h | $(OUTDIR)
	$(CC) $(CFLAGS) $(SRC_ROTATION) -o $(BIN_ROTATION) $(LIBS_ROTATION)

draw_outline: $(BIN_OUTLINE)

$(BIN_OUTLINE): $(SRC_OUTLINE) draw_outline/draw_outline.h | $(OUTDIR)
	$(CC) $(CFLAGS) $(SRC_OUTLINE) -o $(BIN_OUTLINE) $(LIBS_OUTLINE)

$(OUTDIR):
	mkdir -p $(OUTDIR)

clean:
	rm -f $(BIN_SOLVER) $(BIN_ROTATION) $(BIN_OUTLINE)
	rm -rf $(OUTDIR)
