CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99 -D_GNU_SOURCE
LDFLAGS = -pthread -lrt

# Include path
INCLUDES = -Iinclude

# Source directory
SRCDIR = src

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

all: master slave stats_reader

master: $(SRCDIR)/master.c include/common.h
	$(CC) $(CFLAGS) $(INCLUDES) -o master $(SRCDIR)/master.c $(LDFLAGS)

slave: $(SRCDIR)/slave.c include/common.h
	$(CC) $(CFLAGS) $(INCLUDES) -o slave $(SRCDIR)/slave.c $(LDFLAGS)

stats_reader: $(SRCDIR)/stats_reader.c include/common.h
	$(CC) $(CFLAGS) $(INCLUDES) -o stats_reader $(SRCDIR)/stats_reader.c $(LDFLAGS)

# Debug build (default)
debug: DEBUG=1
debug: clean all

# Release build (no debug assertions)
release: DEBUG=0
release: clean all

clean:
	rm -f master slave stats_reader
	rm -f /tmp/master_fifo /tmp/slave_fifo_*
	rm -f /dev/shm/master_stats /dev/shm/sem.stats_ready

# Quick test
test: debug
	./test.sh

# Test release build
test-release: release
	./test.sh


.PHONY: all clean test test-release debug release 
