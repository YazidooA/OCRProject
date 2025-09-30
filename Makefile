CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS_SOLVER =
LIBS_ROTATION = -lSDL2 -lSDL2_image -lm

OUTDIR = output

SRC_SOLVER = solver/solver.c
SRC_ROTATION = rotation/rotation.c

BIN_SOLVER = $(OUTDIR)/solver_compiled
BIN_ROTATION = $(OUTDIR)/rotation_compiled

all: solver rotation

solver: $(BIN_SOLVER)

$(BIN_SOLVER): $(SRC_SOLVER) solver/solver.h | $(OUTDIR)
	$(CC) $(CFLAGS) $(SRC_SOLVER) -o $(BIN_SOLVER) $(LIBS_SOLVER)

rotation: $(BIN_ROTATION)

$(BIN_ROTATION): $(SRC_ROTATION) rotation/rotation.h | $(OUTDIR)
	$(CC) $(CFLAGS) $(SRC_ROTATION) -o $(BIN_ROTATION) $(LIBS_ROTATION)

$(OUTDIR):
	mkdir -p $(OUTDIR)

clean:
	rm -f $(BIN_SOLVER) $(BIN_ROTATION)
	rm -rf $(OUTDIR)
