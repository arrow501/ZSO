#!/bin/bash

# Valgrind test suite - clean format

set -e

echo "Valgrind Test Suite"
echo "==================="

if ! command -v valgrind &> /dev/null; then
    echo "Valgrind not found"
    exit 1
fi

make clean >/dev/null 2>&1
make release >/dev/null 2>&1
echo "Built successfully"

export NUM_MESSAGES_PER_SLAVE=50

# Test 1: Basic memcheck
echo
echo "Test 1: Memcheck"
echo "----------------"

timeout 30s valgrind \
    --tool=memcheck \
    --leak-check=full \
    --error-exitcode=1 \
    ./main 2 2>memcheck.out

MEMCHECK_EXIT=$?
echo "Exit code: $MEMCHECK_EXIT"

# Test 2: Helgrind
echo
echo "Test 2: Helgrind"
echo "----------------"

timeout 30s valgrind \
    --tool=helgrind \
    --error-exitcode=1 \
    ./main 2 2>helgrind.out

HELGRIND_EXIT=$?
echo "Exit code: $HELGRIND_EXIT"

# Test 3: DRD
echo
echo "Test 3: DRD"
echo "-----------"

timeout 30s valgrind \
    --tool=drd \
    --error-exitcode=1 \
    ./main 2 2>drd.out

DRD_EXIT=$?
echo "Exit code: $DRD_EXIT"

# Test 4: Signal spam test
echo
echo "Test 4: Signal spam"
echo "-------------------"

valgrind \
    --tool=helgrind \
    --error-exitcode=1 \
    ./main 3 >signal_spam.log 2>helgrind_spam.out &

VALGRIND_PID=$!
sleep 3

MAIN_PID=""
for i in {1..10}; do
    MAIN_PID=$(pgrep -P $VALGRIND_PID main 2>/dev/null | head -1)
    if [ -n "$MAIN_PID" ]; then
        break
    fi
    sleep 0.5
done

if [ -n "$MAIN_PID" ]; then
    for i in {1..50}; do
        kill -USR2 $MAIN_PID 2>/dev/null || break
    done
    sleep 1
    kill -TERM $MAIN_PID 2>/dev/null || true
fi

wait $VALGRIND_PID 2>/dev/null || true
SIGNAL_EXIT=$?
echo "Exit code: $SIGNAL_EXIT"

# Test 5: Burst signals
echo
echo "Test 5: Burst signals"
echo "---------------------"

timeout 30s valgrind \
    --tool=drd \
    --error-exitcode=1 \
    ./main 2 2>drd_burst.out &

VALGRIND_PID=$!
sleep 2

MAIN_PID=""
for i in {1..5}; do
    MAIN_PID=$(pgrep -P $VALGRIND_PID main 2>/dev/null | head -1)
    if [ -n "$MAIN_PID" ]; then
        break
    fi
    sleep 0.5
done

if [ -n "$MAIN_PID" ]; then
    for burst in {1..5}; do
        for i in {1..10}; do
            kill -USR2 $MAIN_PID 2>/dev/null || break
        done
        sleep 0.1
    done
    sleep 1
    kill -TERM $MAIN_PID 2>/dev/null || true
fi

wait $VALGRIND_PID 2>/dev/null || true
BURST_EXIT=$?
echo "Exit code: $BURST_EXIT"

# Summary
echo
echo "Summary"
echo "-------"
echo "Memcheck:     $MEMCHECK_EXIT"
echo "Helgrind:     $HELGRIND_EXIT"  
echo "DRD:          $DRD_EXIT"
echo "Signal spam:  $SIGNAL_EXIT"
echo "Burst:        $BURST_EXIT"

TOTAL_ISSUES=$((MEMCHECK_EXIT + HELGRIND_EXIT + DRD_EXIT + SIGNAL_EXIT + BURST_EXIT))

echo
if [ $TOTAL_ISSUES -eq 0 ]; then
    echo "All tests passed"
else
    echo "$TOTAL_ISSUES test(s) failed"
fi

# Print all error summaries
echo
echo "Error Details"
echo "-------------"

echo "Memcheck:"
grep "ERROR SUMMARY:" memcheck.out 2>/dev/null || echo "No output"

echo
echo "Helgrind:"
grep "ERROR SUMMARY:" helgrind.out 2>/dev/null || echo "No output"

echo
echo "DRD:"
grep "ERROR SUMMARY:" drd.out 2>/dev/null || echo "No output"

echo
echo "Signal spam:"
grep "ERROR SUMMARY:" helgrind_spam.out 2>/dev/null || echo "No output"

echo
echo "Burst signals:"
grep "ERROR SUMMARY:" drd_burst.out 2>/dev/null || echo "No output"

# Race condition details
RACE_COUNT=$(grep -c "Possible data race" helgrind_spam.out 2>/dev/null || echo "0")
DRD_RACES=$(grep -c "data race" drd_burst.out 2>/dev/null || echo "0")

make clean >/dev/null 2>&1