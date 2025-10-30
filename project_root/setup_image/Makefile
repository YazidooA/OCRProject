# Makefile for setup_image.c SDL2 program

# Compiler
CC = gcc

# Directories
IMG_CLEANER_DIR = ../image_cleaner
ROTATION_DIR = ../rotation

# Source files
SRC = setup_image.c \
      $(wildcard $(IMG_CLEANER_DIR)/*.c) \
      $(wildcard $(ROTATION_DIR)/*.c)

# Output binary
OUT = setup_image

# SDL2 flags (include and lib paths)
CFLAGS = $(shell sdl2-config --cflags) -I$(IMG_CLEANER_DIR) -I$(ROTATION_DIR)
LDFLAGS = $(shell sdl2-config --libs) -lSDL2_image -lm

# Default target
all: $(OUT)

# Compile and link
$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(LDFLAGS)

# Clean up
clean:
	rm -f $(OUT)