# Makefile for native build (compiles for host, runs unit in QEMU via dlopen)

.PHONY: all clean

# Detect platform
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS with Homebrew
    BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
    ifdef BREW_PREFIX
        CFLAGS += -I$(BREW_PREFIX)/include
        LDFLAGS := -L$(BREW_PREFIX)/lib
    endif
endif

# Source files
SOURCES = ui_main.c pattern.c
UNIT_HOST_SRC = ../qemu-arm/unit_host.c ../qemu-arm/wav_file.c ../qemu-arm/sdk_stubs.c
INCLUDES = -I../qemu-arm
LIBS = -lncurses -ldl -lm -pthread -lsndfile

# Compiler flags
CC := gcc
CFLAGS := -std=c11 -O2 -Wall -Wextra -g $(INCLUDES)

# Output
TARGET = ui_player
OBJECTS = $(SOURCES:.c=.o)
UNIT_HOST_OBJS = ../qemu-arm/unit_host_native.o ../qemu-arm/wav_file_native.o ../qemu-arm/sdk_stubs_native.o

all: $(TARGET)

# Build unit_host objects separately to avoid conflicts
../qemu-arm/%_native.o: ../qemu-arm/%.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) $(UNIT_HOST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)
	@echo "Build complete: $(TARGET)"
	@file $(TARGET)
	@ls -lh $(TARGET)

clean:
	rm -f $(TARGET) *.o ../qemu-arm/*_native.o
