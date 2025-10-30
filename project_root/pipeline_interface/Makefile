CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -I. -I../setup_image -I../image_cleaner -I../rotation -I../structure_detection -I../letter_extractor -I../solver -I../draw_outline -I../file_saver
LDFLAGS = -lSDL2 -lSDL2_image -lm

SRC = pipeline_interface.c \
      pipeline_implementation.c \
      ../setup_image/setup_image.c \
      ../image_cleaner/image_cleaner.c \
      ../rotation/rotation.c \
      ../structure_detection/structure_detection.c \
      ../letter_extractor/letter_extractor.c \
      ../solver/solver.c \
      ../draw_outline/draw_outline.c \
      ../file_saver/file_saver.c

OBJ = $(SRC:.c=.o)

ibrahim_interface: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) ibrahim_interface *_solved.png *_preprocessed.png *.csv *.txt
	rm -f ../setup_image/setup_image.o ../solver/solver.o ../draw_outline/draw_outline.o

.PHONY: all clean