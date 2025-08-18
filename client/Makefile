CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g

# Detect OS and set appropriate flags
ifeq ($(OS),Windows_NT)
    LDFLAGS = -lws2_32
    TARGET = client.exe
    CLEAN_CMD = del $(TARGET) 2>nul || true
else
    LDFLAGS = -pthread
    TARGET = client
    CLEAN_CMD = rm -f $(TARGET)
endif

SRCDIR = src
HEADERDIR = $(SRCDIR)/headers

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) $(SRCDIR)/*.c -o $@ -I$(HEADERDIR) $(LDFLAGS)

clean:
	$(CLEAN_CMD)

.PHONY: all clean