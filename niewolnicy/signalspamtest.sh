#!/bin/bash

echo "=== Simple 5x Signal Test ==="

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
echo "Starting system with 2 slaves..."
NUM_MESSAGES_PER_SLAVE=10 ./main 2 &
MAIN_PID=$!
sleep 2

# Get master PID
MASTER_PID=$(cat /tmp/master_pid_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null)
if [[ -z "$MASTER_PID" ]]; then
    echo "❌ FAIL: No master PID found"
    exit 1
fi

echo "Master PID: $MASTER_PID"
echo ""
echo "Sending 5 signals instantly..."
echo "COUNT THE === Master Statistics === HEADERS BELOW:"
echo ""

# Send 5 signals instantly
for i in {1..5}; do
    kill -USR1 $MASTER_PID 2>/dev/null
done

# Wait for all outputs
sleep 3

echo ""
echo "=== END OF OUTPUT ==="
echo "Did you see exactly 5 '=== Master Statistics ===' headers above? (y/n)"

# Cleanup
kill -TERM $MAIN_PID 2>/dev/null
sleep 1