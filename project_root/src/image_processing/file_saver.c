#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <textio.h>

typedef char bstring[50];
typedef char string[];
bstring* get_text()
{
    bstring* text = malloc(sizeof(bstring)*50);
    for (int i = 0; i<50 ;i++)
    {
        fgets(text[i], 50, stdin);
        text[i][strcspn(text[i], "\n")] = 0;
        if (strcmp(text[i],"stop")==0) 
        {
            text[i][0] = '\0';
            break;
        }
    }
    return text;
}

void give_text(bstring* text, string namefile) 
{

    FILE *pFile = fopen(namefile, "w");
    if(pFile == NULL)
    {
        printf("Error opening file\n");
        return;
    }
    for(int i=0; i<50; i++) fprintf(pFile, "%s\n", text[i]);
    printf("File was written successfully!\n");
    fclose(pFile);
    return;
}

int myvim(int argc, char *argv[]) 
{
    if (argc < 2) {
        printf("Usage: %s <fichier>\n", argv[0]);
        return 1;
    }
    bstring* text= get_text();
    if (!text) {
        printf("Memory allocation failed!\n");
        return 1;
    }
    give_text(text,argv[1]);
    for(int i=0;i<50;i++) printf("%s\n",text[i]);
    free(text);
    text=NULL;
    return 0;
}