CC=gcc

main:
	$(CC) -o jurassic_queue.out src/main.c -lm -pthread -g
