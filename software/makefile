.PHONY: linux win

CC=gcc
WCC=x86_64-w64-mingw32-gcc-win32

CFLAGS=-Wall -Wextra

SRC=$(wildcard src/*.c)

all: linux win

linux:
	$(CC) $(CFLAGS) -o nep $(SRC)

win:
	$(WCC) $(CFLAGS) -o nep.exe $(SRC)