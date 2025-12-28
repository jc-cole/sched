CC = gcc
CFLAGS = -Wall -Wextra -g
OBJS = main.o util.o

program: $(OBJS)
	$(CC) $(OBJS) -o program

%.o: %.c
	$(CC) $(CFLAGS) -c $<
