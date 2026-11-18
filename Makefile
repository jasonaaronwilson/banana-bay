CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -O2 -D_POSIX_C_SOURCE=200809L $(shell sdl2-config --cflags)
LDFLAGS := $(shell sdl2-config --libs) -lm

SRC_DIR := src
OBJ_DIR := build
BIN     := banana-bay

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all clean run

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(OBJ_DIR) $(BIN)
