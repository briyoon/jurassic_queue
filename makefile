CC=gcc

main:
	$(CC) -o jurassic_queue src/main.c -lm -pthread -g
