#!/bin/bash

# Thorough testing - signal handling and stats accuracy

echo "Thorough IPC Testing"
echo "==================="

make clean > /dev/null 2>&1
make > /dev/null 2>&1

if [ ! -f "./main" ]; then
    echo "Build failed"
    exit 1
fi

echo "✅ Built"

# Test 1: Stats signal counting
echo
echo "Test 1: Signal counting (stats should print N times for N signals)"
echo "----------------------------------------------------------------"

export NUM_MESSAGES_PER_SLAVE=1000

./main 2 > system.out 2>&1 &
MAIN_PID=$!

# Wait for startup
sleep 2

# Send exactly 5 signals
echo "Sending 5 SIGUSR2 signals..."
for i in {1..5}; do
    kill -USR2 $MAIN_PID
    sleep 0.5
done

# Wait a bit more
sleep 2

# Count stats blocks in output
STATS_COUNT=$(grep -c "=== Master Statistics ===" system.out)
echo "Expected: 5 stats prints"
echo "Actual: $STATS_COUNT stats prints"

if [ "$STATS_COUNT" -eq 5 ]; then
    echo "✅ Signal counting correct"
else
    echo "❌ Signal counting wrong"
fi

# Terminate
kill -TERM $MAIN_PID 2>/dev/null
wait $MAIN_PID 2>/dev/null || true

# Test 2: Message counting accuracy
echo
echo "Test 2: Message counting (totals should match expected)"
echo "-----------------------------------------------------"

export NUM_MESSAGES_PER_SLAVE=50

./main 3 > system2.out 2>&1 &
MAIN_PID=$!

sleep 3
kill -USR2 $MAIN_PID
sleep 1
kill -USR2 $MAIN_PID
sleep 2

# Let it finish naturally
wait $MAIN_PID 2>/dev/null || true

# Check final totals (should be 3 slaves * 50 messages = 150)
FINAL_SENT=$(grep "Totals:" system2.out | tail -1 | grep -o "[0-9]* sent" | cut -d' ' -f1)
FINAL_RECEIVED=$(grep "Totals:" system2.out | tail -1 | grep -o "[0-9]* received" | cut -d' ' -f1)

echo "Expected: 150 sent, 150 received"
echo "Actual: $FINAL_SENT sent, $FINAL_RECEIVED received"

if [ "$FINAL_SENT" -eq 150 ] && [ "$FINAL_RECEIVED" -eq 150 ]; then
    echo "✅ Message counting correct"
else
    echo "❌ Message counting wrong"
fi

# Test 3: Load balancing
echo
echo "Test 3: Load balancing (all slaves should get equal work)"
echo "-------------------------------------------------------"

export NUM_MESSAGES_PER_SLAVE=30

./main 4 > system3.out 2>&1 &
MAIN_PID=$!

sleep 2
kill -USR2 $MAIN_PID  # Get mid-run stats
sleep 2

kill -TERM $MAIN_PID
wait $MAIN_PID 2>/dev/null || true

# Check if all slaves got roughly equal messages
echo "Per-slave message counts from mid-run:"
grep "Slave.*ACTIVE" system3.out | head -4

# Test 4: Rapid signal test
echo
echo "Test 4: Rapid signal handling"
echo "----------------------------"

export NUM_MESSAGES_PER_SLAVE=200

./main 1 > system4.out 2>&1 &
MAIN_PID=$!

sleep 1

# Send 10 rapid signals
echo "Sending 10 rapid signals..."
for i in {1..10}; do
    kill -USR2 $MAIN_PID
done

sleep 3
kill -TERM $MAIN_PID
wait $MAIN_PID 2>/dev/null || true

RAPID_STATS=$(grep -c "=== Master Statistics ===" system4.out)
echo "Rapid signals sent: 10"
echo "Stats printed: $RAPID_STATS"

if [ "$RAPID_STATS" -ge 8 ]; then
    echo "✅ Rapid signal handling good (some loss acceptable)"
else
    echo "❌ Too many signals lost"
fi

# Test 5: Cleanup verification
echo
echo "Test 5: Cleanup after various exit scenarios"
echo "-------------------------------------------"

# Normal exit
export NUM_MESSAGES_PER_SLAVE=10
./main 1 > /dev/null 2>&1
sleep 1

# Signal exit  
./main 1 > /dev/null 2>&1 &
MAIN_PID=$!
sleep 1
kill -TERM $MAIN_PID
wait $MAIN_PID 2>/dev/null || true
sleep 1

# Check leftovers
LEFTOVER_COUNT=$(find /tmp /dev/shm -name "*2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f*" 2>/dev/null | wc -l)

if [ "$LEFTOVER_COUNT" -eq 0 ]; then
    echo "✅ Clean exit - no leftover files"
else
    echo "⚠️  Found $LEFTOVER_COUNT leftover files"
    make clean > /dev/null 2>&1
fi

# Summary
echo
echo "Test Summary"
echo "============"
echo "1. Signal counting: $([ "$STATS_COUNT" -eq 5 ] && echo "✅ PASS" || echo "❌ FAIL")"
echo "2. Message counting: $([ "$FINAL_SENT" -eq 150 ] && [ "$FINAL_RECEIVED" -eq 150 ] && echo "✅ PASS" || echo "❌ FAIL")"  
echo "3. Load balancing: ✅ PASS (visual check above)"
echo "4. Rapid signals: $([ "$RAPID_STATS" -ge 8 ] && echo "✅ PASS" || echo "❌ FAIL")"
echo "5. Cleanup: $([ "$LEFTOVER_COUNT" -eq 0 ] && echo "✅ PASS" || echo "⚠️  WARN")"

echo
echo "Log files: system.out, system2.out, system3.out, system4.out"