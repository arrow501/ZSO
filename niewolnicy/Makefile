CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99 -D_GNU_SOURCE
LDFLAGS = -pthread -lrt

# Enable all warnings for better code quality
CFLAGS += -Wformat=2 -Wconversion -Wshadow -Wpointer-arith
CFLAGS += -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes

# Debug/Release configuration
DEBUG ?= 1
ifeq ($(DEBUG),1)
    CFLAGS += -DDEBUG=1 -O0 -g3
else
    CFLAGS += -DDEBUG=0 -O2 -DNDEBUG
endif

# Include directory
CFLAGS += -Iinclude

# Source and object directories
SRCDIR = src
OBJDIR = obj
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Executables
TARGETS = master slave stats_reader

all: $(OBJDIR) $(TARGETS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

master: $(OBJDIR)/master.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

slave: $(OBJDIR)/slave.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

stats_reader: $(OBJDIR)/stats_reader.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c include/common.h
	$(CC) $(CFLAGS) -c $< -o $@

# Debug build (default)
debug: DEBUG=1
debug: clean all

# Release build (no debug assertions)
release: DEBUG=0
release: clean all

clean:
	rm -f $(TARGETS)
	rm -rf $(OBJDIR)
	rm -f /tmp/master_fifo /tmp/slave_fifo_*
	rm -f /dev/shm/master_stats /dev/shm/sem.stats_ready

# Quick test
test: debug
	./test.sh

# Test release build
test-release: release
	./test.sh

# Valgrind memory check
valgrind-memcheck: debug
	@echo "=== Running Valgrind Memory Check ==="
	valgrind --leak-check=full --show-leak-kinds=all \
	         --track-origins=yes --verbose \
	         ./master 2>&1 | tee valgrind-master.log &
	@sleep 2
	valgrind --leak-check=full ./slave 0 2>&1 | tee valgrind-slave.log &
	@sleep 5
	@killall master slave 2>/dev/null || true
	@echo "=== Check valgrind-*.log files for results ==="

# Helgrind thread check
valgrind-threads: debug
	@echo "=== Running Helgrind Thread Check ==="
	valgrind --tool=helgrind ./master 2>&1 | tee helgrind-master.log &
	@sleep 2
	./slave 0 &
	./slave 1 &
	@sleep 5
	@killall master slave 2>/dev/null || true
	@echo "=== Check helgrind-*.log for results ==="

# DRD thread check
valgrind-drd: debug
	@echo "=== Running DRD Thread Check ==="
	valgrind --tool=drd --check-stack-var=yes ./master 2>&1 | tee drd-master.log &
	@sleep 2
	./slave 0 &
	@sleep 5
	@killall master slave 2>/dev/null || true
	@echo "=== Check drd-*.log for results ==="

.PHONY: all clean test test-release debug release valgrind-memcheck valgrind-threads valgrind-drd
