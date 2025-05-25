#!/bin/bash

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    
    # Kill all child processes
    if [ ! -z "$SLAVE0_PID" ]; then
        kill -TERM $SLAVE0_PID 2>/dev/null
    fi
    if [ ! -z "$SLAVE1_PID" ]; then
        kill -TERM $SLAVE1_PID 2>/dev/null
    fi
    if [ ! -z "$SLAVE2_PID" ]; then
        kill -TERM $SLAVE2_PID 2>/dev/null
    fi
    if [ ! -z "$MASTER_PID" ]; then
        kill -TERM $MASTER_PID 2>/dev/null
    fi
    
    # Wait for processes to finish
    wait
    
    # Remove any leftover FIFOs
    rm -f /tmp/master_fifo /tmp/slave_fifo_*
    
    # Check for leftover resources
    echo "Checking for leftover resources..."
    if ls /tmp/master_fifo 2>/dev/null || ls /tmp/slave_fifo_* 2>/dev/null; then
        echo "WARNING: Some FIFOs were not cleaned up!"
        ls -la /tmp/master_fifo /tmp/slave_fifo_* 2>/dev/null
    else
        echo "All FIFOs cleaned up successfully"
    fi
    
    # Check for leftover shared memory
    if ls /dev/shm/master_stats 2>/dev/null; then
        echo "WARNING: Shared memory was not cleaned up!"
        rm -f /dev/shm/master_stats
    fi
    
    # Check for leftover semaphores
    if ls /dev/shm/sem.master_sem 2>/dev/null; then
        echo "WARNING: Semaphore was not cleaned up!"
        rm -f /dev/shm/sem.master_sem
    fi
}

# Set trap to cleanup on exit
trap cleanup EXIT

# Initial cleanup
echo "Initial cleanup..."
rm -f /tmp/master_fifo /tmp/slave_fifo_*
rm -f /dev/shm/master_stats /dev/shm/sem.master_sem

# Start master
echo "Starting master..."
./master &
MASTER_PID=$!

# Wait for master to initialize
sleep 1

# Check if master is running
if ! kill -0 $MASTER_PID 2>/dev/null; then
    echo "ERROR: Master failed to start"
    exit 1
fi

# Start slaves
echo "Starting slaves..."
./slave 0 &
SLAVE0_PID=$!
./slave 1 &
SLAVE1_PID=$!
./slave 2 &
SLAVE2_PID=$!

# Wait for slaves to register
sleep 2

# Check if all processes are running
for pid in $SLAVE0_PID $SLAVE1_PID $SLAVE2_PID; do
    if ! kill -0 $pid 2>/dev/null; then
        echo "ERROR: Slave $pid failed to start"
        exit 1
    fi
done

# Wait for some communication
echo "Waiting for communication..."
sleep 3

# Request stats
echo "Requesting stats..."
kill -USR1 $MASTER_PID
sleep 1

# Test stats reader
echo "Testing stats reader..."
./stats_reader &
READER_PID=$!
sleep 1

# Send signal to master to update stats
kill -USR1 $MASTER_PID
sleep 2

# Kill stats reader
kill -TERM $READER_PID
wait $READER_PID 2>/dev/null

# Kill a slave
echo "Killing slave 1..."
kill -TERM $SLAVE1_PID
wait $SLAVE1_PID 2>/dev/null

# Wait a bit
sleep 1

# Request stats again
echo "Requesting stats again..."
kill -USR1 $MASTER_PID
sleep 1

# Kill remaining slaves
echo "Killing remaining slaves..."
kill -TERM $SLAVE0_PID $SLAVE2_PID
wait $SLAVE0_PID $SLAVE2_PID 2>/dev/null

# Wait a bit for slaves to unregister
sleep 1

# Kill master
echo "Killing master..."
kill -TERM $MASTER_PID
wait $MASTER_PID 2>/dev/null

echo "Test complete"