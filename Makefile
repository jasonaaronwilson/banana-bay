VERSION := $(strip $(shell cat VERSION 2>/dev/null || echo 1))

CC      := clang
CFLAGS  := -std=c99 -Wall -Wextra -O2 -MMD -MP -D_POSIX_C_SOURCE=200809L -DBANANA_BAY_VERSION=\"$(VERSION)\" $(shell sdl2-config --cflags)
LDFLAGS := $(shell sdl2-config --libs) -lm
STATIC_LDFLAGS := $(shell sdl2-config --static-libs) -lm

SRC_DIR       := src
OBJ_DIR       := build
ARM64_OBJ_DIR := build/arm64
RELEASE_DIR   := releases
BIN           := banana-bay
RELEASE_BIN       := $(RELEASE_DIR)/banana-bay-linux-x86-$(VERSION)
RELEASE_ARM64_BIN := $(RELEASE_DIR)/banana-bay-linux-arm64-$(VERSION)

ARM64_CFLAGS         := --target=aarch64-linux-gnu $(CFLAGS)
ARM64_STATIC_LDFLAGS := --target=aarch64-linux-gnu -fuse-ld=lld /usr/lib/aarch64-linux-gnu/libSDL2.a -L/usr/lib/aarch64-linux-gnu $(shell PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig pkg-config --libs --static sdl2 | sed 's/-lSDL2//g') -lm

SRCS       := $(wildcard $(SRC_DIR)/*.c)
OBJS       := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS       := $(OBJS:.o=.d)
ARM64_OBJS := $(patsubst $(SRC_DIR)/%.c,$(ARM64_OBJ_DIR)/%.o,$(SRCS))
ARM64_DEPS := $(ARM64_OBJS:.o=.d)

.PHONY: all clean run release release-x86 release-arm64

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

release: release-x86 release-arm64

release-x86: $(RELEASE_BIN)

release-arm64: $(RELEASE_ARM64_BIN)

$(RELEASE_BIN): $(OBJS) | $(RELEASE_DIR)
	$(CC) $(OBJS) -o $@ $(STATIC_LDFLAGS)

$(RELEASE_ARM64_BIN): $(ARM64_OBJS) | $(RELEASE_DIR)
	$(CC) $(ARM64_OBJS) -o $@ $(ARM64_STATIC_LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c VERSION | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(ARM64_OBJ_DIR)/%.o: $(SRC_DIR)/%.c VERSION | $(ARM64_OBJ_DIR)
	$(CC) $(ARM64_CFLAGS) -c $< -o $@

-include $(DEPS) $(ARM64_DEPS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(ARM64_OBJ_DIR):
	mkdir -p $(ARM64_OBJ_DIR)

$(RELEASE_DIR):
	mkdir -p $(RELEASE_DIR)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(OBJ_DIR) $(BIN)
