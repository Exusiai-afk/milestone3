all: main.c sbuffer.h sbuffer.c
	gcc -g -Wall main.c sbuffer.c -o main.out

run: all
	./main.out

zip:
	zip milestone3.zip *.c *.h Makefile
