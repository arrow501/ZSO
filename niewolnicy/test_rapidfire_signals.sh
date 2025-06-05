#!/bin/bash

echo "=== Simple Signal Counting Test ==="

cleanup() {
    killall master slave stats_reader 2>/dev/null || true
    sleep 1
}

count_stats() {
    local count=$(grep -c "=== Master Statistics" stats.log 2>/dev/null || echo 0)
    echo $count
}

trap cleanup EXIT
cleanup

echo "Building..."
make clean > /dev/null 2>&1
make > /dev/null 2>&1

echo "Starting master..."
./master > master.log 2>&1 &
MASTER_PID=$!
sleep 2

if ! kill -0 $MASTER_PID 2>/dev/null; then
    echo "FAIL: Master not running"
    exit 1
fi
echo "Master running (PID: $MASTER_PID)"

echo "Starting stats_reader..."
./stats_reader > stats.log 2>&1 &
READER_PID=$!
sleep 1

if ! kill -0 $READER_PID 2>/dev/null; then
    echo "FAIL: Stats reader not running"
    exit 1
fi
echo "Stats reader running (PID: $READER_PID)"

echo ""
echo "Test 1: Send 1 signal"
> stats.log
kill -USR1 $MASTER_PID
sleep 2
COUNT=$(count_stats)
echo "Result: $COUNT displays (expected: 1)"
if [ "$COUNT" = "1" ]; then
    echo "PASS"
else
    echo "FAIL"
fi

echo ""
echo "Test 2: Send 3 signals rapidly"
> stats.log
kill -USR1 $MASTER_PID
kill -USR1 $MASTER_PID
kill -USR1 $MASTER_PID
sleep 3
COUNT=$(count_stats)
echo "Result: $COUNT displays (expected: 3)"
if [ "$COUNT" = "3" ]; then
    echo "PASS"
else
    echo "FAIL"
fi

echo ""
echo "Test 3: Send 2 signals with delay"
> stats.log
kill -USR1 $MASTER_PID
sleep 1
kill -USR1 $MASTER_PID
sleep 2
COUNT=$(count_stats)
echo "Result: $COUNT displays (expected: 2)"
if [ "$COUNT" = "2" ]; then
    echo "PASS"
else
    echo "FAIL"
fi

echo ""
echo "Stats reader log contents:"
echo "=========================="
cat stats.log
echo "=========================="

echo ""
echo "Master log (last 10 lines):"
echo "============================"
tail -10 master.log
echo "============================"

echo ""
echo "Test complete"