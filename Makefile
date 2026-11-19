VERSION := $(strip $(shell cat VERSION 2>/dev/null || echo 0.1.0))

CC      := clang
CFLAGS  := -std=c99 -Wall -Wextra -O2 -MMD -MP -D_POSIX_C_SOURCE=200809L -DBANANA_BAY_VERSION=\"$(VERSION)\" $(shell sdl2-config --cflags)
LDFLAGS := $(shell sdl2-config --libs) -lm
STATIC_LDFLAGS := $(shell sdl2-config --static-libs) -lm

SRC_DIR     := src
OBJ_DIR     := build
RELEASE_DIR := releases
BIN         := banana-bay
RELEASE_BIN := $(RELEASE_DIR)/banana-bay-linux-x86-$(VERSION)

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all clean run release

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

release: $(RELEASE_BIN)

$(RELEASE_BIN): $(OBJS) | $(RELEASE_DIR)
	$(CC) $(OBJS) -o $@ $(STATIC_LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c VERSION | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(RELEASE_DIR):
	mkdir -p $(RELEASE_DIR)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(OBJ_DIR) $(BIN)
