#!/bin/bash

echo "=== Master-Slave IPC Test ==="

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    killall master slave stats_reader 2>/dev/null || true
    sleep 1
}

# Set trap for cleanup
trap cleanup EXIT

# Initial cleanup
cleanup

# Start master
echo "Starting master..."
./master &
MASTER_PID=$!
sleep 1

# Verify master is running
if ! kill -0 $MASTER_PID 2>/dev/null; then
    echo "ERROR: Master failed to start"
    exit 1
fi

# Start slaves
echo "Starting slaves..."
for i in 0 1 2; do
    ./slave $i &
    echo "Started slave $i"
    sleep 0.5
done

# Start stats reader
echo "Starting stats reader..."
./stats_reader &
READER_PID=$!

# Let them communicate
echo "Letting processes communicate..."
sleep 3

# Trigger stats update
echo "Sending SIGUSR1 to master..."
kill -USR1 $MASTER_PID
sleep 1

# Test slave signal handling
echo "Testing slave signal handling (killing slave 1)..."
killall -TERM slave
sleep 2

# Another stats update
echo "Final stats update..."
kill -USR1 $MASTER_PID
sleep 1

# Graceful shutdown
echo "Shutting down..."
kill -TERM $READER_PID 2>/dev/null || true
kill -TERM $MASTER_PID 2>/dev/null || true

wait

echo "=== Test Complete ==="
