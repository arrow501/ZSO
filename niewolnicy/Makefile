CC = gcc
CFLAGS = -Wall -Wextra -g -I.
LDFLAGS = -pthread -lrt

# Konfiguracja
ENABLE_PRINTING = 1

CFLAGS += -DENABLE_PRINTING=$(ENABLE_PRINTING)

# Dodaj więcej warnings w trybie debug
ifeq ($(ENABLE_PRINTING),1)
    CFLAGS += -Wformat=2 -Wconversion -Wshadow
endif

all: master slave

master: master.c
	$(CC) $(CFLAGS) -o master master.c $(LDFLAGS)

slave: slave.c
	$(CC) $(CFLAGS) -o slave slave.c $(LDFLAGS)

# Kompilacja bez printów dla valgrind
silent: ENABLE_PRINTING = 0
silent: clean all

clean:
	rm -f master slave
	rm -f /tmp/master_fifo /tmp/slave_fifo_*
	rm -f /dev/shm/master_stats /dev/shm/sem.master_sem

test: all
	@echo "Uruchamianie testów..."
	@chmod +x run_tests.sh
	./run_tests.sh

# Testy z valgrindem
valgrind-test: silent
	@echo "Uruchamianie testów z valgrindem..."
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--log-file=valgrind-master.log ./master &
	@sleep 2
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--log-file=valgrind-slave0.log ./slave 0 &
	@sleep 1
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--log-file=valgrind-slave1.log ./slave 1 &
	@sleep 5
	@pkill -TERM slave
	@sleep 2
	@pkill -TERM master
	@sleep 2
	@echo "Sprawdzanie logów valgrind..."
	@grep "ERROR SUMMARY" valgrind-*.log
	@rm -f valgrind-*.log

# Test pojedynczego mastera
test-master: all
	./master

# Test pojedynczego slave'a
test-slave: all
	./slave 0

.PHONY: all clean test silent valgrind-test test-master test-slave