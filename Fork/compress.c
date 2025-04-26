#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "Nodos.h"
#include "Tabla.h"

void CountCharacter(Node **list, unsigned char character);


/**
 * Variables globales
 */
extern Table *table;
long int fileLength = 0;
int size = 0;


/**
 * processFile: Función que procesa un archivo y cuenta la frecuencia de cada carácter que hay en él.
 * @param filePath: char* de la ruta del archivo a procesar.
 * @param list: Node** lista donde se guardaran los caracteres y las frecuencias que hay en ellas.
 */
void processFile(const char *filePath, Node **list){
    //abre el archivo
    FILE *file = fopen(filePath, "r");
    if (!file){
        printf("Error opening the file %s\n", filePath);
        return;
    }
    // character es de 1 byte, en realidad debe ser acorde a lo que indica UTF-8 que es de 1 a 4 bytes
    // por simplicidad se deja como unsigned char
    unsigned char character;
    do{
        character = fgetc(file);
        if (feof(file))
            break;
        fileLength++; // Incrementa la longitud por cada carácter leído
        CountCharacter(list, character);
    } while (1);
    fclose(file);
}

/**
 * processDirectory: Función que procesa un directorio y llama a processFile para cada archivo que encuentra 
 * en el directorio.
 * @param directoryPath: char* de la ruta del directorio a procesar.
 * @param list: Node** de la lista donde se guardaran los caracteres y las frecuencias que hay en ellas.
 */
void processDirectory(const char *directoryPath, Node** list) {
    struct dirent *entry;
    DIR *dp = opendir(directoryPath);

    if (dp == NULL) {
        perror("The directory can not be opened");
        return;
    }

    

    while ((entry = readdir(dp))) {
        // Ignorar las entradas "." y ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char filePath[1024];
        snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);

        // Llama a processFile con la ruta completa
        processFile(filePath, list);
        // incrementa el contador de archivos procesados en el directorio
        size++;
    }

    closedir(dp);
}
/**
 * CountCharacter: Función que busca un caracter en la lista y lo inserta o suma su contador.
 * @param list: Node** lista donde se guardaran los caracteres y las frecuencias que hay en ellas.
 * @param character: unsigned char caracter a buscar en la lista.
 */
void CountCharacter(Node **list, unsigned char character) {
  Node *current, *previous, *newNode;
  if (!*list){ // si no está en la lista, crea un nuevo nodo
    *list = (Node *)malloc(sizeof(Node)); 
    (*list)->symbol = character;                  // Asigna the character
    (*list)->count = 1; //Lo inicializa con 1
    (*list)->next = (*list)->left = (*list)->right = NULL; // y pone en null los punteros (buena practica)
  } else {
    // Caso en el cual la lista esta creada, entonces busca el simbolo y lo inserta o suma su contador.
    current = *list;
    previous = NULL;
    while (current && current->symbol < character) {
      previous = current;      // Keep reference to the previous node
      current = current->next; // Move to the next node
    }

    // Check if the character already exists in the list
    if (current && current->symbol == character) {
      current->count++; // If it exists, increment its count
    } else {
      newNode = (Node *)malloc(sizeof(Node));
      newNode->symbol = character;
      newNode->left = newNode->right = NULL;
      newNode-> count = 1;
      newNode->next = current;
      if(previous) previous->next = newNode;
      else *list = newNode;
    }
  }
}

void compressFile(const char *filePath, const char *fileName, Node **list, int fileIndex) {
    FILE *fe = fopen(filePath, "r");
    if (!fe) {
        printf("Error opening the file %s\n", filePath);
        return;
    }

    // Crear un archivo temporal único para cada proceso hijo
    char tempFileName[256];
    snprintf(tempFileName, sizeof(tempFileName), "temp_compressed_%d.bin", fileIndex);
    FILE *tempFile = fopen(tempFileName, "wb");
    if (!tempFile) {
        printf("Error creating temporary file for %s\n", fileName);
        fclose(fe);
        return;
    }

    unsigned char byte = 0;
    int nBits = 0;
    int c;
    long totalCharacters = 0;  // Total de caracteres del archivo original
    long compressedBytes = 0;  // Contador de bytes comprimidos

    // Usar un buffer para almacenar el contenido comprimido
    unsigned char *compressedData = (unsigned char *)malloc(1024 * sizeof(unsigned char));
    size_t bufferSize = 1024;
    size_t bufferIndex = 0;

    // 1. Longitud del nombre del archivo
    int nameLength = strlen(fileName);
    fwrite(&nameLength, sizeof(int), 1, tempFile);

    // 2. Escribir el nombre del archivo
    fwrite(fileName, sizeof(char), nameLength, tempFile);

    // 3. Escribir el total de caracteres, pero primero contar los caracteres
    while ((c = fgetc(fe)) != EOF) {
        totalCharacters++;
    }

    // Escribir el total de caracteres en el archivo temporal
    fwrite(&totalCharacters, sizeof(long), 1, tempFile);

    // Reiniciar el puntero del archivo para comprimir
    rewind(fe);

    // 4. Comprimir el archivo y almacenar los datos en el buffer
    while ((c = fgetc(fe)) != EOF) {
        Table *node = findSymbol(table, (unsigned char)c);
        if (!node) {
            fprintf(stderr, "Symbol not found in the table: %c\n", c);
            continue;
        }

        for (int i = node->nBits - 1; i >= 0; i--) {
            unsigned char bit = (node->bits >> i) & 1;
            byte = (byte << 1) | bit;
            nBits++;

            if (nBits == 8) {
                if (bufferIndex >= bufferSize) {
                    bufferSize *= 2;
                    compressedData = (unsigned char *)realloc(compressedData, bufferSize);
                }
                compressedData[bufferIndex++] = byte;
                compressedBytes++;  // Incrementar contador de bytes comprimidos
                byte = 0;
                nBits = 0;
            }
        }
    }

    // Escribir el último byte si no está lleno
    if (nBits > 0) {
        byte <<= (8 - nBits);
        if (bufferIndex >= bufferSize) {
            bufferSize *= 2;
            compressedData = (unsigned char *)realloc(compressedData, bufferSize);
        }
        compressedData[bufferIndex++] = byte;
        compressedBytes++;  // Incrementar el contador de bytes comprimidos
    }

    // 5. Escribir la cantidad de bytes comprimidos
    fwrite(&compressedBytes, sizeof(long), 1, tempFile);

    // 6. Escribir el contenido comprimido en el archivo temporal
    fwrite(compressedData, sizeof(unsigned char), compressedBytes, tempFile);

    // Liberar el buffer temporal
    free(compressedData);

    fclose(fe);
    fclose(tempFile);  // Cerrar el archivo temporal
}



void compress(const char *directoryPath) {
    Node *list = NULL;
    struct dirent *entry;
    DIR *dp = opendir(directoryPath);
    if (dp == NULL) {
        perror("The directory cannot be opened");
        return;
    }

    int fileIndex = 0;
    pid_t *childPids = NULL;
    int numChilds = 0;

    while ((entry = readdir(dp))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char filePath[1024];
        snprintf(filePath, sizeof(filePath), "%s/%s", directoryPath, entry->d_name);
        
        // crea el fork 
        pid_t pid = fork();
        if (pid == 0) {
            // Proceso hijo: comprime y escribe en un archivo temporal
            compressFile(filePath, entry->d_name, &list, fileIndex);
            exit(0);
        } else if (pid > 0) {
            // // Proceso padre: esperar a que el proceso hijo termine
            // wait(NULL);
            childPids = (pid_t *)realloc(childPids, (numChilds + 1) * sizeof(pid_t));
            if (childPids == NULL) {
                perror("Error allocating memory for child PIDs");
                closedir(dp);
                exit(EXIT_FAILURE);
            }
            // Almacenar el PID del proceso hijo
            childPids[numChilds++] = pid;
            fileIndex++;
        } else {
            perror("Error al crear el proceso hijo");
            closedir(dp);
            free(childPids);  
            return;
        }
        //fileIndex++;
    }

    closedir(dp);

    // Esperar a que todos los procesos hijos terminen
    for (int i = 0; i < numChilds; i++) {
        waitpid(childPids[i], NULL, 0);
    }
    free(childPids);  // Liberar la memoria de los PIDs de los hijos
}
/**
 * appendTempFiles: Función que concatena los archivos temporales en el archivo comprimido.
 * @param numFiles: int número de archivos temporales a concatenar.
 * @param outputFile: FILE* archivo de salida donde se concatenaran los archivos temporales.
 * @return: void
 */
void appendTempFiles(int numFiles, FILE *outputFile) {
    for (int i = 0; i < numFiles; i++) {
        // Abrir el archivo temporal
        char tempFileName[256];
        snprintf(tempFileName, sizeof(tempFileName), "temp_compressed_%d.bin", i);
        FILE *tempFile = fopen(tempFileName, "rb");
        if (!tempFile) {
            printf("Error opening temporary file %s\n", tempFileName);
            continue;
        }

        // Leer el contenido del archivo temporal y escribirlo en el archivo de salida
        // guardamos el archivo en bloques de 1024 bytes
        unsigned char buffer[1024];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, sizeof(unsigned char), sizeof(buffer), tempFile)) > 0) {
            fwrite(buffer, sizeof(unsigned char), bytesRead, outputFile);
        }

        fclose(tempFile);

        // Eliminar el archivo temporal después de usarlo
        remove(tempFileName);
    }
}


int main(int argc, char *argv[]){
    Node *List,*Tree = NULL;
    char *fileName;
    char *directory;
    /* Validando todos los parametros de entrada*/
    if(argc > 3){
        printf("Hay muchos parámetros\n");
        printf("Uso: %s <Directorio a comprimir> <Nombre del archivo comprimido>\n", argv[0]);
        return 1;
    }

    if(argc == 2){
        printf("Nombre del archivo comprimido no proporcionado, se usara : 'CompressedFile.bin'\n");
        fileName = "CompressedFile.bin";
    }else{
        fileName = argv[2];
    }
    if(argc <= 1){
        printf("No hay suficientes parametros proporcionados:\n");
        printf("Uso: %s <Directorio a comprimir> <Nombre del archivo comprimido>\n", argv[0]);
        return 1;
    }
    directory = argv[1];

    // Variables para medir tiempo
    struct timeval start, end;
    double elapsedTime;

    // Obtener el tiempo de inicio
    gettimeofday(&start, NULL);

    // Procesamos el directorio a comprimir
    processDirectory(directory, &List);
    // Ordenamos la lista de nodos
    sortList(&List);

    // Crear el árbol de Huffman
    Tree = List;
    while(Tree && Tree->next){
        Node *newNode = (Node*)malloc(sizeof(Node));
        newNode->symbol = ':';
        newNode->right = Tree;
        Tree = Tree->next;
        newNode->left = Tree;
        Tree = Tree->next;
        newNode->count = newNode->left->count + newNode->right->count;
        insertInOrder(&Tree, newNode);
    }
    // Crear la tabla de símbolos que está en la variable externa table
    createTable(Tree, 0, 0);

    // Abrir el archivo comprimido
    FILE *compressFile = fopen(fileName, "wb");
    if (!compressFile) {
        perror("Error creating compressed file");
        return 1;
    }

    // Realizar la compresión (incluyendo los forks y procesos hijos)
    compress(directory);
    /**
     * Metadatos del archivo comprimido
     * 1. Cantidad de archivos comprimidos
     * 2. Tamano total de los archivos comprimidos (cantidad de bytes)
     * 3. Cantidad de simbolos
     * 4. Contenido de la tabla de simbolos
     *  4.1 Simbolo
     *  4.2 Cantidad de bits
     *  4.3 Cantidad de bits que se han recorrido (codigo)
     */

    // Escribir la longitud del nombre del archivo comprimido
    fwrite(&size, sizeof(int), 1, compressFile);

    // Escribir información de compresión en el archivo
    fwrite(&fileLength, sizeof(long int), 1, compressFile);
    int countElements = 0;
    Table *t = table;  
    while (t) {
        countElements++;
        t = t->next;
    }

    fwrite(&countElements, sizeof(int), 1, compressFile);
    t = table;
    while (t) {
        fwrite(&t->symbol, sizeof(char), 1, compressFile);
        fwrite(&t->bits, sizeof(unsigned long int), 1, compressFile);
        fwrite(&t->nBits, sizeof(char), 1, compressFile);
        t = t->next;
    }

    appendTempFiles(size, compressFile);

    // Cerrar el archivo comprimido
    fclose(compressFile);

    // Liberar memoria
    freeNode(Tree);
    destroyTable(table);

    // Obtener el tiempo de finalización
    gettimeofday(&end, NULL);

    // Calcular el tiempo transcurrido en segundos
    elapsedTime = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

    // Mostrar el tiempo transcurrido
    printf("Tiempo total de compresión fork: %f seconds\n", elapsedTime);

    return 0;
}
