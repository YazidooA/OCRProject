CC = gcc
CFLAGS = -Wall -Wextra -O2 `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lSDL2_image -lSDL2_ttf -lm

# Add file picker flag if USE_PICKER=1 is passed
ifeq ($(USE_PICKER),1)
    CFLAGS += -DUSE_FILE_PICKER
endif

ALL_SRC = $(shell find . -name "*.c")

EXCLUDE = \
	./neural_network/csv2img_simple.c \
	./neural_network/digitalisation_csv.c \
	./neural_network/main.c \
	./neural_network/neural_network.c \
	./neural_network/nn_train.c \
	./pipeline_interface/pipeline_implementation.c

# Exclude file_picker.c unless USE_PICKER=1
ifneq ($(USE_PICKER),1)
    EXCLUDE += ./file_picker/file_picker.c
endif

SRC = $(filter-out $(EXCLUDE), $(ALL_SRC))
OBJ = $(SRC:.c=.o)

BIN = ui_app

# Default target (no file picker)
all: $(BIN)

# Build WITH file picker 
yes:
	@echo "=== Building WITH file picker ==="
	@$(MAKE) clean
	@$(MAKE) USE_PICKER=1 $(BIN)
	@echo "✓ Built with file picker support (use 'O' key or button)"

# Build WITHOUT file picker
no:
	@echo "=== Building WITHOUT file picker ==="
	@$(MAKE) clean
	@$(MAKE) USE_PICKER=0 $(BIN)
	@echo "✓ Built without file picker (use command line args)"

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

# Explicit rules for modules
image_cleaner/image_cleaner.o: image_cleaner/image_cleaner.c image_cleaner/image_cleaner.h
	$(CC) $(CFLAGS) -c $< -o $@

file_saver/file_saver.o: file_saver/file_saver.c file_saver/file_saver.h
	$(CC) $(CFLAGS) -c $< -o $@

# Only compile file_picker when USE_PICKER=1
ifeq ($(USE_PICKER),1)
file_picker/file_picker.o: file_picker/file_picker.c file_picker/file_picker.h
	@echo "Compiling file_picker with USE_FILE_PICKER flag..."
	$(CC) $(CFLAGS) -c $< -o $@
endif

# Generic rule for all .c files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)
	rm -f file_picker/file_picker.o
	@echo "Cleaned build files"

.PHONY: all yes no clean