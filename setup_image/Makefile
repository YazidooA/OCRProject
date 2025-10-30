# Makefile for setup_image.c SDL2 program

# Compiler
CC = gcc

# Source files
SRC = setup_image.c ../image_cleaner/image_cleaner.c

# Output binary
OUT = setup_image

# SDL2 flags (include and lib paths)
CFLAGS = $(shell sdl2-config --cflags) -I../image_cleaner
LDFLAGS = $(shell sdl2-config --libs) -lSDL2_image -lm

# Default target
all: $(OUT)

# Compile
$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(LDFLAGS)

# Clean up build
clean:
	rm -f $(OUT)
