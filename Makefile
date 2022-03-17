CFLAGS = -std=gnu11 -Wall -Werror -Wextra -O3

.PHONY: all
all: task1 task2

.PHONY: clean
clean:
	$(RM) task1 task2 membench.o

membench.o: membench.h membench.c
	$(CC) $(CFLAGS) -c membench.c -o $@

task1: membench.o task1.c
	$(CC) $(CFLAGS) $^ -o $@ -D LOCAL_THREAD -pthread

task2: membench.o task2.c
	$(CC) $(CFLAGS) $^ -o $@ -D LOCAL_THREAD -pthread
