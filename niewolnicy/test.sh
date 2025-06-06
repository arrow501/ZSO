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

count_stats_displays() {
    local log_file="$1"
    grep -c "=== Master Statistics ===" "$log_file" 2>/dev/null || echo 0
}

extract_totals() {
    local log_file="$1"
    grep "Totals:" "$log_file" | tail -n 1 | grep -o "[0-9]\+ sent, [0-9]\+ received" || echo "0 sent, 0 received"
}

trap cleanup EXIT
cleanup

# Start system and capture output
echo "Starting system with 2 slaves..."
NUM_MESSAGES_PER_SLAVE=2 timeout 10s ./main 2 > test_output.log 2>&1 &
MAIN_PID=$!
sleep 1

# Get master PID and send 3 signals  
MASTER_PID=$(cat /tmp/master_pid_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null)
if [[ -n "$MASTER_PID" ]]; then
    echo "Sending 3 stats requests..."
    for i in {1..3}; do
        kill -USR1 $MASTER_PID 2>/dev/null
    done
else
    echo "❌ FAIL: No master PID found"
    exit 1
fi

# Wait for completion
sleep 3
kill -TERM $MAIN_PID 2>/dev/null
wait $MAIN_PID 2>/dev/null || true

# Analyze results
STATS_COUNT=$(count_stats_displays "test_output.log")
FINAL_TOTALS=$(extract_totals "test_output.log")

echo ""
echo "=== Results ==="
echo "Stats displays found: $STATS_COUNT"
echo "Final totals: $FINAL_TOTALS"

# Verify results
if [[ "$STATS_COUNT" -ge 3 ]]; then
    echo "✓ PASS: Found $STATS_COUNT stats displays (expected ≥3)"
else
    echo "❌ FAIL: Only found $STATS_COUNT stats displays (expected ≥3)"
    exit 1
fi

if echo "$FINAL_TOTALS" | grep -q "[1-9][0-9]* sent, [1-9][0-9]* received"; then
    echo "✓ PASS: Messages were sent and received ($FINAL_TOTALS)"
else
    echo "❌ FAIL: No message activity detected ($FINAL_TOTALS)"
    exit 1
fi

echo "🎉 Test PASSED: System correctly counts messages!"
rm -f test_output.log