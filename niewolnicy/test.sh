#!/bin/bash

echo "=== IPC System Test ==="

cleanup() {
    killall main master slave 2>/dev/null || true
    rm -f /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
    rm -f /tmp/slave_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f_* 2>/dev/null || true
    rm -f /tmp/master_pid_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
    rm -f /dev/shm/master_stats_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
    rm -f /dev/shm/sem.stats_ready_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
}

count_stats_in_output() {
    # Count stats headers in the captured output
    grep -c "=== Master Statistics ===" test_output.txt 2>/dev/null || echo 0
}

run_basic_test() {
    local num_slaves="$1"
    local test_name="$2"
    
    echo ""
    echo "=== $test_name ==="
    
    # Start system and capture ALL output to file AND display to user
    NUM_MESSAGES_PER_SLAVE=5 ./main $num_slaves 2>&1 | tee test_output.txt &
    MAIN_PID=$!
    sleep 2
    
    # Get master PID
    MASTER_PID=$(cat /tmp/master_pid_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null)
    if [[ -z "$MASTER_PID" ]]; then
        echo "❌ FAIL: No master PID found"
        kill $MAIN_PID 2>/dev/null
        return 1
    fi
    
    echo "Master PID: $MASTER_PID, sending 3 stats requests..."
    
    # Send 3 signals
    for i in {1..3}; do
        kill -USR1 $MASTER_PID 2>/dev/null
        sleep 0.5
    done
    
    # Wait for processing
    sleep 2
    
    # Stop main process
    kill -TERM $MAIN_PID 2>/dev/null
    sleep 2
    
    # Count stats displays
    local stats_count=$(count_stats_in_output)
    echo "Stats displays found: $stats_count"
    
    if [[ "$stats_count" -ge 3 ]]; then
        echo "✓ PASS: Found $stats_count stats displays (expected ≥3)"
        return 0
    else
        echo "❌ FAIL: Only found $stats_count stats displays (expected ≥3)"
        return 1
    fi
}

trap cleanup EXIT
cleanup

# Test 1: Basic functionality
run_basic_test 2 "Basic Test (2 slaves)"
BASIC_RESULT=$?

cleanup
sleep 1

# Test 2: Multiple slaves  
run_basic_test 5 "Multiple Slaves Test (5 slaves)"
MULTI_RESULT=$?

cleanup

# Test 3: Invalid parameters
echo ""
echo "=== Invalid Parameters Test ==="
./main 2>/dev/null
if [ $? -ne 0 ]; then
    echo "✓ PASS: Correctly rejected missing parameters"
    PARAM_RESULT=0
else
    echo "❌ FAIL: Should have rejected missing parameters"
    PARAM_RESULT=1
fi

# Summary
echo ""
echo "=== Test Results ==="
echo "Basic Test:           $([ $BASIC_RESULT -eq 0 ] && echo "PASS" || echo "FAIL")"
echo "Multiple Slaves Test: $([ $MULTI_RESULT -eq 0 ] && echo "PASS" || echo "FAIL")"  
echo "Invalid Params Test:  $([ $PARAM_RESULT -eq 0 ] && echo "PASS" || echo "FAIL")"

TOTAL_PASSED=$((3 - BASIC_RESULT - MULTI_RESULT - PARAM_RESULT))
echo "Tests passed: $TOTAL_PASSED/3"

if [ $TOTAL_PASSED -eq 3 ]; then
    echo "🎉 All tests PASSED!"
    exit 0
else
    echo "❌ Some tests failed"
    exit 1
fi

# Cleanup temp file
rm -f test_output.txt