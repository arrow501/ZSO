CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99 -D_GNU_SOURCE
LDFLAGS = -pthread -lrt

# Include path
INCLUDES = -Iinclude

# Source directory
SRCDIR = src

# Enable comprehensive warnings
CFLAGS += -Wformat=2 -Wconversion -Wshadow -Wpointer-arith
CFLAGS += -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes

# Debug/Release configuration
DEBUG ?= 1
ifeq ($(DEBUG),1)
    CFLAGS += -DENABLE_PRINTING=1 -DENABLE_ASSERTS=1 -O0 -g3
else
    CFLAGS += -DENABLE_PRINTING=0 -DENABLE_ASSERTS=0 -O2 -DNDEBUG
endif

# Parametrizable build options
ifdef NUM_SLAVES
    CFLAGS += -DNUM_SLAVES=$(NUM_SLAVES)
endif

ifdef NUM_MESSAGES_PER_SLAVE
    CFLAGS += -DNUM_MESSAGES_PER_SLAVE=$(NUM_MESSAGES_PER_SLAVE)
endif

all: master slave main

master: $(SRCDIR)/master.c include/common.h include/parameters.h
	$(CC) $(CFLAGS) $(INCLUDES) -o master $(SRCDIR)/master.c $(LDFLAGS)

slave: $(SRCDIR)/slave.c include/common.h include/parameters.h
	$(CC) $(CFLAGS) $(INCLUDES) -o slave $(SRCDIR)/slave.c $(LDFLAGS)

# Debug build (default)
debug: DEBUG=1
debug: clean all

# Release build (no debug output or assertions)
release: DEBUG=0
release: clean all

# Custom configurations
small: NUM_SLAVES=2 NUM_MESSAGES_PER_SLAVE=5
small: debug

large: NUM_SLAVES=10 NUM_MESSAGES_PER_SLAVE=20
large: debug

clean:
	rm -f master slave main
	rm -f /tmp/master_fifo_* /tmp/slave_fifo_* /tmp/master_pid_*
	rm -f /dev/shm/master_stats_* /dev/shm/sem.stats_ready_* /dev/shm/sem.stats_init_*

# Main launcher (no separate stats_reader needed)
main: $(SRCDIR)/main.c $(SRCDIR)/stats_reader.c include/common.h include/parameters.h
	$(CC) $(CFLAGS) $(INCLUDES) -o main $(SRCDIR)/main.c $(SRCDIR)/stats_reader.c $(LDFLAGS)

# Quick test with main launcher
test: debug main
	./test.sh

# Test with different configurations
test-small: small main
	NUM_SLAVES=2 ./test.sh

test-large: large main
	NUM_SLAVES=10 ./test.sh

.PHONY: all clean debug release small large test test-small test-large