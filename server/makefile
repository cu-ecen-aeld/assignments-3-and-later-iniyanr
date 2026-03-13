CC ?= gcc
CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -lpthread -lrt
TARGET = aesdsocket
SRC = aesdsocket.c
all: $(TARGET)

default: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) $(SRC)-o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o
