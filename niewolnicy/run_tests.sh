#!/bin/bash

# Cleanup
rm -f /tmp/master_fifo /tmp/slave_fifo_*

# Start master
./master &
MASTER_PID=$!
sleep 1

# Start slaves
./slave 0 &
SLAVE0_PID=$!
./slave 1 &
SLAVE1_PID=$!
./slave 2 &
SLAVE2_PID=$!

# Wait for some communication
sleep 3

# Request stats
echo "Requesting stats..."
kill -USR1 $MASTER_PID
sleep 1

# Kill a slave
echo "Killing slave 1..."
kill -TERM $SLAVE1_PID
sleep 1

# Request stats again
echo "Requesting stats again..."
kill -USR1 $MASTER_PID
sleep 1

# Kill remaining processes
kill -TERM $SLAVE0_PID $SLAVE2_PID
sleep 1
kill -TERM $MASTER_PID

# Wait for all to finish
wait

echo "Test complete"
