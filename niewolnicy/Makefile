CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99 -D_GNU_SOURCE
LDFLAGS = -pthread -lrt
INCLUDES = -Iinclude
SRCDIR = src

# Debug/Release mode
DEBUG ?= 1
ifeq ($(DEBUG),1)
    CFLAGS += -DENABLE_PRINTING=1 -DENABLE_ASSERTS=1 -O0 -g3
else
    CFLAGS += -DENABLE_PRINTING=0 -DENABLE_ASSERTS=0 -O2 -DNDEBUG
endif

# Override parameters if provided
ifdef NUM_SLAVES
    CFLAGS += -DNUM_SLAVES=$(NUM_SLAVES)
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

# Convenience targets
debug: DEBUG=1
debug: clean all

release: DEBUG=0
release: clean all

small: NUM_SLAVES=2 NUM_MESSAGES_PER_SLAVE=5
small: debug

large: NUM_SLAVES=10 NUM_MESSAGES_PER_SLAVE=20
large: debug

# Testing
test: debug
	./test.sh

# Cleanup
clean:
	rm -f master slave main
	rm -f /tmp/master_fifo_* /tmp/slave_fifo_* /tmp/master_pid_*
	rm -f /dev/shm/master_stats_* /dev/shm/sem.stats_ready_* /dev/shm/sem.stats_init_*

.PHONY: all clean debug release small large test