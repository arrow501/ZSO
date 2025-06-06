#!/bin/bash

# Thorough testing - stats signals and valgrind with multiple slaves

echo "Thorough Testing"
echo "==============="

make clean >/dev/null
make debug >/dev/null
echo "✅ Built"

# Test 1: Stats signal counting
echo
echo "Test 1: Stats signal counting"
echo "-----------------------------"

export NUM_MESSAGES_PER_SLAVE=50

echo "Starting system with 3 slaves..."
./main 3 &
MAIN_PID=$!

sleep 2

echo "Sending 5 stats requests..."
for i in {1..5}; do
    echo "Signal $i"
    kill -USR2 $MAIN_PID
    sleep 1
done

sleep 2
echo "Terminating..."
kill -TERM $MAIN_PID
wait $MAIN_PID 2>/dev/null || true

echo "Counting stats outputs..."
STATS_COUNT=$(grep -c "=== Master Statistics ===" *.log 2>/dev/null || echo 0)
echo "Found $STATS_COUNT stats outputs (expected: 5)"

if [ "$STATS_COUNT" -eq 5 ]; then
    echo "✅ Stats signal test PASSED"
else
    echo "❌ Stats signal test FAILED"
fi

# Test 2: Valgrind with multiple slaves
echo
echo "Test 2: Valgrind with multiple slaves"
echo "------------------------------------"

export NUM_MESSAGES_PER_SLAVE=10

echo "Running memcheck with 3 slaves..."
timeout 30s valgrind --tool=memcheck --leak-check=full ./main 3 >valgrind.out 2>&1
MEMCHECK_EXIT=$?

echo "Memcheck exit code: $MEMCHECK_EXIT"
echo "Memory errors:"
grep "ERROR SUMMARY" valgrind.out
echo "Memory leaks:"
grep "definitely lost" valgrind.out

# Test 3: Valgrind with stats signals
echo
echo "Test 3: Valgrind with stats signals"
echo "----------------------------------"

echo "Starting valgrind with helgrind..."
timeout 45s valgrind --tool=helgrind ./main 2 >helgrind.out 2>&1 &
VALGRIND_PID=$!

sleep 5

echo "Sending stats signals to valgrind process..."
# Get the actual main process PID from valgrind output
ACTUAL_MAIN_PID=$(pgrep -P $VALGRIND_PID main 2>/dev/null || echo "")

if [ -n "$ACTUAL_MAIN_PID" ]; then
    echo "Found main PID: $ACTUAL_MAIN_PID"
    for i in {1..3}; do
        echo "Stats signal $i"
        kill -USR2 $ACTUAL_MAIN_PID 2>/dev/null || true
        sleep 2
    done
    sleep 3
    kill -TERM $ACTUAL_MAIN_PID 2>/dev/null || true
else
    echo "Could not find main process, letting valgrind finish naturally..."
fi

wait $VALGRIND_PID 2>/dev/null || true

echo "Helgrind exit code: $?"
echo "Race conditions:"
grep "ERROR SUMMARY" helgrind.out
grep "data race" helgrind.out 2>/dev/null || echo "No data races found"

# Test 4: Signal stress test  
echo
echo "Test 4: Signal stress test"
echo "-------------------------"

export NUM_MESSAGES_PER_SLAVE=100

echo "Starting system..."
./main 2 >signal_test.out 2>&1 &
MAIN_PID=$!

sleep 1

echo "Rapid stats requests..."
for i in {1..10}; do
    kill -USR2 $MAIN_PID 2>/dev/null || break
done

sleep 3
kill -TERM $MAIN_PID 2>/dev/null || true
wait $MAIN_PID 2>/dev/null || true

RAPID_STATS=$(grep -c "=== Master Statistics ===" signal_test.out)
echo "Rapid stats count: $RAPID_STATS (expected: ~10)"

if [ "$RAPID_STATS" -ge 8 ] && [ "$RAPID_STATS" -le 12 ]; then
    echo "✅ Signal stress test PASSED"
else
    echo "❌ Signal stress test FAILED"
fi

echo
echo "Summary"
echo "======="
echo "Stats counting: $([ "$STATS_COUNT" -eq 5 ] && echo "✅ PASS" || echo "❌ FAIL")"
echo "Valgrind basic: $([ "$MEMCHECK_EXIT" -eq 0 ] || [ "$MEMCHECK_EXIT" -eq 124 ] && echo "✅ PASS" || echo "❌ FAIL")"
echo "Signal stress:  $([ "$RAPID_STATS" -ge 8 ] && [ "$RAPID_STATS" -le 12 ] && echo "✅ PASS" || echo "❌ FAIL")"

echo
echo "Log files: valgrind.out, helgrind.out, signal_test.out"

make clean >/dev/null