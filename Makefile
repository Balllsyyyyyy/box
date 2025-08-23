all:
	gcc main.c -o box -m64 -march=native -mtune=native -Oz -g0 -lraylib -lm
