CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -I. -I../setup_image -I../image_cleaner -I../rotation -I../structure_detection -I../letter_extractor -I../solver -I../draw_outline -I../file_saver -I../neural_network
LDFLAGS = -lSDL2 -lSDL2_image -lm

# Pipeline sources
PIPELINE_SRC = pipeline_interface.c \
               pipeline_implementation.c \
               ../setup_image/setup_image.c \
               ../image_cleaner/image_cleaner.c \
               ../rotation/rotation.c \
               ../structure_detection/structure_detection.c \
               ../letter_extractor/letter_extractor.c \
               ../solver/solver.c \
               ../draw_outline/draw_outline.c \
               ../file_saver/file_saver.c

PIPELINE_OBJ = $(PIPELINE_SRC:.c=.o)

# Compile neural_network.c as library (no main)
nn_lib.o: ../neural_network/neural_network.c
	$(CC) $(CFLAGS) -c $< -o $@

# Main pipeline executable
ibrahim_interface: $(PIPELINE_OBJ) nn_lib.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(PIPELINE_OBJ) nn_lib.o ibrahim_interface *_solved.png *_preprocessed.png *.csv *.txt
	rm -f ../setup_image/*.o ../solver/*.o ../draw_outline/*.o

.PHONY: clean