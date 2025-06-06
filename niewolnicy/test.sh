#!/bin/bash

echo "=== Comprehensive IPC System Test ==="

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    killall main master slave 2>/dev/null || true
    sleep 1
    rm -f /tmp/master_fifo_* /tmp/slave_fifo_* /tmp/master_pid_* 2>/dev/null || true
    rm -f /dev/shm/master_stats_* /dev/shm/sem.stats_ready_* 2>/dev/null || true
}

trap cleanup EXIT
cleanup

get_master_pid() {
    # Read master PID from PID file
    local pid_file="/tmp/master_pid_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f"
    if [[ -f "$pid_file" ]]; then
        cat "$pid_file" 2>/dev/null
    else
        # Fallback to pgrep
        pgrep master | head -n 1
    fi
}

run_test() {
    local test_name="$1"
    local num_slaves="$2"
    local test_duration="$3"
    
    echo ""
    echo "=== $test_name ==="
    
    # Start main process
    ./main $num_slaves &
    MAIN_PID=$!
    
    sleep 2
    
    # Verify processes are running
    if ! kill -0 $MAIN_PID 2>/dev/null; then
        echo "FAIL: Main process not running"
        return 1
    fi
    
    if ! pgrep master > /dev/null; then
        echo "FAIL: Master not running"
        return 1
    fi
    
    local slave_count=$(pgrep slave | wc -l)
    if [ "$slave_count" -ne "$num_slaves" ]; then
        echo "FAIL: Expected $num_slaves slaves, found $slave_count"
        return 1
    fi
    
    echo "✓ All processes started successfully"
    
    # Get master PID for direct signaling
    local master_pid=$(get_master_pid)
    if [[ -z "$master_pid" ]]; then
        echo "FAIL: Could not get master PID"
        return 1
    fi
    
    # Test signal handling - send directly to master
    echo "Testing signal handling..."
    for i in $(seq 1 3); do
        kill -USR1 $master_pid 2>/dev/null
        sleep 0.5
    done
    echo "✓ Signal handling tested"
    
    # Let system run for specified duration
    sleep $test_duration
    
    # Test rapid fire signals - send directly to master
    echo "Testing rapid fire signals..."
    for i in $(seq 1 5); do
        kill -USR1 $master_pid 2>/dev/null
        sleep 0.1
    done
    echo "✓ Rapid fire signals tested"
    
    # Graceful shutdown - terminate main
    echo "Testing graceful shutdown..."
    kill -TERM $MAIN_PID 2>/dev/null
    
    # Wait for processes to exit
    for i in $(seq 1 10); do
        if ! pgrep -f "main|master|slave" > /dev/null; then
            echo "✓ All processes shut down gracefully"
            return 0
        fi
        sleep 1
    done
    
    echo "WARNING: Some processes still running"
    killall -9 main master slave 2>/dev/null || true
    return 1
}

# Test 1: Single slave
run_test "Single Slave Test" 1 3
TEST1_RESULT=$?

sleep 2
cleanup

# Test 2: Multiple slaves
run_test "Multiple Slaves Test" 3 3
TEST2_RESULT=$?

sleep 2
cleanup

# Test 3: Maximum slaves
run_test "Maximum Slaves Test" 10 3
TEST3_RESULT=$?

sleep 2
cleanup

# Test 4: Stress test with signal handling
echo ""
echo "=== Stress Test with Signal Handling ==="
./main 5 &
MAIN_PID=$!
sleep 3

if kill -0 $MAIN_PID 2>/dev/null; then
    echo "✓ Stress test setup complete"
    
    # Get master PID
    MASTER_PID=$(get_master_pid)
    
    if [[ -n "$MASTER_PID" ]]; then
        # Burst of signals to master
        for burst in $(seq 1 3); do
            echo "Signal burst $burst/3..."
            for i in $(seq 1 10); do
                kill -USR1 $MASTER_PID 2>/dev/null
                sleep 0.05
            done
            sleep 1
        done
        
        # Test slave termination
        echo "Testing slave termination..."
        killall -TERM slave 2>/dev/null
        sleep 2
        
        # Final stats
        kill -USR1 $MASTER_PID 2>/dev/null
        sleep 1
    fi
    
    # Cleanup stress test
    kill -TERM $MAIN_PID 2>/dev/null
    sleep 2
    
    if ! pgrep -f "main|master|slave" > /dev/null; then
        echo "✓ Stress test completed successfully"
        TEST4_RESULT=0
    else
        echo "WARNING: Stress test processes still running"
        killall -9 main master slave 2>/dev/null || true
        TEST4_RESULT=1
    fi
else
    echo "FAIL: Stress test setup failed"
    TEST4_RESULT=1
fi

# Test 5: Invalid parameters
echo ""
echo "=== Invalid Parameters Test ==="
./main 2>/dev/null
if [ $? -ne 0 ]; then
    echo "✓ Correctly rejected missing parameters"
    TEST5_RESULT=0
else
    echo "FAIL: Should have rejected missing parameters"
    TEST5_RESULT=1
fi

./main 0 2>/dev/null
if [ $? -ne 0 ]; then
    echo "✓ Correctly rejected invalid slave count"
else
    echo "FAIL: Should have rejected invalid slave count"
    TEST5_RESULT=1
fi

# Summary
echo ""
echo "=== Test Results Summary ==="
echo "Single Slave Test:        $([ $TEST1_RESULT -eq 0 ] && echo "PASS" || echo "FAIL")"
echo "Multiple Slaves Test:     $([ $TEST2_RESULT -eq 0 ] && echo "PASS" || echo "FAIL")"
echo "Maximum Slaves Test:      $([ $TEST3_RESULT -eq 0 ] && echo "PASS" || echo "FAIL")"
echo "Stress Test:              $([ $TEST4_RESULT -eq 0 ] && echo "PASS" || echo "FAIL")"
echo "Invalid Parameters Test:  $([ $TEST5_RESULT -eq 0 ] && echo "PASS" || echo "FAIL")"

TOTAL_PASSED=$((5 - TEST1_RESULT - TEST2_RESULT - TEST3_RESULT - TEST4_RESULT - TEST5_RESULT))
echo ""
echo "Tests passed: $TOTAL_PASSED/5"

if [ $TOTAL_PASSED -eq 5 ]; then
    echo "🎉 All tests passed!"
    exit 0
else
    echo "❌ Some tests failed"
    exit 1
fi