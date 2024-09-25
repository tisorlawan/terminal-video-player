CC := gcc
CFLAGS := -Wall -Wextra -O2
INCLUDES :=
LIBS := -lncurses -lavformat -lavcodec -lavutil
SRC := main.c
TARGET := tvp

.PHONY := all clean

all: $(TARGET)

$(TARGET): $(SRC) $(RAYLIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) $(LIBS) -o $@

clean:
	rm -f $(TARGET)
