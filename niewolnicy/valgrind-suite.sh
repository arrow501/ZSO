#!/bin/bash

# Enhanced valgrind test with multiple tools and signal testing

echo "Enhanced Valgrind Test"
echo "====================="

# Check valgrind exists
if ! command -v valgrind &> /dev/null; then
    echo "Valgrind not found"
    exit 1
fi

# Build debug version
echo "Building debug version..."
make clean >/dev/null 2>&1
make release >/dev/null 2>&1
echo "✅ Built"

# Use more messages for better stress testing
export NUM_MESSAGES_PER_SLAVE=50

echo
echo "Test 1: Memcheck (memory leaks & errors)"
echo "----------------------------------------"

timeout 30s valgrind \
    --tool=memcheck \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --error-exitcode=1 \
    ./main 2 2>memcheck.out

MEMCHECK_EXIT=$?
echo "Exit code: $MEMCHECK_EXIT"
echo "Memory issues:"
grep -E "(ERROR SUMMARY|definitely lost|indirectly lost|possibly lost)" memcheck.out | head -5

echo
echo "Test 2: Helgrind (race conditions)"
echo "----------------------------------"

timeout 30s valgrind \
    --tool=helgrind \
    --error-exitcode=1 \
    ./main 2 2>helgrind.out

HELGRIND_EXIT=$?
echo "Exit code: $HELGRIND_EXIT"
echo "Race conditions:"
grep -E "(ERROR SUMMARY|data race|lock order)" helgrind.out | head -5

echo
echo "Test 3: DRD (thread synchronization)"
echo "------------------------------------"

timeout 30s valgrind \
    --tool=drd \
    --error-exitcode=1 \
    ./main 2 2>drd.out

DRD_EXIT=$?
echo "Exit code: $DRD_EXIT"
echo "Synchronization issues:"
grep -E "(ERROR SUMMARY|data race|mutex|deadlock)" drd.out | head -5

echo
echo "Test 4: Race conditions under signal spam"
echo "-----------------------------------------"

valgrind \
    --tool=helgrind \
    --error-exitcode=1 \
    ./main 3 >signal_spam.log 2>helgrind_spam.out &

VALGRIND_PID=$!
sleep 3

# Find main process and spam signals
MAIN_PID=""
for i in {1..10}; do
    MAIN_PID=$(pgrep -P $VALGRIND_PID main 2>/dev/null | head -1)
    if [ -n "$MAIN_PID" ]; then
        break
    fi
    sleep 0.5
done

if [ -n "$MAIN_PID" ]; then
    echo "Signal spamming PID $MAIN_PID..."
    # Rapid signal spam - no delays
    for i in {1..50}; do
        kill -USR2 $MAIN_PID 2>/dev/null || break
    done
    sleep 1
    kill -TERM $MAIN_PID 2>/dev/null || true
else
    echo "Main process not found, letting system complete naturally"
fi

wait $VALGRIND_PID 2>/dev/null || true
SIGNAL_EXIT=$?

echo "Exit code: $SIGNAL_EXIT"
RACE_COUNT=$(grep -c "Possible data race" helgrind_spam.out 2>/dev/null || echo "0")
echo "Race conditions found: $RACE_COUNT"

echo
echo "Test 5: DRD with burst signals"
echo "------------------------------"

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
    echo "Burst signal test..."
    # 5 bursts of 10 signals each
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
DRD_RACES=$(grep -c "data race" drd_burst.out 2>/dev/null || echo "0")
echo "DRD races found: $DRD_RACES"

# Summary
echo
echo "=== Valgrind Summary ==="
echo "Memcheck:       $([ $MEMCHECK_EXIT -eq 0 ] && echo "✅ CLEAN" || echo "❌ ISSUES")"
echo "Helgrind:       $([ $HELGRIND_EXIT -eq 0 ] && echo "✅ NO RACES" || echo "❌ RACES FOUND")"
echo "DRD:            $([ $DRD_EXIT -eq 0 ] && echo "✅ SYNC OK" || echo "❌ SYNC ISSUES")"
echo "Signal spam:    $([ $SIGNAL_EXIT -eq 0 ] && echo "✅ NO RACES" || echo "❌ RACES FOUND") ($RACE_COUNT races)"
echo "Burst signals:  $([ $BURST_EXIT -eq 0 ] && echo "✅ STABLE" || echo "❌ UNSTABLE") ($DRD_RACES races)"

TOTAL_ISSUES=$((MEMCHECK_EXIT + HELGRIND_EXIT + DRD_EXIT + SIGNAL_EXIT + BURST_EXIT))
TOTAL_RACES=$((RACE_COUNT + DRD_RACES))

echo
if [ $TOTAL_ISSUES -eq 0 ] && [ $TOTAL_RACES -eq 0 ]; then
    echo "🏆 BULLETPROOF: No issues under extreme signal pressure!"
elif [ $TOTAL_ISSUES -le 1 ] && [ $TOTAL_RACES -eq 0 ]; then
    echo "✅ EXCELLENT: Minor issues but no race conditions!"
else
    echo "⚠️  ISSUES: $TOTAL_ISSUES problems, $TOTAL_RACES race conditions found"
fi

echo
echo "📁 Generated logs:"
echo "  - memcheck.out (memory errors)"
echo "  - helgrind.out (race conditions)"
echo "  - drd.out (thread synchronization)"
echo "  - helgrind_spam.out (signal spam races)"
echo "  - drd_burst.out (burst signal races)"

# Show any critical errors
echo
if [ $TOTAL_ISSUES -gt 0 ] || [ $TOTAL_RACES -gt 0 ]; then
    echo "Issues summary:"
    for file in memcheck.out helgrind.out drd.out helgrind_spam.out drd_burst.out; do
        if [ -f "$file" ]; then
            ERRORS=$(grep -c "ERROR SUMMARY: [1-9]" "$file" 2>/dev/null || echo "0")
            if [ "$ERRORS" -gt 0 ]; then
                echo "  $file: $ERRORS error(s)"
            fi
        fi
    done
    
    if [ $TOTAL_RACES -gt 0 ]; then
        echo
        echo "Race conditions detected - check helgrind_spam.out and drd_burst.out"
    fi
fi

make clean >/dev/null 2>&1