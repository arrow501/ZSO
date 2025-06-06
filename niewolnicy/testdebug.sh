#!/bin/bash

echo "=== Debug Test ==="

cleanup() {
    echo "Cleaning up..."
    killall main master slave 2>/dev/null || true
    rm -f /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
    rm -f /tmp/slave_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f_* 2>/dev/null || true
    rm -f /dev/shm/master_stats_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
}

trap cleanup EXIT
cleanup

echo "Building..."
make clean > /dev/null 2>&1
make debug > /dev/null 2>&1

echo "Starting system with debug output..."
QUERY_DELAY_CYCLES=10000000 NUM_MESSAGES_PER_SLAVE=50 ./main 1 &
MAIN_PID=$!

echo "Main PID: $MAIN_PID"

# Check startup progress
for i in {1..10}; do
    echo "Check $i: Process alive? $(kill -0 $MAIN_PID 2>/dev/null && echo 'YES' || echo 'NO')"
    echo "Check $i: FIFO exists? $([ -p /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f ] && echo 'YES' || echo 'NO')"
    
    if kill -0 $MAIN_PID 2>/dev/null && [ -p /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f ]; then
        echo "✅ System is ready!"
        break
    fi
    
    sleep 1
done

if kill -0 $MAIN_PID 2>/dev/null; then
    echo ""
    echo "Testing signal..."
    kill -USR2 $MAIN_PID
    sleep 2
    echo "System should have displayed stats above"
    
    echo ""
    echo "Stopping..."
    kill -TERM $MAIN_PID
    sleep 2
else
    echo "❌ Process died"
fi

echo "Done."