#!/bin/bash

# Simple signal test - don't mess with NUM_MESSAGES_PER_SLAVE
# Just test signal handling with default parameters

set -e

echo "=== Simple Signal Test ==="

make clean >/dev/null 2>&1
make >/dev/null 2>&1
echo "✅ Built"

# Test 1: Signals with 0.2s delay
echo
echo "Test 1: Signals with 0.2s delay"
echo "-------------------------------"

./main 2 > test1.log 2>&1 &
PID=$!

sleep 1  # Let it start

echo "Sending 3 signals with 0.2s delay..."
kill -USR2 $PID
sleep 0.2
kill -USR2 $PID  
sleep 0.2
kill -USR2 $PID

sleep 1
kill -TERM $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

STATS1=$(grep -c "=== Master Statistics ===" test1.log || echo "0")
echo "Result: $STATS1/3 stats printed"

# Test 2: Rapid signals (no delay)
echo
echo "Test 2: Rapid signals (no delay)"
echo "-------------------------------"

./main 2 > test2.log 2>&1 &
PID=$!

sleep 1  # Let it start

echo "Sending 3 rapid signals..."
kill -USR2 $PID
kill -USR2 $PID
kill -USR2 $PID

sleep 1
kill -TERM $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

STATS2=$(grep -c "=== Master Statistics ===" test2.log || echo "0")
echo "Result: $STATS2/3 stats printed"

# Test 3: Natural completion (no signals)
echo
echo "Test 3: Natural completion"
echo "-------------------------"

echo "Running system to natural completion..."
time ./main 1 > test3.log 2>&1

FINAL_STATS=$(grep "Totals:" test3.log | tail -1 || echo "No totals found")
echo "Final stats: $FINAL_STATS"

# Test 4: Interactive-style test
echo
echo "Test 4: Interactive simulation"
echo "-----------------------------"

./main 1 > test4.log 2>&1 &
PID=$!

sleep 0.5
echo "Requesting stats at different times..."

if kill -0 $PID 2>/dev/null; then
    kill -USR2 $PID
    echo "  Stats request 1 sent"
fi

sleep 0.5

if kill -0 $PID 2>/dev/null; then
    kill -USR2 $PID
    echo "  Stats request 2 sent"
fi

# Let it finish naturally
wait $PID 2>/dev/null || true

INTERACTIVE_STATS=$(grep -c "=== Master Statistics ===" test4.log || echo "0")
echo "Interactive stats: $INTERACTIVE_STATS"

# Summary
echo
echo "=== Results ==="
echo "0.2s delay:     $STATS1/3 ($([ "$STATS1" -eq 3 ] && echo "✅ PERFECT" || echo "⚠️  Some lost"))"
echo "No delay:       $STATS2/3 ($([ "$STATS2" -ge 1 ] && echo "✅ WORKING" || echo "❌ BROKEN"))"
echo "Natural run:    $([ -n "$FINAL_STATS" ] && echo "✅ COMPLETED" || echo "❌ NO STATS")"
echo "Interactive:    $INTERACTIVE_STATS ($([ "$INTERACTIVE_STATS" -ge 1 ] && echo "✅ WORKING" || echo "❌ BROKEN"))"

echo
echo "📁 Log files: test1.log, test2.log, test3.log, test4.log"

# Quick cleanup check
LEFTOVER=$(find /tmp /dev/shm -name "*2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f*" 2>/dev/null | wc -l)
echo "Leftover files: $LEFTOVER"

if [ "$LEFTOVER" -gt 0 ]; then
    make clean >/dev/null 2>&1
fi

echo
if [ "$STATS1" -eq 3 ] && [ "$STATS2" -ge 1 ] && [ "$INTERACTIVE_STATS" -ge 1 ]; then
    echo "🎉 All tests show signal handling works!"
else
    echo "⚠️  Some signal tests had issues - check logs"
fi