CC = gcc
CFLAGS = -Wall -Wextra -O2 -I.-DHAVE_LIBGPIOD
LDFLAGS = -pthread -lm
LIBS = -pthread -lm -lgpiod
# Automatically find all C files in the directory
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
TARGET = loki

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
