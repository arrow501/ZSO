#!/bin/bash

# Signal handling test - clean and simple

set -e

echo "Signal Test"
echo "==========="

make clean >/dev/null 2>&1
make >/dev/null 2>&1
echo "Built successfully"

# Test 1: Spaced signals
echo
echo "Test 1: Spaced signals (0.001s delay)"
echo "--------------------------------------"

./main 2 > test1.log 2>&1 &
PID=$!
sleep 0.1

for i in {1..10}; do
    kill -USR2 $PID
    sleep 0.001
done

sleep 1
kill -TERM $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

STATS1=$(grep -c "=== Master Statistics ===" test1.log || echo "0")
echo "Result: $STATS1/10"

# Test 2: Burst signals
echo
echo "Test 2: Burst signals (no delay)"
echo "--------------------------------"

./main 2 > test2.log 2>&1 &
PID=$!
sleep 0.1

kill -USR2 $PID
kill -USR2 $PID
kill -USR2 $PID
kill -USR2 $PID
kill -USR2 $PID
kill -USR2 $PID
kill -USR2 $PID
kill -USR2 $PID
kill -USR2 $PID
kill -USR2 $PID

sleep 1
kill -TERM $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

STATS2=$(grep -c "=== Master Statistics ===" test2.log || echo "0")
echo "Result: $STATS2/10"

# Test 3: Natural completion
echo
echo "Test 3: Natural completion"
echo "--------------------------"

./main 1 > test3.log 2>&1 &
PID=$!
sleep 0.8

if kill -0 $PID 2>/dev/null; then
    kill -USR2 $PID
    sleep 0.3
fi

wait $PID 2>/dev/null || true

NATURAL_STATS=$(grep -c "=== Master Statistics ===" test3.log || echo "0")
FINAL_STATS=$(grep "Totals:" test3.log | tail -1 || echo "None")
echo "Stats: $NATURAL_STATS"
echo "Final: $FINAL_STATS"

# Test 4: Interactive test
echo
echo "Test 4: Interactive test"
echo "------------------------"

./main 1 > test4.log 2>&1 &
PID=$!

sleep 0.5
if kill -0 $PID 2>/dev/null; then
    kill -USR2 $PID
fi

sleep 0.5
if kill -0 $PID 2>/dev/null; then
    kill -USR2 $PID
fi

wait $PID 2>/dev/null || true

INTERACTIVE_STATS=$(grep -c "=== Master Statistics ===" test4.log || echo "0")
echo "Stats: $INTERACTIVE_STATS"

# Results
echo
echo "Summary"
echo "-------"
echo "Spaced:      $STATS1/10"
echo "Burst:       $STATS2/10"
echo "Natural:     $NATURAL_STATS"
echo "Interactive: $INTERACTIVE_STATS"

# Cleanup
LEFTOVER=$(find /tmp /dev/shm -name "*2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f*" 2>/dev/null | wc -l)
if [ "$LEFTOVER" -gt 0 ]; then
    make clean >/dev/null 2>&1
fi

echo
if [ "$STATS1" -eq 10 ] && [ "$STATS2" -ge 5 ] && [ "$NATURAL_STATS" -ge 1 ] && [ "$INTERACTIVE_STATS" -ge 1 ]; then
    echo "All tests passed"
else
    echo "Some issues detected - check logs"
fi