#!/bin/bash

echo "=== Debug Test ==="

cleanup() {
    killall main master slave 2>/dev/null || true
    rm -f /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
    rm -f /tmp/slave_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f_* 2>/dev/null || true
    rm -f /dev/shm/master_stats_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
}

trap cleanup EXIT
cleanup

echo "Building with debug..."
make clean > /dev/null 2>&1
make debug > /dev/null 2>&1

echo "Starting system with debug output..."
QUERY_DELAY_CYCLES=1000000 NUM_MESSAGES_PER_SLAVE=10 ./main 1 &
MAIN_PID=$!

echo "Main PID: $MAIN_PID"
sleep 2

echo ""
echo "Checking what processes are running..."
ps aux | grep -E "(main|master|slave)" | grep -v grep

echo ""
echo "Checking if master FIFO exists..."
ls -la /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || echo "FIFO not found!"

echo ""
echo "Sending SIGUSR2 to main..."
kill -USR2 $MAIN_PID 2>/dev/null && echo "Signal sent successfully" || echo "Failed to send signal"

sleep 2

echo ""
echo "Stopping..."
kill -TERM $MAIN_PID 2>/dev/null
sleep 1

echo "Done!"