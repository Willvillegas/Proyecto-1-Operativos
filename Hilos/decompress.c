#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdint.h>

#define DEBUG printf("DEBUG: %s:%d\n", __FILE__, __LINE__);

// Arbol  de nodos Huffman 
typedef struct tree {
    unsigned char symbol;
    unsigned long int bits;
    char nBits;
    struct tree *left;
    struct tree *right;
} Node;

// Metadata por cada archivo descompreso
typedef struct {
    char *filePath;              // direccion relativa
    unsigned int symbolCount;    // número de caracteres
    unsigned int byteLen;        // bytes del segmento comprimido
    int64_t offsetBytes;         // corrimiento
    char *directory;
    Node *tree;
    char *compressFileName;
} ThreadArg;

void deleteTree(Node *n) {
    if (!n) return;
    deleteTree(n->left);
    deleteTree(n->right);
    free(n);
}

// Construcción del árbol de Huffman a partir de los datos de los encabezado
void createTree(Node *tree, FILE *fi, int elements) {
    for (int i = 0; i < elements; i++) {
        Node *current = malloc(sizeof(Node));
        fread(&current->symbol, sizeof(char), 1, fi);
        fread(&current->bits,   sizeof(unsigned long int), 1, fi);
        fread(&current->nBits,  sizeof(char), 1, fi);
        current->left = current->right = NULL;
        unsigned long int mask = 1UL << (current->nBits - 1);
        Node *cursor = tree;
        while (mask > 1) {
            if (current->bits & mask) {
                if (!cursor->right) {
                    cursor->right = malloc(sizeof(Node));
                    cursor->right->symbol = 0;
                    cursor->right->left = cursor->right->right = NULL;
                }
                cursor = cursor->right;
            } else {
                if (!cursor->left) {
                    cursor->left = malloc(sizeof(Node));
                    cursor->left->symbol = 0;
                    cursor->left->left = cursor->left->right = NULL;
                }
                cursor = cursor->left;
            }
            mask >>= 1;
        }
        
        if (current->bits & 1)
            cursor->right = current;
        else
            cursor->left  = current;
    }
}

// Procesar segmentos por bytes y decodificar el buffer
long processSegment(FILE *fi, Node *tree, unsigned int symbolCount, int64_t offset, unsigned char *buffer, int mode) {
    fseek(fi, offset, SEEK_SET);
    unsigned long remaining = symbolCount;
    unsigned char byte = 0;
    int bitPos = 8;
    Node *cursor = tree;
    long startPos = ftell(fi);
    while (remaining > 0) {
        if (bitPos == 8) {
            if (fread(&byte, 1, 1, fi) != 1) break;
            bitPos = 0;
        }
        int bit = (byte >> (7 - bitPos)) & 1;
        bitPos++;
        cursor = bit ? cursor->right : cursor->left;
        if (!cursor->left && !cursor->right) {
            if (mode == 1) {
                *buffer++ = cursor->symbol;
            }
            remaining--;
            cursor = tree;
        }
    }
    long endPos = ftell(fi);
    return endPos - startPos;
}

// Función de hilos: decodificar y escribir archivos
void *decompressThread(void *arg) {
    ThreadArg *t = (ThreadArg *)arg;
    FILE *fi = fopen(t->compressFileName, "rb");
    if (!fi) { perror("Error abriendo el archivo comprimido"); free(t); return NULL; }

    // Asignación de memoria del buffer de salida
    unsigned char *outBuf = malloc(t->symbolCount);
    if (!outBuf) { perror("Error de asignación del buffer"); fclose(fi); free(t); return NULL; }

    // Decodificación de segmento
    processSegment(fi, t->tree, t->symbolCount, t->offsetBytes, outBuf, 1);

    // path de salida
    char fullPath[4096];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", t->directory, t->filePath);
    char *p = fullPath;
    while ((p = strchr(p + 1, '/'))) {
        *p = '\0';
        mkdir(fullPath, 0755);
        *p = '/';
    }

    // Reescribir el archivo
    FILE *fs = fopen(fullPath, "wb");
    if (!fs) {
        perror("Error creating output file");
    } else {
        fwrite(outBuf, 1, t->symbolCount, fs);
        fclose(fs);
    }

    free(outBuf);
    fclose(fi);
    free(t->filePath);
    free(t);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <Compressed File> [Output Directory]\n", argv[0]);
        return 1;
    }
    char *compName = argv[1];
    char *directory = (argc == 3) ? argv[2] : "CompressedFile";

    // Eliminacion del directorio actual
    struct stat st;
    if (stat(directory, &st) == 0 && S_ISDIR(st.st_mode)) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", directory);
        system(cmd);
    }
    mkdir(directory, 0755);

    struct timeval start, end;
    gettimeofday(&start, NULL);

    FILE *fi = fopen(compName, "rb");
    if (!fi) { perror("Opening compressed file"); return 1; }

    int treeSize, elements;
    int64_t totalChars;
    fread(&treeSize,    sizeof(treeSize),    1, fi);
    fread(&totalChars,  sizeof(totalChars),  1, fi);
    fread(&elements,    sizeof(elements),    1, fi);

    // Constrcción del arbol de Huffman
    Node *tree = malloc(sizeof(Node));
    tree->symbol = 0;
    tree->left = tree->right = NULL;
    createTree(tree, fi, elements);

    // Segmentación
    int fileCount = 0;
    ThreadArg **tasks = NULL;
    while (totalChars > 0) {
        uint32_t nameLen, symbolCount;
        if (fread(&nameLen, sizeof(nameLen), 1, fi) != 1) break;
        char *fname = malloc(nameLen + 1);
        fread(fname, 1, nameLen, fi);
        fname[nameLen] = '\0';
        fread(&symbolCount, sizeof(symbolCount), 1, fi);

        int64_t offset = ftell(fi);
        long byteLen = processSegment(fi, tree, symbolCount, offset, NULL, 0);
        totalChars -= symbolCount;

        ThreadArg *t = malloc(sizeof(ThreadArg));
        t->filePath = fname;
        t->symbolCount = symbolCount;
        t->byteLen = byteLen;
        t->offsetBytes = offset;
        t->directory = directory;
        t->tree = tree;
        t->compressFileName = compName;

        tasks = realloc(tasks, sizeof(*tasks) * (fileCount + 1));
        tasks[fileCount++] = t;
    }
    fclose(fi);

    // Creación de Hilos 
    pthread_t *threads = malloc(sizeof(*threads) * fileCount);
    for (int i = 0; i < fileCount; i++) {
        pthread_create(&threads[i], NULL, decompressThread, tasks[i]);
    }
    for (int i = 0; i < fileCount; i++) {
        pthread_join(threads[i], NULL);
    }

    deleteTree(tree);

    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
    printf("Tiempo total de decompresión hilos: %.6f seconds\n", elapsed);
    return 0;
}
