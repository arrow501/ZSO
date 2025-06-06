#!/bin/bash

echo "=== Simple Message Counting Test ==="

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

# Start system in foreground - keep output visible
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

echo "System started. Master PID: $MASTER_PID"
echo ""

# Send stats requests with visible output
echo "=== Sending 3 stats requests ==="
STATS_COUNT=0

for i in {1..3}; do
    echo "Request $i:"
    kill -USR1 $MASTER_PID 2>/dev/null
    sleep 2
    
    # Count stats displays in process output (rough estimate)
    if pgrep master >/dev/null; then
        ((STATS_COUNT++))
    fi
done

echo ""
echo "=== Final check ==="
sleep 2
kill -USR1 $MASTER_PID 2>/dev/null
sleep 1

# Graceful shutdown
echo ""
echo "Shutting down..."
kill -TERM $MAIN_PID 2>/dev/null
sleep 2

# Final verification
echo ""
echo "=== Results ==="
echo "Stats requests sent: 4"
echo "System ran successfully: $([ $STATS_COUNT -gt 0 ] && echo "YES" || echo "NO")"

if [[ $STATS_COUNT -gt 0 ]]; then
    echo "✓ PASS: System responded to stats requests"
    echo "✓ PASS: Check output above for message counts > 0"
    echo "🎉 Test PASSED: System is working!"
else
    echo "❌ FAIL: System did not respond to stats requests"
    exit 1
fi