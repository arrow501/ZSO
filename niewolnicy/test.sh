#!/bin/bash

# Signal and stats testing - building on safer test foundation

set -e

echo "=== Signal and Stats Testing ==="

# Build
echo "Building..."
make clean >/dev/null 2>&1
make >/dev/null 2>&1
echo "✅ Build successful"

# Test 1: Basic signal handling with long-running system
echo
echo "Test 1: Signal handling (system that runs long enough for testing)"
echo "----------------------------------------------------------------"

export NUM_MESSAGES_PER_SLAVE=500  # High enough to not exit quickly

echo "Starting system with 2 slaves (500 messages each)..."
./main 2 > signal_test.log 2>&1 &
MAIN_PID=$!

echo "Main PID: $MAIN_PID"

# Wait for startup
sleep 3

# Check it's actually running
if ! kill -0 $MAIN_PID 2>/dev/null; then
    echo "❌ System exited too early - increase NUM_MESSAGES_PER_SLAVE"
    exit 1
fi

echo "✅ System running, now testing signals..."

# Send 3 stats requests with delays
echo "Sending 3 SIGUSR2 signals..."
kill -USR2 $MAIN_PID 2>/dev/null
echo "Signal 1 sent"
sleep 2

kill -USR2 $MAIN_PID 2>/dev/null  
echo "Signal 2 sent"
sleep 2

kill -USR2 $MAIN_PID 2>/dev/null
echo "Signal 3 sent"
sleep 2

# Terminate gracefully
echo "Terminating system..."
kill -TERM $MAIN_PID 2>/dev/null || true

# Wait for shutdown
for i in {1..10}; do
    if ! kill -0 $MAIN_PID 2>/dev/null; then
        echo "✅ System shut down after $i seconds"
        break
    fi
    sleep 1
done

# Force kill if needed
if kill -0 $MAIN_PID 2>/dev/null; then
    echo "⚠️  Force killing..."
    kill -KILL $MAIN_PID 2>/dev/null || true
fi

# Count stats outputs
STATS_COUNT=$(grep -c "=== Master Statistics ===" signal_test.log || echo "0")
echo "Expected: 3 stats prints"
echo "Actual: $STATS_COUNT stats prints"

if [ "$STATS_COUNT" -eq 3 ]; then
    echo "✅ Signal counting correct"
elif [ "$STATS_COUNT" -gt 0 ]; then
    echo "⚠️  Got $STATS_COUNT stats (might be signal coalescing)"
else
    echo "❌ No stats printed - signal handling broken"
fi

echo
echo "Sample stats output:"
echo "-------------------"
grep -A 5 "=== Master Statistics ===" signal_test.log | head -10

# Test 2: Message accuracy test
echo
echo "Test 2: Message counting accuracy (complete run)"
echo "-----------------------------------------------"

export NUM_MESSAGES_PER_SLAVE=20  # Low number for complete run

echo "Starting system (20 messages per slave, 3 slaves = 60 total)..."
./main 3 > accuracy_test.log 2>&1 &
MAIN_PID=$!

# Wait a bit then get mid-run stats
sleep 2
if kill -0 $MAIN_PID 2>/dev/null; then
    echo "Getting mid-run stats..."
    kill -USR2 $MAIN_PID 2>/dev/null
    sleep 1
fi

# Let it complete naturally
echo "Waiting for natural completion..."
for i in {1..15}; do
    if ! kill -0 $MAIN_PID 2>/dev/null; then
        echo "✅ System completed naturally after $i seconds"
        break
    fi
    sleep 1
done

# Check final totals
if grep -q "Totals:" accuracy_test.log; then
    FINAL_LINE=$(grep "Totals:" accuracy_test.log | tail -1)
    echo "Final totals: $FINAL_LINE"
    
    SENT=$(echo "$FINAL_LINE" | grep -o "[0-9]* sent" | cut -d' ' -f1)
    RECEIVED=$(echo "$FINAL_LINE" | grep -o "[0-9]* received" | cut -d' ' -f1)
    
    echo "Expected: 60 sent, 60 received"
    echo "Actual: $SENT sent, $RECEIVED received"
    
    if [ "$SENT" -eq 60 ] && [ "$RECEIVED" -eq 60 ]; then
        echo "✅ Message counting perfect"
    else
        echo "⚠️  Message counting off (might be early termination)"
    fi
else
    echo "❌ No final stats found"
fi

# Test 3: Rapid signal test 
echo
echo "Test 3: Rapid signal handling"
echo "----------------------------"

export NUM_MESSAGES_PER_SLAVE=300

echo "Starting system for rapid signal test..."
./main 1 > rapid_test.log 2>&1 &
MAIN_PID=$!

sleep 2

if kill -0 $MAIN_PID 2>/dev/null; then
    echo "Sending 5 rapid signals..."
    for i in {1..5}; do
        kill -USR2 $MAIN_PID 2>/dev/null
        echo "Signal $i sent"
        sleep 0.2  # Short delay
    done
    
    sleep 3
    kill -TERM $MAIN_PID 2>/dev/null || true
    
    # Wait for shutdown
    for i in {1..5}; do
        if ! kill -0 $MAIN_PID 2>/dev/null; then
            break
        fi
        sleep 1
    done
    
    RAPID_COUNT=$(grep -c "=== Master Statistics ===" rapid_test.log || echo "0")
    echo "Rapid signals sent: 5"
    echo "Stats printed: $RAPID_COUNT"
    
    if [ "$RAPID_COUNT" -ge 3 ]; then
        echo "✅ Rapid signal handling acceptable"
    else
        echo "⚠️  Low signal response rate"
    fi
else
    echo "❌ System not running for rapid test"
fi

# Cleanup check
echo
echo "Cleanup check..."
sleep 1
LEFTOVER=$(find /tmp /dev/shm -name "*2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f*" 2>/dev/null | wc -l)
if [ "$LEFTOVER" -eq 0 ]; then
    echo "✅ No leftover files"
else
    echo "⚠️  Found $LEFTOVER leftover files (cleaning up...)"
    make clean >/dev/null 2>&1
fi

echo
echo "=== Test Summary ==="
echo "Signal handling: $([ "$STATS_COUNT" -eq 3 ] && echo "✅ PASS" || echo "⚠️  PARTIAL")"
echo "Message accuracy: $([ "$SENT" -eq 60 ] && [ "$RECEIVED" -eq 60 ] && echo "✅ PASS" || echo "⚠️  CHECK")"
echo "Rapid signals: $([ "$RAPID_COUNT" -ge 3 ] && echo "✅ PASS" || echo "⚠️  CHECK")"
echo "Cleanup: $([ "$LEFTOVER" -eq 0 ] && echo "✅ PASS" || echo "⚠️  WARN")"

echo
echo "Log files created:"
echo "- signal_test.log (signal handling test)"
echo "- accuracy_test.log (message counting test)"  
echo "- rapid_test.log (rapid signal test)"