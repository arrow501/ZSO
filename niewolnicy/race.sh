#!/bin/bash

# Race condition test with signal spamming

echo "Race Condition Signal Spam Test"
echo "==============================="

if ! command -v valgrind &> /dev/null; then
    echo "Valgrind not found"
    exit 1
fi

make clean >/dev/null 2>&1
make release >/dev/null 2>&1
echo "✅ Built"

export NUM_MESSAGES_PER_SLAVE=200  # Enough to keep system busy

echo
echo "Test 1: Helgrind with rapid signal spam"
echo "---------------------------------------"

valgrind \
    --tool=helgrind \
    --error-exitcode=1 \
    ./main 3 >signal_spam.log 2>helgrind_spam.out &

VALGRIND_PID=$!
sleep 2

# Get the actual main process PID
MAIN_PID=""
for i in {1..10}; do
    MAIN_PID=$(pgrep -P $VALGRIND_PID main 2>/dev/null | head -1)
    if [ -n "$MAIN_PID" ]; then
        break
    fi
    sleep 0.5
done

if [ -n "$MAIN_PID" ]; then
    echo "Found main PID: $MAIN_PID"
    echo "Spamming 50 signals rapidly..."
    
    # Rapid signal spam - no delays
    for i in {1..50}; do
        kill -USR2 $MAIN_PID 2>/dev/null || break
    done
    
    echo "Waiting 2 seconds..."
    sleep 2
    
    echo "Another 30 signals..."
    for i in {1..30}; do
        kill -USR2 $MAIN_PID 2>/dev/null || break
    done
    
    sleep 1
    kill -TERM $MAIN_PID 2>/dev/null || true
else
    echo "Could not find main process"
fi

wait $VALGRIND_PID 2>/dev/null || true
HELGRIND_EXIT=$?

echo "Helgrind exit code: $HELGRIND_EXIT"
HELGRIND_ERRORS=$(grep -c "ERROR SUMMARY: [1-9]" helgrind_spam.out 2>/dev/null || echo "0")
RACE_CONDITIONS=$(grep -c "Possible data race" helgrind_spam.out 2>/dev/null || echo "0")

echo "Race conditions found: $RACE_CONDITIONS"
echo "Total helgrind errors: $HELGRIND_ERRORS"

echo
echo "Test 2: DRD with signal spam"
echo "----------------------------"

valgrind \
    --tool=drd \
    --error-exitcode=1 \
    ./main 2 >signal_spam2.log 2>drd_spam.out &

VALGRIND_PID=$!
sleep 2

MAIN_PID=""
for i in {1..10}; do
    MAIN_PID=$(pgrep -P $VALGRIND_PID main 2>/dev/null | head -1)
    if [ -n "$MAIN_PID" ]; then
        break
    fi
    sleep 0.5
done

if [ -n "$MAIN_PID" ]; then
    echo "Rapid fire 100 signals..."
    
    # Even more aggressive spam
    for i in {1..100}; do
        kill -USR2 $MAIN_PID 2>/dev/null || break
        # No sleep at all - maximum pressure
    done
    
    sleep 1
    kill -TERM $MAIN_PID 2>/dev/null || true
fi

wait $VALGRIND_PID 2>/dev/null || true
DRD_EXIT=$?

echo "DRD exit code: $DRD_EXIT"
DRD_ERRORS=$(grep -c "ERROR SUMMARY: [1-9]" drd_spam.out 2>/dev/null || echo "0")
DRD_RACES=$(grep -c "data race" drd_spam.out 2>/dev/null || echo "0")

echo "Data races found: $DRD_RACES"
echo "Total DRD errors: $DRD_ERRORS"

echo
echo "Test 3: Memcheck during signal storm"
echo "------------------------------------"

valgrind \
    --tool=memcheck \
    --leak-check=full \
    --error-exitcode=1 \
    ./main 2 >signal_spam3.log 2>memcheck_spam.out &

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
    echo "Signal storm test..."
    
    # Burst pattern - alternating rapid bursts
    for burst in {1..5}; do
        for i in {1..20}; do
            kill -USR2 $MAIN_PID 2>/dev/null || break
        done
        sleep 0.1  # Tiny pause between bursts
    done
    
    sleep 1
    kill -TERM $MAIN_PID 2>/dev/null || true
fi

wait $VALGRIND_PID 2>/dev/null || true
MEMCHECK_EXIT=$?

echo "Memcheck exit code: $MEMCHECK_EXIT"
MEMCHECK_ERRORS=$(grep -c "ERROR SUMMARY: [1-9]" memcheck_spam.out 2>/dev/null || echo "0")

echo "Memory errors during signal storm: $MEMCHECK_ERRORS"

# Results summary
echo
echo "=== Signal Spam Results ==="
echo "Helgrind races:     $RACE_CONDITIONS ($([ $RACE_CONDITIONS -eq 0 ] && echo "✅ NONE" || echo "❌ FOUND"))"
echo "DRD races:          $DRD_RACES ($([ $DRD_RACES -eq 0 ] && echo "✅ NONE" || echo "❌ FOUND"))"
echo "Memory errors:      $MEMCHECK_ERRORS ($([ $MEMCHECK_ERRORS -eq 0 ] && echo "✅ CLEAN" || echo "❌ ISSUES"))"

TOTAL_ISSUES=$((RACE_CONDITIONS + DRD_RACES + MEMCHECK_ERRORS))

echo
if [ $TOTAL_ISSUES -eq 0 ]; then
    echo "🏆 BULLETPROOF: No race conditions under extreme signal pressure!"
else
    echo "⚠️  ISSUES: $TOTAL_ISSUES problems found during signal spam"
    echo
    echo "Check these files for details:"
    echo "  - helgrind_spam.out"
    echo "  - drd_spam.out" 
    echo "  - memcheck_spam.out"
fi

# Show any actual race conditions found
if [ $RACE_CONDITIONS -gt 0 ] || [ $DRD_RACES -gt 0 ]; then
    echo
    echo "Race condition details:"
    if [ $RACE_CONDITIONS -gt 0 ]; then
        echo "Helgrind races:"
        grep -A 2 "Possible data race" helgrind_spam.out | head -10
    fi
    if [ $DRD_RACES -gt 0 ]; then
        echo "DRD races:"
        grep -A 2 "data race" drd_spam.out | head -10
    fi
fi

make clean >/dev/null 2>&1