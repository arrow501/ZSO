CC = gcc
CFLAGS = -Wall -Wextra -g -I./include
LDFLAGS = -pthread -lm

# Configuration options (defaults)
ENABLE_PRINTING = 1
ENABLE_ASSERTS = 1

# Add configuration to CFLAGS
CFLAGS += -DENABLE_PRINTING=$(ENABLE_PRINTING) -DENABLE_ASSERTS=$(ENABLE_ASSERTS)

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
EXEC = $(BIN_DIR)/pthread_app

.PHONY: all clean dirs release debug

all: dirs $(EXEC)

# Build configurations
debug: ENABLE_PRINTING = 1
debug: ENABLE_ASSERTS = 1
debug: clean all

release: ENABLE_PRINTING = 0
release: ENABLE_ASSERTS = 0
release: clean all

dirs:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(EXEC): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
