CC = gcc
CFLAGS = -Wall -Wextra -g
OBJS = build/main.o

sched: $(OBJS)
	$(CC) $(OBJS) -o sched

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

