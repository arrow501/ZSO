#!/bin/bash

echo "=== Simplified Signal Test ==="

cleanup() {
    killall main master slave 2>/dev/null || true
    rm -f /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
    rm -f /tmp/slave_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f_* 2>/dev/null || true
    rm -f /tmp/master_pid_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
    rm -f /dev/shm/master_stats_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
}

count_stats_headers() {
    # Count the number of times "=== Master Statistics ===" appears
    grep -c "=== Master Statistics ===" output.log 2>/dev/null || echo 0
}

trap cleanup EXIT
cleanup

echo "Building system..."
make clean > /dev/null 2>&1
make debug > /dev/null 2>&1

if [[ ! -x "./main" ]]; then
    echo "❌ Build failed"
    exit 1
fi

echo "Starting system with 2 slaves..."
# Use reasonable message count for testing
NUM_MESSAGES_PER_SLAVE=20 ./main 2 > output.log 2>&1 &
MAIN_PID=$!

# Wait for system to start
sleep 3

# Get master PID
MASTER_PID=$(cat /tmp/master_pid_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null)
if [[ -z "$MASTER_PID" ]]; then
    echo "❌ FAIL: No master PID found"
    kill $MAIN_PID 2>/dev/null
    exit 1
fi

echo "Master PID: $MASTER_PID"
echo ""

# Test 1: Send 5 signals with delays (should definitely work)
echo "Test 1: Sending 5 signals with 1 second delays..."
for i in {1..5}; do
    echo "  Signal $i"
    kill -USR1 $MASTER_PID 2>/dev/null
    sleep 1
done

sleep 2
count1=$(count_stats_headers)
echo "Stats displays after test 1: $count1"

# Test 2: Send 5 rapid signals (the critical test)
echo ""
echo "Test 2: Sending 5 rapid signals..."
for i in {1..5}; do
    kill -USR1 $MASTER_PID 2>/dev/null
done

sleep 3
count2=$(count_stats_headers)
total_expected=$((5 + 5))
echo "Total stats displays: $count2 (expected: $total_expected)"

# Test 3: Ultra rapid fire
echo ""
echo "Test 3: Sending 10 ultra-rapid signals..."
for i in {1..10}; do
    kill -USR1 $MASTER_PID 2>/dev/null
done

sleep 4
count3=$(count_stats_headers)
final_expected=$((5 + 5 + 10))
echo "Final stats displays: $count3 (expected: $final_expected)"

# Shutdown
echo ""
echo "Shutting down..."
kill -TERM $MAIN_PID 2>/dev/null
sleep 2

# Show results
echo ""
echo "=== RESULTS ==="
echo "Expected total stats displays: $final_expected"
echo "Actual stats displays: $count3"

if [[ $count3 -eq $final_expected ]]; then
    echo "🎉 SUCCESS: Perfect 1:1 signal-to-display ratio!"
    result=0
else
    echo "❌ FAILURE: Signal loss detected"
    result=1
fi

echo ""
echo "Last 20 lines of output:"
tail -20 output.log

exit $result