#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <myvim.h>

typedef char bstring[50];

// Fonction pour écrire le texte dans un fichier
void give_text(bstring* text, char* namefile) 
{
    FILE *pFile = fopen(namefile, "w");
    if (pFile == NULL)
    {
        printf("Erreur lors de l'ouverture du fichier\n");
        return;
    }
    
    for (int i = 0; i < 50; i++) 
    {
        if (text[i][0] == '\0') break; // Arrêter si ligne vide
        fprintf(pFile, "%s\n", text[i]);
    }
    
    printf("Fichier écrit avec succès!\n");
    fclose(pFile);
}

// Fonction myvim : prend le nom du fichier et un array de strings
int myvim(char* file, char* text_array[]) 
{
    // Convertir char*[] en bstring* pour compatibilité avec give_text
    bstring* text = malloc(sizeof(bstring) * 50);
    if (text == NULL) {
        printf("Erreur d'allocation mémoire\n");
        return -1;
    }
    
    // Initialiser toutes les strings à vide
    for (int i = 0; i < 50; i++) {
        text[i][0] = '\0';
    }
    
    // Copier les strings du tableau d'entrée
    int i = 0;
    while (text_array[i] != NULL && i < 50) 
    {
        strncpy(text[i], text_array[i], 49);
        text[i][49] = '\0'; // Assurer la terminaison
        i++;
    }
    
    // Écrire dans le fichier
    give_text(text, file);
    
    // Afficher le contenu
    for (int j = 0; j < i; j++) {
        printf("%s\n", text[j]);
    }
    
    // Libérer la mémoire
    free(text);
    
    return 0;
}