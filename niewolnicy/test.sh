#!/bin/bash

echo "=== Working Signal Test ==="

cleanup() {
    killall main master slave 2>/dev/null || true
    rm -f /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
    rm -f /tmp/slave_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f_* 2>/dev/null || true
    rm -f /dev/shm/master_stats_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
    rm -f output.log 2>/dev/null || true
}

count_stats_headers() {
    if [[ -f "output.log" ]]; then
        grep -c "=== Master Statistics ===" output.log 2>/dev/null || echo "0"
    else
        echo "0"
    fi
}

trap cleanup EXIT
cleanup

echo "Building system..."
make clean > /dev/null 2>&1
make > /dev/null 2>&1

echo "Starting system with VERY long runtime..."
# Use HUGE message count so system runs for a long time
QUERY_DELAY_CYCLES=10000000 NUM_MESSAGES_PER_SLAVE=10000 ./main 2 > output.log 2>&1 &
MAIN_PID=$!

echo "Waiting for startup..."
sleep 3

# Check startup quickly
if ! kill -0 $MAIN_PID 2>/dev/null; then
    echo "❌ Failed to start"
    cat output.log
    exit 1
fi

# Wait a bit more for full startup
sleep 2

echo "System should be running. Testing signals immediately..."

# Test 1: Send signals while system is definitely running
echo "Sending 3 test signals..."
for i in {1..3}; do
    echo "  Signal $i"
    if kill -0 $MAIN_PID 2>/dev/null; then
        kill -USR2 $MAIN_PID 2>/dev/null
        sleep 1
    else
        echo "    Process died before signal $i"
        break
    fi
done

sleep 2
count1=$(count_stats_headers)
echo "Stats displays: $count1"

# Test 2: Rapid fire
echo "Sending 3 rapid signals..."
for i in {1..3}; do
    if kill -0 $MAIN_PID 2>/dev/null; then
        kill -USR2 $MAIN_PID 2>/dev/null
    fi
done

sleep 3
count2=$(count_stats_headers)
total_expected=6
echo "Total stats displays: $count2 (expected: $total_expected)"

# Shutdown
echo "Shutting down..."
if kill -0 $MAIN_PID 2>/dev/null; then
    kill -TERM $MAIN_PID 2>/dev/null
    sleep 3
fi

# Results
echo ""
echo "=== RESULTS ==="
echo "Expected: $total_expected"
echo "Actual: $count2"

if [[ $count2 -eq $total_expected ]]; then
    echo "🎉 SUCCESS: Perfect 1:1 signal-to-display ratio!"
    result=0
else
    echo "❌ FAILURE: Signal loss detected (got $count2, expected $total_expected)"
    result=1
fi

echo ""
echo "Sample output:"
head -20 output.log
echo "..."
tail -10 output.log

exit $result
EOF

chmod +x working_test.sh
./working_test.sh