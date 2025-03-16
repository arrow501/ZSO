CC = gcc
CFLAGS = -Wall -Wextra -g -I./include
LDFLAGS = -pthread -lm

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
EXEC = $(BIN_DIR)/pthread_app

.PHONY: all clean dirs

all: dirs $(EXEC)

dirs:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(EXEC): $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
