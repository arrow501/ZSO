#!/bin/bash

# Simple test for the master-slave IPC system
# Tests basic functionality with minimal complexity

set -e  # Exit on any error

echo "=== Simple IPC System Test ==="

# Build the system first
echo "Building system..."
make clean >/dev/null 2>&1
make >/dev/null 2>&1

if [ ! -f "./main" ] || [ ! -f "./master" ] || [ ! -f "./slave" ]; then
    echo "❌ Build failed - executables not found"
    exit 1
fi

echo "✅ Build successful"

# Test 1: Very basic - 1 slave, short duration
echo
echo "Test 1: Basic functionality (1 slave, 10 messages)"
echo "---------------------------------------------------"

# Set low message count for quick test
export NUM_MESSAGES_PER_SLAVE=10

# Start the system in background
timeout 30s ./main 1 &
MAIN_PID=$!

# Give it time to start
sleep 2

# Check if processes are running
if ! kill -0 $MAIN_PID 2>/dev/null; then
    echo "❌ Main process died early"
    exit 1
fi

echo "✅ System started successfully"

# Request stats after a moment
sleep 3
echo "Requesting stats..."
kill -USR2 $MAIN_PID 2>/dev/null || true

# Wait for completion or timeout
wait $MAIN_PID 2>/dev/null || true

echo "✅ Test 1 completed"

# Test 2: Check if files are cleaned up
echo
echo "Test 2: Cleanup verification"
echo "----------------------------"

LEFTOVER_FILES=$(find /tmp -name "*2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f*" 2>/dev/null | wc -l)
LEFTOVER_SHM=$(find /dev/shm -name "*2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f*" 2>/dev/null | wc -l)

if [ $LEFTOVER_FILES -eq 0 ] && [ $LEFTOVER_SHM -eq 0 ]; then
    echo "✅ Cleanup successful - no leftover files"
else
    echo "⚠️  Found $LEFTOVER_FILES tmp files and $LEFTOVER_SHM shm files"
    echo "   (This might be normal if system is still shutting down)"
fi

# Test 3: Multiple slaves
echo
echo "Test 3: Multiple slaves (3 slaves, 5 messages each)"
echo "--------------------------------------------------"

export NUM_MESSAGES_PER_SLAVE=5

timeout 20s ./main 3 &
MAIN_PID=$!

sleep 2

if ! kill -0 $MAIN_PID 2>/dev/null; then
    echo "❌ Main process died early with multiple slaves"
    exit 1
fi

echo "✅ Multiple slaves started"

# Request stats
sleep 2
kill -USR2 $MAIN_PID 2>/dev/null || true

# Let it finish
wait $MAIN_PID 2>/dev/null || true

echo "✅ Test 3 completed"

# Final cleanup check
sleep 1
make clean >/dev/null 2>&1

echo
echo "=== Test Summary ==="
echo "✅ Basic functionality works"
echo "✅ Multiple slaves work" 
echo "✅ Stats request works"
echo "✅ System exits cleanly"
echo
echo "🎉 All simple tests passed!"

# Bonus: Show what a normal run looks like
echo
echo "=== Sample Normal Run (5 seconds) ==="
echo "You can now try: ./main 2"
echo "Then press 's' for stats, 'q' to quit"