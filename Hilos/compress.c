#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdint.h>

// --- Estructuras ---
typedef struct HuffmanNode {
    unsigned char simbolo;
    unsigned long frecuencia;
    struct HuffmanNode *izquierda, *derecha, *siguiente;
} HuffmanNode;

typedef struct {
    char *path;                // Ruta completa
    char *nombre;              // Nombre de archivo
    unsigned int nombre_len;
    unsigned int size;         // Longitud original (4 bytes para compatibilidad)
    unsigned long local_freq[256]; // Frecuencias locales por símbolo
    uint8_t *compressed_data;  // Buffer comprimido
    size_t compressed_size;    // Tamaño del buffer
} FileInfo;

// --- Variables globales ---
FileInfo *files = NULL;
int file_count = 0;
long int total_original = 0;      // Total de caracteres (coincide con long int en descompresor)
unsigned long global_freq[256] = {0};
uint32_t code_bits[256] = {0};
uint8_t code_len[256] = {0};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Funciones auxiliares Huffman ---
void insertarOrdenado(HuffmanNode **lista, HuffmanNode *nuevo) {
    HuffmanNode *actual = *lista, *anterior = NULL;
    while (actual && nuevo->frecuencia > actual->frecuencia) {
        anterior = actual;
        actual = actual->siguiente;
    }
    nuevo->siguiente = actual;
    if (anterior)
        anterior->siguiente = nuevo;
    else
        *lista = nuevo;
}

void liberarNodos(HuffmanNode *nodo) {
    if (!nodo) return;
    liberarNodos(nodo->izquierda);
    liberarNodos(nodo->derecha);
    free(nodo);
}

void crearTablaCodificacion(HuffmanNode *raiz, int bits_count, uint32_t bits_val) {
    if (!raiz) return;
    if (!raiz->izquierda && !raiz->derecha) {
        code_len[raiz->simbolo] = bits_count;
        code_bits[raiz->simbolo] = bits_val;
    }
    if (raiz->izquierda)
        crearTablaCodificacion(raiz->izquierda, bits_count + 1, bits_val << 1);
    if (raiz->derecha)
        crearTablaCodificacion(raiz->derecha, bits_count + 1, (bits_val << 1) | 1);
}

// --- Conteo paralelo de frecuencias ---
void *thread_count(void *arg) {
    FileInfo *fi = (FileInfo *)arg;
    FILE *f = fopen(fi->path, "rb");
    if (!f) {
        perror("Error abriendo archivo");
        return NULL;
    }
    int c;
    while ((c = fgetc(f)) != EOF) {
        fi->size++;
        fi->local_freq[(unsigned char)c]++;
    }
    fclose(f);
    return NULL;
}

// --- Compresión en memoria ---
void *thread_compress(void *arg) {
    FileInfo *fi = (FileInfo *)arg;
    FILE *f = fopen(fi->path, "rb");
    if (!f) {
        perror("Error abriendo para comprimir");
        return NULL;
    }
    size_t cap = 1024;
    uint8_t *buf = malloc(cap);
    size_t sz = 0;
    uint8_t current = 0;
    int bitcount = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        uint8_t len = code_len[(unsigned char)c];
        uint32_t bits = code_bits[(unsigned char)c];
        for (int i = len - 1; i >= 0; --i) {
            current = (current << 1) | ((bits >> i) & 1);
            if (++bitcount == 8) {
                if (sz + 1 > cap) buf = realloc(buf, cap *= 2);
                buf[sz++] = current;
                current = 0;
                bitcount = 0;
            }
        }
    }
    if (bitcount > 0) {
        current <<= (8 - bitcount);
        if (sz + 1 > cap) buf = realloc(buf, cap *= 2);
        buf[sz++] = current;
    }
    fclose(f);
    fi->compressed_data = buf;
    fi->compressed_size = sz;
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <Directorio> [ArchivoSalida]\n", argv[0]);
        return 1;
    }
    const char *dirpath = argv[1];
    const char *outname = (argc == 3 ? argv[2] : "CompressedFile.bin");
    //toma del tiempos
    struct timeval inicio, fin;
    gettimeofday(&inicio, NULL);
    // Listar archivos
    DIR *d = opendir(dirpath);
    if (!d) { perror("No se pudo abrir directorio"); return 1; }
    struct dirent *entry;
    while ((entry = readdir(d))) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
            file_count++;
    }
    closedir(d);
    if (file_count == 0) {
        fprintf(stderr, "No hay archivos que comprimir.\n");
        return 1;
    }
    files = calloc(file_count, sizeof(FileInfo));

    // Enrutador
    d = opendir(dirpath);
    int idx = 0;
    while ((entry = readdir(d))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        size_t plen = strlen(dirpath) + 1 + strlen(entry->d_name) + 1;
        files[idx].path = malloc(plen);
        snprintf(files[idx].path, plen, "%s/%s", dirpath, entry->d_name);
        files[idx].nombre = strdup(entry->d_name);
        files[idx].nombre_len = strlen(entry->d_name);
        idx++;
    }
    closedir(d);

    // Conteo paralelo
    pthread_t *threads = malloc(file_count * sizeof(pthread_t));
    for (int i = 0; i < file_count; ++i)
        pthread_create(&threads[i], NULL, thread_count, &files[i]);
    for (int i = 0; i < file_count; ++i) {
        pthread_join(threads[i], NULL);
        total_original += files[i].size;
        for (int c = 0; c < 256; ++c)
            global_freq[c] += files[i].local_freq[c];
    }

    // Construir árbol Huffman
    HuffmanNode *lista = NULL;
    for (int c = 0; c < 256; ++c) {
        if (global_freq[c]) {
            HuffmanNode *n = malloc(sizeof(HuffmanNode));
            n->simbolo = (unsigned char)c;
            n->frecuencia = global_freq[c];
            n->izquierda = n->derecha = NULL;
            n->siguiente = NULL;
            insertarOrdenado(&lista, n);
        }
    }
    while (lista && lista->siguiente) {
        HuffmanNode *n1 = lista;
        HuffmanNode *n2 = lista->siguiente;
        lista = n2->siguiente;
        HuffmanNode *parent = malloc(sizeof(HuffmanNode));
        parent->simbolo = 0;
        parent->frecuencia = n1->frecuencia + n2->frecuencia;
        parent->izquierda = n1;
        parent->derecha = n2;
        parent->siguiente = NULL;
        insertarOrdenado(&lista, parent);
    }
    crearTablaCodificacion(lista, 0, 0);

    // Compresión paralela
    for (int i = 0; i < file_count; ++i)
        pthread_create(&threads[i], NULL, thread_compress, &files[i]);
    for (int i = 0; i < file_count; ++i)
        pthread_join(threads[i], NULL);
    free(threads);

    // Escrituras de salida
    FILE *out = fopen(outname, "wb");
    if (!out) { perror("No se puede crear archivo de salida"); return 1; }
    fwrite(&file_count, sizeof(int), 1, out);
    fwrite(&total_original, sizeof(long int), 1, out);
    int symbols = 0;
    for (int c = 0; c < 256; ++c)
        if (code_len[c]) symbols++;
    fwrite(&symbols, sizeof(int), 1, out);
    for (int c = 0; c < 256; ++c) {
        if (code_len[c]) {
            fwrite(&c, sizeof(unsigned char), 1, out);
            unsigned long bits = (unsigned long)code_bits[c];
            fwrite(&bits, sizeof(unsigned long), 1, out);
            fwrite(&code_len[c], sizeof(unsigned char), 1, out);
        }
    }
    for (int i = 0; i < file_count; ++i) {
        fwrite(&files[i].nombre_len, sizeof(unsigned int), 1, out);
        fwrite(files[i].nombre, sizeof(char), files[i].nombre_len, out);
        fwrite(&files[i].size, sizeof(unsigned int), 1, out);
        fwrite(files[i].compressed_data, 1, files[i].compressed_size, out);
    }
    fclose(out);

    // Liberacion de memoria
    liberarNodos(lista);
    for (int i = 0; i < file_count; ++i) {
        free(files[i].path);
        free(files[i].nombre);
        free(files[i].compressed_data);
    }
    free(files);
    pthread_mutex_destroy(&mutex);
    gettimeofday(&fin, NULL);
    double tiempo = (fin.tv_sec - inicio.tv_sec) + (fin.tv_usec - inicio.tv_usec) / 1000000.0;
    printf("Tiempo total de compresión con hilos: %f segundos\n", tiempo);
    return 0;
}
