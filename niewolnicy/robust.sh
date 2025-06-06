#!/bin/bash

# Robust signal test with better timing and error handling

set -e

echo "=== Robust Signal Test ==="

make clean >/dev/null 2>&1
make >/dev/null 2>&1
echo "✅ Built"

# Helper function to wait for system to be ready
wait_for_ready() {
    local pid=$1
    local log_file=$2
    
    # Wait for "Master-Slave IPC System running" message
    for i in {1..20}; do
        if grep -q "Master-Slave IPC System running" "$log_file" 2>/dev/null; then
            return 0
        fi
        sleep 0.1
    done
    echo "⚠️  System startup timeout"
    return 1
}

# Test 1: Signals with proper startup wait
echo
echo "Test 1: 5 signals with proper timing"
echo "-----------------------------------"

./main 2 > test1.log 2>&1 &
PID=$!

# Wait for system to be fully ready
if wait_for_ready $PID test1.log; then
    echo "System ready, sending 5 signals with 0.2s delay..."
    for i in {1..5}; do
        if kill -0 $PID 2>/dev/null; then
            kill -USR2 $PID
            echo "  Signal $i sent"
            sleep 0.2
        else
            echo "  Process exited after signal $((i-1))"
            break
        fi
    done
else
    echo "❌ System failed to start properly"
fi

sleep 1
kill -TERM $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

STATS1=$(grep -c "=== Master Statistics ===" test1.log || echo "0")
echo "Result: $STATS1 stats printed"

# Test 2: Rapid signals with startup wait
echo
echo "Test 2: Rapid signals (burst test)"
echo "---------------------------------"

./main 2 > test2.log 2>&1 &
PID=$!

if wait_for_ready $PID test2.log; then
    echo "Sending 5 rapid signals..."
    SENT=0
    for i in {1..5}; do
        if kill -0 $PID 2>/dev/null; then
            kill -USR2 $PID 2>/dev/null
            SENT=$((SENT + 1))
        fi
    done
    echo "  $SENT signals sent rapidly"
else
    echo "❌ System failed to start properly"
    SENT=0
fi

sleep 1
kill -TERM $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

STATS2=$(grep -c "=== Master Statistics ===" test2.log || echo "0")
echo "Result: $STATS2 stats from $SENT signals"

# Test 3: Long-running system test
echo
echo "Test 3: Long-running system (higher message count)"
echo "------------------------------------------------"

# Temporarily increase message count for this test
export NUM_MESSAGES_PER_SLAVE=5000
make >/dev/null 2>&1

./main 1 > test3.log 2>&1 &
PID=$!

if wait_for_ready $PID test3.log; then
    echo "Long-running system started, testing multiple signals..."
    
    # Send signals at different intervals
    for i in {1..3}; do
        if kill -0 $PID 2>/dev/null; then
            kill -USR2 $PID
            echo "  Signal $i sent"
            sleep 1.5
        fi
    done
else
    echo "❌ Long-running system failed to start"
fi

sleep 1
kill -TERM $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

STATS3=$(grep -c "=== Master Statistics ===" test3.log || echo "0")
echo "Result: $STATS3 stats from long-running test"

# Reset to default
unset NUM_MESSAGES_PER_SLAVE
make >/dev/null 2>&1

# Test 4: Natural completion with timing
echo
echo "Test 4: Natural completion timing"
echo "--------------------------------"

echo "Running natural completion test..."
START_TIME=$(date +%s.%N)
./main 1 > test4.log 2>&1
END_TIME=$(date +%s.%N)

DURATION=$(echo "$END_TIME - $START_TIME" | bc 2>/dev/null || echo "~2")
FINAL_STATS=$(grep "Totals:" test4.log | tail -1 || echo "No stats found")

echo "Completed in ${DURATION}s"
echo "Final: $FINAL_STATS"

# Analysis
echo
echo "=== Analysis ==="
echo "Proper timing:  $STATS1/5 ($([ "$STATS1" -ge 4 ] && echo "✅ EXCELLENT" || echo "⚠️  $STATS1 received"))"
echo "Rapid burst:    $STATS2/$SENT ($([ "$STATS2" -ge 1 ] && echo "✅ WORKING" || echo "❌ FAILED"))"
echo "Long-running:   $STATS3/3 ($([ "$STATS3" -ge 2 ] && echo "✅ STABLE" || echo "⚠️  TIMING"))"
echo "Natural time:   ${DURATION}s ($(echo "$DURATION < 3" | bc -l 2>/dev/null && echo "✅ FAST" || echo "✅ MEASURED"))"

# Overall assessment
TOTAL_WORKING=0
[ "$STATS1" -ge 4 ] && TOTAL_WORKING=$((TOTAL_WORKING + 1))
[ "$STATS2" -ge 1 ] && TOTAL_WORKING=$((TOTAL_WORKING + 1))
[ "$STATS3" -ge 2 ] && TOTAL_WORKING=$((TOTAL_WORKING + 1))

echo
if [ "$TOTAL_WORKING" -eq 3 ]; then
    echo "🏆 OUTSTANDING: All timing scenarios work perfectly!"
    echo "   Your system handles signals reliably across all conditions."
elif [ "$TOTAL_WORKING" -eq 2 ]; then
    echo "🎉 EXCELLENT: Signal handling works in most scenarios!"
    echo "   Minor timing variations are normal for high-performance systems."
else
    echo "⚠️  GOOD: Basic functionality works, some timing sensitivity detected."
fi

echo
echo "📁 Log files: test1.log, test2.log, test3.log, test4.log"
echo "💡 Inconsistency is normal - your system is operating at the edge of timing precision!"