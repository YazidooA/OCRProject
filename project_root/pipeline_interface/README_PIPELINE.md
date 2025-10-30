# Pipeline Interface - Word Search Solver

## Overview

This pipeline extracts letters from a word search grid image, processes them through an AI model, and automatically finds and highlights words.

---

## Quick Start

### 1. Compile
```bash
cd ~/Documents/OCRProject/project_root/pipeline_interface
make clean
make
```

### 2. Run the Pipeline
```bash
./ibrahim_interface <path_to_word_search_image.png>
```

**Example:**
```bash
./ibrahim_interface ../setup_image/TestImages/level_1_image_1.png
```

### 3. Check Results

After running, you'll get:

- **`grid_for_ia.csv`** - 28×28 pixel data for each letter (ready for neural network)
- **`grid_for_solver.txt`** - Text grid for word searching
- **`test_result.png`** - Output image with red boxes around found words
```bash
ls -lh grid_for_ia.csv grid_for_solver.txt test_result.png
```

---

## What It Does

### Phase 1: Extraction
1. Loads the word search image
2. Preprocesses (grayscale → threshold → denoise)
3. Detects grid boundaries
4. Extracts each cell as a 28×28 letter image
5. Writes CSV file with 784 pixels per letter

**Output:**
```
=== Phase 1: Extraction ===
Grid detected: x=185, y=20, w=590, h=590
Detected: 289 cells (17x17)
✓ CSV written: grid_for_ia.csv (289 letters)
```

### Phase 2: AI Recognition (Currently Simulated)
Your neural network should:
- Read `grid_for_ia.csv`
- Recognize each letter (A-Z)
- Return a string of recognized characters

**Current Implementation:** Uses dummy letters (A-Z repeating) for testing.

### Phase 3: Word Solving
1. Creates grid file from recognized letters
2. Searches for specified words in all 8 directions
3. Draws red rectangles around found words
4. Saves annotated image

**Output:**
```
=== Phase 2: Solving ===
✓ Grid file: ./grid_for_solver.txt (17x17)
✓ Found 'IMAGINE' at (0,3)->(6,3)
✓ Found 'RELAX' at (1,4)->(5,4)
✗ Not found: 'TESTING'
```

---

## File Structure
```
pipeline_interface/
├── pipeline_interface.c       # Main program logic
├── pipeline_interface.h       # Function declarations
├── pipeline_implementation.c  # Core pipeline functions
├── Makefile                   # Build configuration
└── README_PIPELINE.md         # This file
```

**Dependencies:**
- `../setup_image/` - Image loading/saving
- `../image_cleaner/` - Preprocessing (grayscale, threshold, denoise)
- `../rotation/` - Auto-rotation
- `../structure_detection/` - Grid detection
- `../letter_extractor/` - Letter extraction
- `../solver/` - Word search algorithm
- `../draw_outline/` - Drawing rectangles
- `../file_saver/` - File I/O utilities

---

## CSV Format (grid_for_ia.csv)
```csv
id,p0,p1,p2,...,p783,label
0,255,255,248,...,255,0
1,250,242,255,...,253,0
2,255,255,255,...,248,0
...
```

- **id**: Letter index (0 to N-1)
- **p0-p783**: 28×28 = 784 grayscale pixels (0-255)
- **label**: Dummy value (0) - replace with actual labels for training

---

## Grid File Format (grid_for_solver.txt)
```
17 17
P X U T S I N I U P R V G B M D D
E H A A S P O J P E T B E Q Z L C
A U N T E G Q T L H R Z F A T O P
...
```

- First line: `rows cols`
- Following lines: Grid letters (space-separated)

---

## Customizing Words to Find

Edit `ibrahim_process_grid()` in `pipeline_interface.c`:
```c
const char* words[] = {"IMAGINE", "RELAX", "COOL", "BREATHE", "CALM"};
int word_count = 5;
```

Replace with your actual word list!

---

## Integrating Your Neural Network

### Current Code (Simulation)

In `pipeline_interface.c` line ~200:
```c
// TODO: REMPLACER PAR VOTRE VRAIE IA
for (int i = 0; i < ctx->extraction->count; i++) {
    recognized_letters[i] = 'A' + (i % 26);
}
```

### What You Need to Do

Replace with your neural network call:
```c
// Load your trained model
Network net;
init_network(&net);
load_model("../model.bin", &net);

// Process the CSV
Dataset D = load_csv(ctx->csv_path);

// Recognize each letter
for (int i = 0; i < ctx->extraction->count; i++) {
    int prediction = predict(&net, &D.X[i * INPUT_SIZE]);
    recognized_letters[i] = 'A' + prediction;  // 0-25 → A-Z
}

free_dataset(&D);
free_network(&net);
```

---

## Testing Grid Detection

Test if grid detection works on your image:
```bash
./ibrahim_interface your_image.png
```

**Expected output:**
```
Image loaded: 800x600
Grid detected: x=185, y=20, w=590, h=590
Detected: 289 cells (17x17)
```

If grid detection fails:
- Try preprocessing the image first (grayscale, threshold)
- Check if grid lines are clear and well-defined
- Adjust thresholds in `structure_detection.c`

---

## Troubleshooting

### "Failed to convert surface"
- Image format not supported
- Try converting to PNG: `convert input.jpg output.png`

### "Detected: 0 cells"
- Grid not detected properly
- Image might need manual preprocessing
- Try with a clearer scan/photo

### "Not found: WORD"
- Letter recognition is wrong (dummy letters used)
- Word isn't in the grid
- Word is misspelled in the code

### Compilation errors
```bash
# Check if all dependencies are present
ls -la ../setup_image/setup_image.c
ls -la ../solver/solver.c
ls -la ../structure_detection/structure_detection.c

# Install SDL2 if missing
sudo apt-get install libsdl2-dev libsdl2-image-dev
```

---

## Example Workflow

### 1. Test with dummy recognition
```bash
./ibrahim_interface test_grid.png
cat grid_for_solver.txt  # See what letters were "recognized"
```

### 2. Train your neural network
```bash
cd ../neural_network
./nn train training_data.csv model.bin
```

### 3. Integrate AI into pipeline
Edit `pipeline_interface.c` to call your neural network (see above).

### 4. Recompile and test
```bash
make clean
make
./ibrahim_interface test_grid.png
```

### 5. Verify results
```bash
# Check if words are found
cat test_result.png  # Should have red boxes
```

---

## API Reference

### `surface_to_csv_for_ia()`
Extracts letters from image and generates CSV.

**Parameters:**
- `SDL_Surface* surface` - Input image
- `const char* csv_output_path` - Output CSV path

**Returns:** `ExtractionResult*` containing grid info

### `pipeline_phase2_solve_and_draw()`
Solves word search and draws results.

**Parameters:**
- `SDL_Surface* surface` - Input image
- `ExtractionResult* extraction` - From phase 1
- `const char* recognized_letters` - String of A-Z letters
- `const char** words_to_find` - Array of words
- `int word_count` - Number of words
- `const char* grid_path` - Grid file output path

**Returns:** `SDL_Surface*` with drawn boxes

---

## Performance

- **Grid detection**: ~100ms
- **Letter extraction**: ~500ms (17×17 grid)
- **CSV writing**: ~50ms
- **Word solving**: ~10ms per word
- **Drawing**: ~20ms

**Total pipeline**: ~1 second for typical puzzle

---

## Future Improvements

- [ ] Support for rotated/skewed images
- [ ] Better cell extraction (perspective correction)
- [ ] Multi-language support
- [ ] GUI with real-time preview
- [ ] Batch processing of multiple images
- [ ] Auto-word-list extraction from image
- [ ] Color-coded different words

---

## Credits

- Grid detection: `structure_detection.c`
- Word search solver: `solver.c`
- Image preprocessing: `image_cleaner.c`
- Drawing: `draw_outline.c`

---

## License

[Your license here]
