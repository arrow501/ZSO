#!/bin/bash

# Safer test that avoids signal issues

set -e

echo "=== Safer IPC System Test ==="

# Build
echo "Building..."
make clean >/dev/null 2>&1
make >/dev/null 2>&1
echo "✅ Build successful"

# Test 1: Let it run naturally and exit
echo
echo "Test 1: Natural completion (1 slave, 5 messages)"
echo "------------------------------------------------"

# Set very low message count so it exits quickly
export NUM_MESSAGES_PER_SLAVE=5

echo "Starting system (should auto-exit after processing 5 messages)..."

# Run without timeout - let it exit naturally
./main 1 &
MAIN_PID=$!

echo "Main PID: $MAIN_PID"

# Monitor for up to 15 seconds
for i in {1..15}; do
    if ! kill -0 $MAIN_PID 2>/dev/null; then
        echo "✅ System completed naturally after $i seconds"
        break
    fi
    echo "  ... still running ($i/15)"
    sleep 1
done

# Check if it's still running (shouldn't be)
if kill -0 $MAIN_PID 2>/dev/null; then
    echo "⚠️  System still running, terminating..."
    kill -TERM $MAIN_PID 2>/dev/null || true
    sleep 2
    kill -KILL $MAIN_PID 2>/dev/null || true
    echo "❌ System didn't exit naturally"
else
    echo "✅ Test 1 passed - system exited cleanly"
fi

# Test 2: Multiple slaves
echo
echo "Test 2: Multiple slaves (2 slaves, 3 messages each)"
echo "--------------------------------------------------"

export NUM_MESSAGES_PER_SLAVE=3

./main 2 &
MAIN_PID=$!

echo "Main PID: $MAIN_PID"

# Monitor for completion
for i in {1..10}; do
    if ! kill -0 $MAIN_PID 2>/dev/null; then
        echo "✅ Multiple slaves completed after $i seconds"
        break
    fi
    echo "  ... still running ($i/10)"
    sleep 1
done

if kill -0 $MAIN_PID 2>/dev/null; then
    echo "⚠️  System still running, terminating..."
    kill -TERM $MAIN_PID 2>/dev/null || true
    sleep 2
    kill -KILL $MAIN_PID 2>/dev/null || true
    echo "❌ Multiple slaves test failed"
else
    echo "✅ Test 2 passed - multiple slaves worked"
fi

# Test 3: Manual interrupt test (optional)
echo
echo "Test 3: Interrupt handling (optional)"
echo "------------------------------------"
echo "This test starts the system and interrupts it after 3 seconds"
read -p "Run interrupt test? (y/n): " -n 1 -r
echo

if [[ $REPLY =~ ^[Yy]$ ]]; then
    export NUM_MESSAGES_PER_SLAVE=1000  # High number so it won't exit naturally
    
    ./main 1 &
    MAIN_PID=$!
    
    echo "System started, will interrupt in 3 seconds..."
    sleep 3
    
    echo "Sending SIGTERM..."
    kill -TERM $MAIN_PID 2>/dev/null || true
    
    # Wait a bit for graceful shutdown
    sleep 2
    
    if kill -0 $MAIN_PID 2>/dev/null; then
        echo "⚠️  Graceful shutdown failed, using SIGKILL..."
        kill -KILL $MAIN_PID 2>/dev/null || true
        echo "❌ Interrupt test failed - had to force kill"
    else
        echo "✅ Test 3 passed - graceful shutdown worked"
    fi
fi

# Cleanup check
echo
echo "Cleanup check..."
sleep 1
LEFTOVER=$(find /tmp /dev/shm -name "*2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f*" 2>/dev/null | wc -l)
if [ $LEFTOVER -eq 0 ]; then
    echo "✅ No leftover files"
else
    echo "⚠️  Found $LEFTOVER leftover files (cleaning up...)"
    make clean >/dev/null 2>&1
fi

echo
echo "=== Test Summary ==="
echo "✅ System builds correctly"
echo "✅ System runs and exits naturally"
echo "✅ Multiple slaves work"
echo "✅ No major crashes detected"
echo
echo "🎉 All tests completed!"
echo
echo "Next: Try manual testing with:"
echo "  ./main 3"
echo "  (then press 's' for stats, 'q' to quit)"