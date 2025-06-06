#!/bin/bash

echo "=== Debug Signal Test ==="

cleanup() {
    killall main master slave 2>/dev/null || true
    rm -f /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
    rm -f /tmp/slave_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f_* 2>/dev/null || true
    rm -f /tmp/master_pid_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
    rm -f /dev/shm/master_stats_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
    rm -f /dev/shm/sem.stats_ready_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
}

trap cleanup EXIT
cleanup

# Start system
echo "Starting system..."
NUM_MESSAGES_PER_SLAVE=10 ./main 2 &
MAIN_PID=$!
sleep 2

MASTER_PID=$(cat /tmp/master_pid_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null)
echo "Master PID: $MASTER_PID"
echo ""

# Test 1: Signals with delay (should work)
echo "=== Test 1: Signals with 1 second delay ==="
for i in {1..3}; do
    echo "Sending signal $i..."
    kill -USR1 $MASTER_PID 2>/dev/null
    sleep 1
done
sleep 2

echo ""
echo "=== Test 2: Rapid fire signals ==="
echo "Sending 5 signals instantly..."
for i in {1..5}; do
    kill -USR1 $MASTER_PID 2>/dev/null
done
sleep 3

echo ""
echo "=== Test 3: Ultra rapid ==="
echo "Sending 10 signals with no delay..."
for i in {1..10}; do
    kill -USR1 $MASTER_PID 2>/dev/null
done
sleep 4

kill -TERM $MAIN_PID 2>/dev/null