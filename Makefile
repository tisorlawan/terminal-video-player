CC := gcc
CFLAGS := -Wall -Wextra -O2
INCLUDES :=
LIBS := -lncurses -lavformat -lavcodec -lavutil
SRC := main.c
TARGET := tvp

.PHONY := all clean example

all: $(TARGET)

$(TARGET): $(SRC) $(RAYLIB)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRC) $(LIBS) -o $@

clean:
	@rm -f $(TARGET)

example: clean all
	./$(TARGET) ./raws/test-short.mkv ./raws/output.mp4
