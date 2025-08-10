CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
INCLUDES = -Isrc/headers
LIBS = -lpthread  # Aggiungi altre librerie necessarie per Linux

SERVER_SRC = $(wildcard src/*.c)
SERVER_OBJ = $(SERVER_SRC:.c=.o)
SERVER_TARGET = server

all: $(SERVER_TARGET)

$(SERVER_TARGET): $(SERVER_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(SERVER_TARGET) $(SERVER_OBJ)