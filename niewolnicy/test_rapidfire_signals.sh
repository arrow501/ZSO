#!/bin/bash

echo "=== SIGNAL COUNTING VERIFICATION TEST ==="

# This test specifically verifies the requirement:
# "statystyki mają wyświetlić się tyle razy ile razy został wysłany sygnał"

cleanup() {
    killall master slave stats_reader 2>/dev/null || true
    sleep 1
}

count_displays() {
    grep -c "=== Master Statistics" stats_test.log 2>/dev/null || echo 0
}

verify_exact_count() {
    local expected=$1
    local description="$2"
    local actual=$(count_displays)
    
    echo -n "Test: $description (expected: $expected, actual: $actual) ... "
    
    if [ "$actual" -eq "$expected" ]; then
        echo "✅ PASS"
        return 0
    else
        echo "❌ FAIL"
        return 1
    fi
}

trap cleanup EXIT
cleanup

# Build if needed
if [ ! -x "./master" ] || [ ! -x "./stats_reader" ]; then
    echo "Building project..."
    make clean && make
fi

echo "Starting master..."
./master > master_test.log 2>&1 &
MASTER_PID=$!
sleep 2

if ! kill -0 $MASTER_PID 2>/dev/null; then
    echo "❌ Master failed to start"
    exit 1
fi

echo "Starting stats_reader..."
./stats_reader > stats_test.log 2>&1 &
READER_PID=$!
sleep 1

if ! kill -0 $READER_PID 2>/dev/null; then
    echo "❌ Stats reader failed to start"
    exit 1
fi

echo ""
echo "🧪 Running signal counting tests..."
echo ""

# Test 1: Zero signals initially
sleep 1
verify_exact_count 0 "Initial state (no signals sent)"

# Test 2: One signal
echo "Sending 1 signal..."
> stats_test.log
kill -USR1 $MASTER_PID
sleep 2
verify_exact_count 1 "One signal"

# Test 3: Two more signals (total should be 2, not 3)
echo "Sending 2 more signals..."
> stats_test.log
kill -USR1 $MASTER_PID
kill -USR1 $MASTER_PID
sleep 3
verify_exact_count 2 "Two signals after log clear"

# Test 4: Rapid fire 5 signals
echo "Sending 5 rapid signals..."
> stats_test.log
for i in {1..5}; do
    kill -USR1 $MASTER_PID
done
sleep 4
verify_exact_count 5 "Five rapid signals"

# Test 5: Signals with delays
echo "Sending 3 signals with delays..."
> stats_test.log
kill -USR1 $MASTER_PID
sleep 1
kill -USR1 $MASTER_PID
sleep 1  
kill -USR1 $MASTER_PID
sleep 2
verify_exact_count 3 "Three signals with delays"

# Test 6: Many signals (stress test)
echo "Sending 10 signals (stress test)..."
> stats_test.log
for i in {1..10}; do
    kill -USR1 $MASTER_PID
done
sleep 5
verify_exact_count 10 "Ten signals with small delays"

echo ""
echo "📊 Additional verification - checking log content..."

# Verify that each display actually shows statistics
DISPLAYS_WITH_STATS=$(grep -c "Total messages:" stats_test.log 2>/dev/null || echo 0)
echo "Displays containing actual statistics: $DISPLAYS_WITH_STATS"

# Show last few lines of stats output
echo ""
echo "Sample of stats output:"
echo "----------------------"
tail -20 stats_test.log | head -10
echo "----------------------"

echo ""
echo "🔍 Process status:"
echo "Master PID: $MASTER_PID ($(kill -0 $MASTER_PID 2>/dev/null && echo "alive" || echo "dead"))"
echo "Reader PID: $READER_PID ($(kill -0 $READER_PID 2>/dev/null && echo "alive" || echo "dead"))"

echo ""
echo "📁 Log files available for inspection:"
echo "  - stats_test.log (stats reader output)"
echo "  - master_test.log (master output)"

echo ""
echo "=== Signal Counting Test Complete ==="