# Use the CROSS_COMPILE variable if provided, otherwise default to gcc
CC = $(CROSS_COMPILE)gcc
CFLAGS = -g -Wall

# The default target
all: writer

writer: writer.o
	$(CC) $(CFLAGS) -o writer writer.o

writer.o: writer.c
	$(CC) $(CFLAGS) -c writer.c

clean:
	rm -f writer *.o