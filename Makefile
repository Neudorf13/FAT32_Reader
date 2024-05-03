CC = clang
CFLAGS = -Wall -Wpedantic -Wextra -Werror -g
TARGET = reader
SRCS = FAT_Reader_Main.c
OBJS = $(SRCS:.c=.o)
OUTPUT_FILES = Output/*

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET) $(OBJS) $(OUTPUT_FILES)