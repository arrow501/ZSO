#!/bin/bash

echo "=== Debug Test ==="

cleanup() {
    killall master slave stats_reader 2>/dev/null || true
    sleep 1
}

trap cleanup EXIT
cleanup

echo "Starting master..."
./master &
MASTER_PID=$!
sleep 2

echo "Master PID: $MASTER_PID"
if ! kill -0 $MASTER_PID 2>/dev/null; then
    echo "Master failed to start"
    exit 1
fi

echo ""
echo "Starting stats_reader in foreground to see what happens..."
echo "Press Ctrl+C to stop when you see output"
echo ""

# Start stats reader in foreground so we can see what it does
./stats_reader &
READER_PID=$!

sleep 2

echo ""
echo "Sending 1 SIGUSR1 signal to master..."
kill -USR1 $MASTER_PID

echo ""
echo "Waiting 3 seconds to see if stats appear..."
sleep 3

echo ""
echo "Sending another SIGUSR1 signal..."
kill -USR1 $MASTER_PID

sleep 2

echo ""
echo "Killing stats_reader..."
kill $READER_PID 2>/dev/null

echo ""
echo "What we should have seen:"
echo "1. Stats reader waiting message"
echo "2. After first signal: stats display"
echo "3. After second signal: another stats display"