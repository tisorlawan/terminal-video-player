CC := gcc
CFLAGS := -Wall -Wextra -O2
INCLUDES :=
LIBS := -lncurses
SRC := main.c
TARGET := main

.PHONY := all clean

all: $(TARGET)

$(TARGET): $(SRC) $(RAYLIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) $(LIBS) -o $@

clean:
	rm -f $(TARGET)
