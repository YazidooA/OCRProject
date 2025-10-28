#ifndef TEXTIO_H
#define TEXTIO_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

typedef char bstring[50];
typedef char string[];

bstring* get_text(void);
void give_text(bstring* text, string namefile);

int run_myvim(int argc, char *argv[]);

#endif
