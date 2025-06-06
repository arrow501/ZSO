CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99 -D_GNU_SOURCE
LDFLAGS = -pthread -lrt
INCLUDES = -Iinclude
SRCDIR = src

# Release mode by default, debug as option
DEBUG ?= 0
ifeq ($(DEBUG),1)
    CFLAGS += -DENABLE_PRINTING=1 -DENABLE_ASSERTS=1 -O0 -g3
else
    CFLAGS += -DENABLE_PRINTING=0 -DENABLE_ASSERTS=0 -O2 -DNDEBUG
endif

# Override parameters if provided
ifdef MAX_SLAVES
    CFLAGS += -DMAX_SLAVES=$(MAX_SLAVES)
endif
ifdef NUM_MESSAGES_PER_SLAVE
    CFLAGS += -DNUM_MESSAGES_PER_SLAVE=$(NUM_MESSAGES_PER_SLAVE)
endif

# Targets
all: master slave main

master: $(SRCDIR)/master.c include/common.h include/parameters.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)

slave: $(SRCDIR)/slave.c include/common.h include/parameters.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)

main: $(SRCDIR)/main.c include/common.h include/parameters.h
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)

# Debug mode
debug: DEBUG=1
debug: clean all

# Testing (uses debug build)
test: debug
	./test.sh

# Cleanup
clean:
	rm -f master slave main
	rm -f /tmp/master_fifo_* /tmp/slave_fifo_* /tmp/master_pid_*
	rm -f /dev/shm/master_stats_* /dev/shm/sem.stats_ready_*

.PHONY: all clean debug test