CC = gcc
CFLAGS = -Wall -Wextra -O2 `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lSDL2_image -lSDL2_ttf -lm

ALL_SRC = $(shell find . -name "*.c")

EXCLUDE = \
	./neural_network/csv2img_simple.c \
	./neural_network/digitalisation_csv.c \
	./neural_network/main.c \
	./neural_network/neural_network.c \
	./neural_network/nn_train.c \
	./pipeline_interface/pipeline_implementation.c


SRC = $(filter-out $(EXCLUDE), $(ALL_SRC))
OBJ = $(SRC:.c=.o)

BIN = ui_app

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

image_cleaner/image_cleaner.o: image_cleaner/image_cleaner.c image_cleaner/image_cleaner.h
	$(CC) $(CFLAGS) -c $< -o $@

file_saver/file_saver.o: file_saver/file_saver.c file_saver/file_saver.h
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)
