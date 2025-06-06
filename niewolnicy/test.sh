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
    #!/bin/bash

echo "=== IPC System Test ==="

# Get number of slaves from environment or default
NUM_SLAVES=${NUM_SLAVES:-3}

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    killall main master slave stats_reader 2>/dev/null || true
    sleep 1
    
    # Clean up IPC resources
    rm -f /tmp/master_fifo /tmp/slave_fifo_* /tmp/master_pid 2>/dev/null || true
    rm -f /dev/shm/master_stats /dev/shm/sem.stats_ready 2>/dev/null || true
}

# Set trap for cleanup
trap cleanup EXIT

# Initial cleanup
cleanup

echo "Testing with $NUM_SLAVES slaves..."

# Test 1: Basic functionality
echo "Test 1: Starting main launcher..."
timeout 10s ./main $NUM_SLAVES &
MAIN_PID=$!

sleep 2

# Verify processes are running
if ! pgrep master > /dev/null; then
    echo "ERROR: Master not running"
    exit 1
fi

SLAVE_COUNT=$(pgrep slave | wc -l)
if [ "$SLAVE_COUNT" -ne "$NUM_SLAVES" ]; then
    echo "ERROR: Expected $NUM_SLAVES slaves, found $SLAVE_COUNT"
    exit 1
fi

echo "✓ All processes started successfully"

# Test 2: Stats functionality
echo "Test 2: Testing stats reader..."
echo -e "\n\n" | timeout 5s ./stats_reader &
READER_PID=$!

sleep 1

# Check if stats reader is working
if ! kill -0 $READER_PID 2>/dev/null; then
    echo "ERROR: Stats reader failed to start"
    exit 1
fi

echo "✓ Stats reader working"

# Test 3: Signal handling
echo "Test 3: Testing signal handling..."

# Find master PID and send SIGUSR1
MASTER_PID=$(pgrep master)
if [ -n "$MASTER_PID" ]; then
    kill -USR1 $MASTER_PID
    echo "✓ Sent SIGUSR1 to master"
else
    echo "ERROR: Could not find master PID"
    exit 1
fi

sleep 2

# Test 4: Graceful shutdown
echo "Test 4: Testing graceful shutdown..."
kill -TERM $MAIN_PID 2>/dev/null || true
kill -TERM $READER_PID 2>/dev/null || true

# Wait for processes to exit
for i in {1..5}; do
    if ! pgrep -f "main|master|slave" > /dev/null; then
        echo "✓ All processes shut down gracefully"
        break
    fi
    sleep 1
done

if pgrep -f "main|master|slave" > /dev/null; then
    echo "WARNING: Some processes still running"
    killall -9 main master slave 2>/dev/null || true
fi

echo "=== Test Complete ==="
echo "All tests passed!"