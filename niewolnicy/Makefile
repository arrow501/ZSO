CC = gcc
CFLAGS = -Wall -Wextra -g -I.
LDFLAGS = -pthread -lrt

# Konfiguracja
ENABLE_PRINTING = 1

CFLAGS += -DENABLE_PRINTING=$(ENABLE_PRINTING)

all: master slave

master: master.c
	$(CC) $(CFLAGS) -o master master.c $(LDFLAGS)

slave: slave.c
	$(CC) $(CFLAGS) -o slave slave.c $(LDFLAGS)

clean:
	rm -f master slave
	rm -f /tmp/master_fifo /tmp/slave_fifo_*

run: all
	./master &
	sleep 1
	./slave 0 &
	./slave 1 &
	./slave 2 &

test: all
	@echo "Uruchamianie testów..."
	./run_tests.sh

.PHONY: all clean run test
