CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
TARGET = aesdsocket

all: $(TARGET)

default: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c

clean:
	rm -f $(TARGET) *.o
