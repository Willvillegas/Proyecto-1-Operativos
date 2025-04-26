#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>

#define DEBUG printf("Aqui\n");

int size = 0;

typedef struct tree{
    unsigned char symbol;
    unsigned long int bits;
    char nBits;
    struct tree *left;
    struct tree *right;
} Node;

void createTree(Node *current, Node *newNode, Node *tree, FILE *fi, int elements);
void decompress(Node *current, Node *newNode, Node *tree, FILE *fi, long int characters, char *directory);
void deleteTree(Node *n);

void deleteTree(Node *n)
{
   if(n->left) deleteTree(n->left);
   if(n->right)  deleteTree(n->right);
   free(n);
}

void decompress(Node *current, Node *newNode, Node *tree, FILE *fi, long int characters, char *directory){
    int cant = 0;
    char filePath[1024];
    char fullPath[2048];
    
    for (int i = 0; i < 1024; i ++){
        filePath[i] = '\0';
    }
    for (int i = 0; i < 2048; i ++){
        fullPath[i] = '\0';
    }
    unsigned int cantBook = 0;
    fread(&cant, sizeof(int), 1, fi);
    fread(&filePath, sizeof(char[cant]), 1, fi);
    fread(&cantBook, sizeof(unsigned int), 1, fi);
    unsigned long int bits = 0;
    unsigned char temp = 0;
    int j;
    int cuenta = 0;
    //printf("Son %li caracteres\n", characters);
    while(1){
        snprintf(fullPath, sizeof(fullPath), "%s/%s", directory, filePath);
        //printf("%s\n", filePath);
        FILE *fs = fopen(fullPath, "w");
        
        bits = 0;
        fread(&temp, sizeof(char), 1, fi);
        bits |= temp;
        j = 0;
        newNode = tree;
        do {
            if(bits & 0x80) 
                newNode = newNode->right; 
            else 
                newNode = newNode->left;
            bits <<= 1;           
            j++;
            if(!newNode->right && !newNode->left)          
            {
                putc(newNode->symbol, fs);           
                cantBook--;                   
                characters--;
                newNode= tree;                      
            }
            if(cantBook <= 0)
                break;
            if(8 == j)            
            {
                fread(&temp, sizeof(char), 1, fi);
                bits |= temp;                    
                j = 0;                        
            }                                
            
        } while(cantBook);
        if(characters <= 0){
            
            break;
        }
        cantBook = 0;
        cant = 0;
        fread(&cant, sizeof(int), 1, fi);
        for (int i = 0; i < 1024; i ++)
            filePath[i] = '\0';
        fread(&filePath, sizeof(char[cant]), 1, fi);
        fread(&cantBook, sizeof(unsigned int), 1, fi);
        temp = 0;
        fclose(fs);
        cuenta++;
        //printf("Nombre: %s\n", filePath);
        //printf("faltan %li caracteres\n", characters);
    }
}


void createTree(Node *current, Node *newNode, Node *tree, FILE *fi, int elements){
    int j;
    for(int i = 0; i < elements; i++){
        current = (Node *)malloc(sizeof(Node));
        fread(&current->symbol, sizeof(char), 1, fi); 
        fread(&current->bits, sizeof(unsigned long int), 1, fi);
        fread(&current->nBits, sizeof(char), 1, fi); 
        current->right = current->left = NULL;
        j = 1 << (current->nBits-1);
        newNode = tree;
        while(j > 1){
            if(current->bits & j){
                if(newNode->right) {
                    newNode = newNode->right;
                } else {
                    newNode->right = (Node *)malloc(sizeof(Node));
                    newNode = newNode->right;
                    newNode->symbol = 0;
                    newNode->right = newNode->left = NULL;
                }
            }else{
                if(newNode->left){
                    newNode = newNode->left; 
                } else {
                    newNode->left = (Node *)malloc(sizeof(Node)); 
                    newNode = newNode->left;
                    newNode->symbol = 0;
                    newNode->right = newNode->left = NULL;
                }
            }
            j >>= 1;
        }
        if(current->bits & 1)
            newNode->right = current;
        else
            newNode->left = current;
    }
}

void removeDirectoryContents(const char *path) {
    DIR *d = opendir(path);
    struct dirent *dir;
    char filePath[1024];

    if (!d) {
        perror("Error opening the directory");
        return;
    }

    while ((dir = readdir(d)) != NULL) {
        // Ignorar "." y ".."
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
            continue;
        }

        // Crear la ruta completa para el archivo o subdirectorio
        snprintf(filePath, sizeof(filePath), "%s/%s", path, dir->d_name);

        struct stat st;
        if (stat(filePath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Si es un directorio, eliminar su contenido recursivamente
                removeDirectoryContents(filePath);
                rmdir(filePath);  // Eliminar el directorio vacío
            } else {
                // Si es un archivo, eliminarlo
                remove(filePath);
            }
        }
    }
    closedir(d);
}


int main(int argc, char* argv[]){
    Node *tree;
    long int characters;
    int elements;

    char *fileName;
    char *directory;


    if(argc > 3){
        printf("Expecting less arguments\n");
        printf("Correct Usage: ./decompress <Compressed File Name> <Directory Name>\n");
        return 1;
    }

    if(argc == 2){
        printf("Directory argument not given using the default name: 'CompressedFile'\n");
        directory = "CompressedFile";
    }else{
        directory = argv[2];
    }
    if(argc <= 1){
        printf("Not enought arguments passed\n");
        printf("Correct Usage: ./decompress <Compressed File Name> <Directory Name>\n");
        return 1;
    }
    fileName = argv[1];
    struct stat st = {0};
    size_t len;
    // Asignar el nombre del directorio desde los argumentos
    
    while (1) {
        // Verificar si el directorio ya existe
        if (stat(directory, &st) == 0 && S_ISDIR(st.st_mode)) {
            char respuesta[10];
            printf("The directory '%s' already exists. ¿Do you want to replace it? (s/n): ", directory);
            fgets(respuesta, sizeof(respuesta), stdin);

            // Verificar la respuesta del usuario
            if (respuesta[0] == 's' || respuesta[0] == 'S') {
                // Eliminar el directorio existente
                if (rmdir(directory) == 0) {
                    printf("Directory '%s' deleted.\n", directory);
                } else {
                    perror("Error deleting the directory");
                    free(directory);
                    return 1;
                }
                break;  // Salir del bucle si se decide reemplazar
            } else {
                // Pedir al usuario un nuevo nombre de directorio
                printf("Enter a new name for the directory: ");
                fgets(respuesta, sizeof(respuesta), stdin);
                respuesta[strcspn(respuesta, "\n")] = 0;  // Eliminar el salto de línea al final

                // Realocar memoria para el nuevo nombre del directorio
                len = strlen(respuesta);
                directory = (char *)realloc(directory, len + 1);
                if (directory == NULL) {
                    perror("Error assigning memory");
                    return 1;
                }
                strcpy(directory, respuesta);
            }
        } else {
            break;  // Salir del bucle si el directorio no existe
        }
    }

    // Crear el nuevo directorio
    if (mkdir(directory, 0755) == 0) {
        printf("Directory succesfully created: %s\n", directory);
    } else {
        perror("Error creating the directory");
    }
    // Variables para medir tiempo
    struct timeval start, end;
    double elapsedTime;

    // Obtener el tiempo de inicio
    gettimeofday(&start, NULL);

    tree = (Node *)malloc(sizeof(Node));
    tree->symbol = 0;
    tree->right = tree->left = NULL;

    FILE *fi = fopen(fileName, "rb");
    fread(&size, sizeof(int), 1, fi);
    fread(&characters, sizeof(long int), 1, fi);
    fread(&elements, sizeof(int), 1, fi);
    
    

    Node *current = NULL;
    Node *newNode = NULL;

    
    createTree(current, newNode, tree, fi, elements);
    decompress(current, newNode, tree, fi, characters, directory);
    fclose(fi);
    deleteTree(tree);


     // Obtener el tiempo de finalización
    gettimeofday(&end, NULL);

    // Calcular el tiempo transcurrido en segundos
    elapsedTime = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

    printf("Tiempo total de descompresión serial: %f seconds\n", elapsedTime);
}

