# Compilador y banderas
CC = gcc
CFLAGS = -Wall -I.

# Archivos fuente y objetos
COMPRESS_SRCS = compress.c 
DECOMPRESS_SRCS = decompress.c

COMPRESS_OBJS = $(COMPRESS_SRCS:.c=.o)
DECOMPRESS_OBJS = $(DECOMPRESS_SRCS:.c=.o)

# Nombre de los ejecutables
COMPRESS_TARGET = compress
DECOMPRESS_TARGET = decompress

# Regla principal para compilar todo
all: $(COMPRESS_TARGET) $(DECOMPRESS_TARGET)

# Regla para crear el ejecutable de compress
$(COMPRESS_TARGET): $(COMPRESS_OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

# Regla para crear el ejecutable de decompress
$(DECOMPRESS_TARGET): $(DECOMPRESS_OBJS)
	$(CC) -o $@ $^ $(CFLAGS)

# Regla genérica para compilar archivos .c en .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpiar los archivos generados
clean:
	rm -f $(COMPRESS_OBJS) $(DECOMPRESS_OBJS) $(COMPRESS_TARGET) $(DECOMPRESS_TARGET)

# Phony targets
.PHONY: all clean
