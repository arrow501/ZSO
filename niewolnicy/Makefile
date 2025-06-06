CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99 -D_GNU_SOURCE
LDFLAGS = -pthread -lrt
INCLUDES = -Iinclude
SRCDIR = src

# Follow parameters.h defaults by default
# (no explicit flags = use what's in parameters.h)

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

# Debug mode (force debug settings)
debug: CFLAGS += -DENABLE_PRINTING=1 -DENABLE_ASSERTS=1 -O0 -g3
debug: clean all

# Release mode (force release settings)
release: CFLAGS += -DENABLE_PRINTING=0 -DENABLE_ASSERTS=0 -O2 -DNDEBUG
release: clean all

# Testing (with shorter message limit)
test: all
	NUM_MESSAGES_PER_SLAVE=10 ./test.sh

# Cleanup (only our files with full UUID)
clean:
	rm -f master slave main
	rm -f /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
	rm -f /tmp/slave_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f_* 2>/dev/null || true
	rm -f /tmp/master_pid_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
	rm -f /dev/shm/master_stats_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
	rm -f /dev/shm/sem.stats_ready_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true

.PHONY: all clean debug release test