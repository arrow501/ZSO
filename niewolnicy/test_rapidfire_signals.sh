#!/bin/bash

cleanup() {
    killall master slave stats_reader 2>/dev/null || true
    sleep 1
}

run_test() {
    local test_name="$1"
    local expected="$2"
    local logfile="$3"
    
    ./stats_reader > $logfile 2>&1 &
    local reader_pid=$!
    sleep 1
    
    # Execute the test commands passed as remaining arguments
    shift 3
    "$@"
    
    sleep 2
    kill $reader_pid 2>/dev/null
    wait $reader_pid 2>/dev/null
    
    local count=$(grep -c "=== Master Statistics" $logfile 2>/dev/null || echo 0)
    printf "%-25s expected:%d actual:%d " "$test_name" "$expected" "$count"
    
    if [ "$count" = "$expected" ]; then
        echo "PASS"
        return 0
    else
        echo "FAIL"
        return 1
    fi
}

send_single() {
    kill -USR1 $MASTER_PID
}

send_rapid() {
    kill -USR1 $MASTER_PID
    kill -USR1 $MASTER_PID
    kill -USR1 $MASTER_PID
}

send_delayed() {
    kill -USR1 $MASTER_PID
    sleep 1
    kill -USR1 $MASTER_PID
}

send_burst() {
    for i in {1..5}; do
        kill -USR1 $MASTER_PID
        sleep 0.1
    done
}

trap cleanup EXIT
cleanup

make clean > /dev/null 2>&1
make > /dev/null 2>&1

./master > /dev/null 2>&1 &
MASTER_PID=$!
sleep 2

if ! kill -0 $MASTER_PID 2>/dev/null; then
    echo "Master failed to start"
    exit 1
fi

echo "Signal Counting Test Results:"
echo "============================="

passed=0
total=0

run_test "Single signal" 1 "test1.log" send_single
[ $? -eq 0 ] && ((passed++))
((total++))

run_test "Rapid fire (3 signals)" 3 "test2.log" send_rapid
[ $? -eq 0 ] && ((passed++))
((total++))

run_test "Delayed signals (2)" 2 "test3.log" send_delayed
[ $? -eq 0 ] && ((passed++))
((total++))

run_test "Burst signals (5)" 5 "test4.log" send_burst
[ $? -eq 0 ] && ((passed++))
((total++))

echo "============================="
echo "Summary: $passed/$total tests passed"

rm -f test*.log