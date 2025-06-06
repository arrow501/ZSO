#!/bin/bash

echo "=== Simplified Signal Test ==="

cleanup() {
    killall main master slave 2>/dev/null || true
    rm -f /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
    rm -f /tmp/slave_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f_* 2>/dev/null || true
    rm -f /dev/shm/master_stats_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
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
make debug > /dev/null 2>&1

if [[ ! -x "./main" ]]; then
    echo "❌ Build failed"
    exit 1
fi

echo "Starting system with 2 slaves..."
# Use LONG message count so system stays alive during testing
QUERY_DELAY_CYCLES=5000000 NUM_MESSAGES_PER_SLAVE=1000 ./main 2 > output.log 2>&1 &
MAIN_PID=$!

# Wait longer for system to start
echo "Waiting for system startup..."
sleep 8

# Check if processes are actually running
if ! kill -0 $MAIN_PID 2>/dev/null; then
    echo "❌ Main process died early"
    echo "Last lines of output:"
    tail -10 output.log
    echo ""
    echo "Checking for FIFO existence:"
    ls -la /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || echo "Master FIFO not found"
    exit 1
fi

# Double-check that the system is actually running
echo "Checking system status..."
if [[ ! -p "/tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f" ]]; then
    echo "❌ Master FIFO not found - system may not have started properly"
    echo "Last lines of output:"
    tail -10 output.log
    exit 1
fi

echo "Main PID: $MAIN_PID"

# Test 1: Send 5 signals with delays (should definitely work)
echo "Test 1: Sending 5 signals with 1 second delays..."
for i in {1..5}; do
    echo "  Signal $i"
    kill -USR2 $MAIN_PID 2>/dev/null  # Send SIGUSR2 to main
    sleep 1
done

sleep 2
count1=$(count_stats_headers)
echo "Stats displays after test 1: $count1"

# Test 2: Send 5 rapid signals (the critical test)
echo ""
echo "Test 2: Sending 5 rapid signals..."
for i in {1..5}; do
    kill -USR2 $MAIN_PID 2>/dev/null  # Send SIGUSR2 to main
done

sleep 3
count2=$(count_stats_headers)
total_expected=$((5 + 5))
echo "Total stats displays: $count2 (expected: $total_expected)"

# Test 3: Ultra rapid fire
echo ""
echo "Test 3: Sending 10 ultra-rapid signals..."
for i in {1..10}; do
    kill -USR2 $MAIN_PID 2>/dev/null  # Send SIGUSR2 to main
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
    echo "❌ FAILURE: Signal loss detected (got $count3, expected $final_expected)"
    result=1
fi

echo ""
echo "Last 20 lines of output:"
tail -20 output.log

exit $result