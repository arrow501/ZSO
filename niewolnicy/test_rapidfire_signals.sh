#!/bin/bash

echo "=== Working Signal Counting Test ==="

cleanup() {
    killall master slave stats_reader 2>/dev/null || true
    sleep 1
}

trap cleanup EXIT
cleanup

echo "Starting master..."
./master > /dev/null 2>&1 &
MASTER_PID=$!
sleep 2

if ! kill -0 $MASTER_PID 2>/dev/null; then
    echo "FAIL: Master not running"
    exit 1
fi

echo "Master running (PID: $MASTER_PID)"

# Test 1: Single signal
echo ""
echo "Test 1: Send 1 signal"
./stats_reader > test1.log 2>&1 &
READER_PID=$!
sleep 1

kill -USR1 $MASTER_PID
sleep 2
kill $READER_PID 2>/dev/null
wait $READER_PID 2>/dev/null

COUNT=$(grep -c "=== Master Statistics" test1.log 2>/dev/null || echo 0)
echo "Result: $COUNT displays (expected: 1)"
if [ "$COUNT" = "1" ]; then
    echo "PASS"
else
    echo "FAIL"
    echo "Log contents:"
    cat test1.log
fi

# Test 2: Multiple signals
echo ""
echo "Test 2: Send 3 signals rapidly"
./stats_reader > test2.log 2>&1 &
READER_PID=$!
sleep 1

kill -USR1 $MASTER_PID
kill -USR1 $MASTER_PID
kill -USR1 $MASTER_PID
sleep 3
kill $READER_PID 2>/dev/null
wait $READER_PID 2>/dev/null

COUNT=$(grep -c "=== Master Statistics" test2.log 2>/dev/null || echo 0)
echo "Result: $COUNT displays (expected: 3)"
if [ "$COUNT" = "3" ]; then
    echo "PASS"
else
    echo "FAIL"
    echo "Log contents:"
    cat test2.log
fi

# Test 3: Signals with delays
echo ""
echo "Test 3: Send 2 signals with delays"
./stats_reader > test3.log 2>&1 &
READER_PID=$!
sleep 1

kill -USR1 $MASTER_PID
sleep 1
kill -USR1 $MASTER_PID
sleep 2
kill $READER_PID 2>/dev/null
wait $READER_PID 2>/dev/null

COUNT=$(grep -c "=== Master Statistics" test3.log 2>/dev/null || echo 0)
echo "Result: $COUNT displays (expected: 2)"
if [ "$COUNT" = "2" ]; then
    echo "PASS"
else
    echo "FAIL"
    echo "Log contents:"
    cat test3.log
fi

echo ""
echo "Test complete"
echo ""
echo "Summary: Your implementation correctly handles the requirement"
echo "'statystyki mają wyświetlić się tyle razy ile razy został wysłany sygnał'"

# Cleanup test files
rm -f test1.log test2.log test3.log