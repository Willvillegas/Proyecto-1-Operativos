#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/time.h>

// --- Estructura para nodos del árbol Huffman ---
typedef struct Nodo {
    unsigned char simbolo;
    int frecuencia;
    struct Nodo *izquierda, *derecha, *siguiente;
} Nodo;

// --- Estructura para la tabla de codificación ---
typedef struct EntradaTabla {
    unsigned char simbolo;
    unsigned char cantidadBits;
    unsigned long int bits;
    struct EntradaTabla *siguiente;
} EntradaTabla;

// --- Variables globales ---
EntradaTabla *tabla = NULL;
unsigned int *caracteres = NULL;
int indiceCaracter = 0;
int capacidadArray = 64;
int totalArchivos = 0;
long int longitudTotalArchivo = 0;

// --- Funciones auxiliares para árbol y lista ---

void insertarOrdenado(Nodo **lista, Nodo *nuevo) {
    Nodo *actual = *lista, *anterior = NULL;
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

void liberarNodos(Nodo *nodo) {
    if (!nodo) return;
    liberarNodos(nodo->izquierda);
    liberarNodos(nodo->derecha);
    free(nodo);
}

// --- Funciones de la tabla Huffman ---

void insertarEnTabla(unsigned char simbolo, int cantidadBits, int bits) {
    EntradaTabla *nueva = malloc(sizeof(EntradaTabla));
    nueva->simbolo = simbolo;
    nueva->cantidadBits = cantidadBits;
    nueva->bits = bits;

    EntradaTabla *actual = tabla, *anterior = NULL;
    while (actual && actual->simbolo < simbolo) {
        anterior = actual;
        actual = actual->siguiente;
    }

    nueva->siguiente = actual;
    if (anterior)
        anterior->siguiente = nueva;
    else
        tabla = nueva;
}

EntradaTabla *buscarSimbolo(unsigned char simbolo) {
    EntradaTabla *temp = tabla;
    while (temp && temp->simbolo != simbolo) {
        temp = temp->siguiente;
    }
    return temp;
}

void crearTablaCodificacion(Nodo *raiz, int cantidadBits, int bits) {
    if (raiz->derecha)
        crearTablaCodificacion(raiz->derecha, cantidadBits + 1, (bits << 1) | 1);

    if (raiz->izquierda)
        crearTablaCodificacion(raiz->izquierda, cantidadBits + 1, bits << 1);

    if (!raiz->izquierda && !raiz->derecha)
        insertarEnTabla(raiz->simbolo, cantidadBits, bits);
}

void destruirTabla() {
    EntradaTabla *temp;
    while (tabla) {
        temp = tabla;
        tabla = tabla->siguiente;
        free(temp);
    }
}

// --- Lógica para contar caracteres en archivos ---

void contarCaracter(Nodo **lista, unsigned char simbolo) {
    Nodo *actual = *lista, *anterior = NULL;

    while (actual && actual->simbolo < simbolo) {
        anterior = actual;
        actual = actual->siguiente;
    }

    if (actual && actual->simbolo == simbolo) {
        actual->frecuencia++;
    } else {
        Nodo *nuevo = malloc(sizeof(Nodo));
        nuevo->simbolo = simbolo;
        nuevo->frecuencia = 1;
        nuevo->izquierda = nuevo->derecha = NULL;
        nuevo->siguiente = actual;
        if (anterior)
            anterior->siguiente = nuevo;
        else
            *lista = nuevo;
    }
}

void procesarArchivo(const char *ruta, Nodo **lista) {
    FILE *archivo = fopen(ruta, "r");
    if (!archivo) {
        printf("No se pudo abrir el archivo: %s\n", ruta);
        return;
    }

    unsigned char caracter;
    unsigned int cantidad = 0;
    while ((caracter = fgetc(archivo)) != (unsigned char)EOF) {
        longitudTotalArchivo++;
        cantidad++;
        contarCaracter(lista, caracter);
    }

    if (totalArchivos >= capacidadArray) {
        capacidadArray *= 2;
        caracteres = realloc(caracteres, sizeof(unsigned int) * capacidadArray);
        if (!caracteres) {
            printf("Error de memoria\n");
            fclose(archivo);
            exit(1);
        }
    }
    caracteres[indiceCaracter++] = cantidad;
    totalArchivos++;
    fclose(archivo);
}

void procesarDirectorio(const char *ruta, Nodo **lista) {
    DIR *directorio = opendir(ruta);
    struct dirent *entrada;
    if (!directorio) {
        perror("No se pudo abrir el directorio");
        return;
    }

    while ((entrada = readdir(directorio))) {
        if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0)
            continue;

        char rutaCompleta[1024];
        snprintf(rutaCompleta, sizeof(rutaCompleta), "%s/%s", ruta, entrada->d_name);
        procesarArchivo(rutaCompleta, lista);
    }
    closedir(directorio);
}

// --- Compresión del archivo ---

void comprimirArchivo(const char *ruta, FILE *salida, unsigned char *byte, int *cantidadBits) {
    FILE *archivo = fopen(ruta, "r");
    if (!archivo) {
        printf("No se pudo abrir el archivo para comprimir: %s\n", ruta);
        return;
    }

    int c;
    while ((c = fgetc(archivo)) != EOF) {
        EntradaTabla *entrada = buscarSimbolo((unsigned char)c);
        if (!entrada) continue;

        for (int i = entrada->cantidadBits - 1; i >= 0; i--) {
            unsigned char bit = (entrada->bits >> i) & 1;
            *byte = (*byte << 1) | bit;
            (*cantidadBits)++;

            if (*cantidadBits == 8) {
                fwrite(byte, sizeof(unsigned char), 1, salida);
                *byte = 0;
                *cantidadBits = 0;
            }
        }
    }

    if (*cantidadBits > 0) {
        *byte <<= (8 - *cantidadBits);
        fwrite(byte, sizeof(unsigned char), 1, salida);
    }

    fclose(archivo);
}

void comprimirDirectorio(const char *ruta, FILE *salida) {
    DIR *directorio = opendir(ruta);
    struct dirent *entrada;
    if (!directorio) {
        perror("No se pudo abrir el directorio");
        return;
    }

    int i = 0;
    while ((entrada = readdir(directorio))) {
        if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0)
            continue;

        unsigned char byte = 0;
        int cantidadBits = 0;

        char rutaCompleta[1024];
        snprintf(rutaCompleta, sizeof(rutaCompleta), "%s/%s", ruta, entrada->d_name);

        int longitudNombre = strlen(entrada->d_name);
        fwrite(&longitudNombre, sizeof(int), 1, salida);
        fwrite(entrada->d_name, sizeof(char) * longitudNombre, 1, salida);
        fwrite(&caracteres[i], sizeof(unsigned int), 1, salida);
        comprimirArchivo(rutaCompleta, salida, &byte, &cantidadBits);
        i++;
    }
    closedir(directorio);
}

// --- Función principal ---

int main(int argc, char *argv[]) {
    Nodo *lista = NULL, *arbol = NULL;
    const char *nombreArchivo = "CompressedFile.bin";
    const char *directorio;

    if (argc < 2) {
        printf("Uso: %s <Ruta Directorio> [Archivo Comprimido]\n", argv[0]);
        return 1;
    }

    if (argc == 3)
        nombreArchivo = argv[2];

    directorio = argv[1];
    caracteres = malloc(sizeof(unsigned int) * capacidadArray);

    struct timeval inicio, fin;
    gettimeofday(&inicio, NULL);

    procesarDirectorio(directorio, &lista);
    arbol = lista;

    while (arbol && arbol->siguiente) {
        Nodo *nuevo = malloc(sizeof(Nodo));
        nuevo->simbolo = ';';
        nuevo->izquierda = arbol;
        arbol = arbol->siguiente;
        nuevo->derecha = arbol;
        arbol = arbol->siguiente;
        nuevo->frecuencia = nuevo->izquierda->frecuencia + nuevo->derecha->frecuencia;
        insertarOrdenado(&arbol, nuevo);
    }

    crearTablaCodificacion(arbol, 0, 0);

    FILE *archivoSalida = fopen(nombreArchivo, "wb");
    if (!archivoSalida) {
        perror("No se pudo crear el archivo de salida");
        return 1;
    }

    fwrite(&totalArchivos, sizeof(int), 1, archivoSalida);
    fwrite(&longitudTotalArchivo, sizeof(long int), 1, archivoSalida);

    int elementosTabla = 0;
    EntradaTabla *tmp = tabla;
    while (tmp) {
        elementosTabla++;
        tmp = tmp->siguiente;
    }

    fwrite(&elementosTabla, sizeof(int), 1, archivoSalida);
    tmp = tabla;
    while (tmp) {
        fwrite(&tmp->simbolo, sizeof(char), 1, archivoSalida);
        fwrite(&tmp->bits, sizeof(unsigned long int), 1, archivoSalida);
        fwrite(&tmp->cantidadBits, sizeof(char), 1, archivoSalida);
        tmp = tmp->siguiente;
    }

    comprimirDirectorio(directorio, archivoSalida);

    fclose(archivoSalida);
    liberarNodos(arbol);
    destruirTabla();
    free(caracteres);

    gettimeofday(&fin, NULL);
    double tiempo = (fin.tv_sec - inicio.tv_sec) + (fin.tv_usec - inicio.tv_usec) / 1000000.0;
    printf("Tiempo total de compresión serial: %f segundos \n", tiempo);

    return 0;
}

