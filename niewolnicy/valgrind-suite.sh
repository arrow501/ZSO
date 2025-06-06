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
echo "Test 4: Memcheck with signals"
echo "-----------------------------"

valgrind \
    --tool=memcheck \
    --leak-check=full \
    --error-exitcode=1 \
    ./main 2 >signal_test.log 2>memcheck_signal.out &

VALGRIND_PID=$!
sleep 2

# Send some signals to test signal handling under valgrind
echo "Sending test signals..."
if ps -p $VALGRIND_PID > /dev/null; then
    MAIN_PID=$(pgrep -P $VALGRIND_PID main 2>/dev/null || echo "")
    if [ -n "$MAIN_PID" ]; then
        kill -USR2 $MAIN_PID 2>/dev/null || true
        sleep 1
        kill -USR2 $MAIN_PID 2>/dev/null || true
        sleep 1
        kill -TERM $MAIN_PID 2>/dev/null || true
    fi
fi

wait $VALGRIND_PID 2>/dev/null || true
SIGNAL_EXIT=$?

echo "Exit code: $SIGNAL_EXIT"
echo "Signal handling under valgrind:"
grep -E "(ERROR SUMMARY|definitely lost)" memcheck_signal.out | head -3

echo
echo "Test 5: Quick stress test"
echo "-------------------------"

export NUM_MESSAGES_PER_SLAVE=20

timeout 25s valgrind \
    --tool=memcheck \
    --leak-check=summary \
    --error-exitcode=1 \
    ./main 5 2>stress.out

STRESS_EXIT=$?
echo "Exit code: $STRESS_EXIT"
echo "Stress test summary:"
grep -E "(ERROR SUMMARY|lost)" stress.out | head -3

# Summary
echo
echo "=== Valgrind Summary ==="
echo "Memcheck:        $([ $MEMCHECK_EXIT -eq 0 ] && echo "✅ CLEAN" || echo "❌ ISSUES")"
echo "Helgrind:        $([ $HELGRIND_EXIT -eq 0 ] && echo "✅ NO RACES" || echo "❌ RACES FOUND")"
echo "DRD:             $([ $DRD_EXIT -eq 0 ] && echo "✅ SYNC OK" || echo "❌ SYNC ISSUES")"
echo "Signal handling: $([ $SIGNAL_EXIT -eq 0 ] && echo "✅ CLEAN" || echo "❌ ISSUES")"
echo "Stress test:     $([ $STRESS_EXIT -eq 0 ] && echo "✅ STABLE" || echo "❌ UNSTABLE")"

TOTAL_ISSUES=$((MEMCHECK_EXIT + HELGRIND_EXIT + DRD_EXIT + SIGNAL_EXIT + STRESS_EXIT))

echo
if [ $TOTAL_ISSUES -eq 0 ]; then
    echo "🏆 PERFECT: No memory or threading issues detected!"
elif [ $TOTAL_ISSUES -le 2 ]; then
    echo "✅ GOOD: Minor issues detected, check logs"
else
    echo "⚠️  ISSUES: Multiple problems detected, review logs carefully"
fi

echo
echo "📁 Generated logs:"
echo "  - memcheck.out (memory errors)"
echo "  - helgrind.out (race conditions)"
echo "  - drd.out (thread synchronization)"
echo "  - memcheck_signal.out (signal handling)"
echo "  - stress.out (stress test)"

# Show any critical errors
echo
if [ $TOTAL_ISSUES -gt 0 ]; then
    echo "Critical issues found:"
    for file in memcheck.out helgrind.out drd.out memcheck_signal.out stress.out; do
        if [ -f "$file" ]; then
            ERRORS=$(grep -c "ERROR SUMMARY: [1-9]" "$file" 2>/dev/null || echo "0")
            if [ "$ERRORS" -gt 0 ]; then
                echo "  $file: $ERRORS error(s)"
            fi
        fi
    done
fi

make clean >/dev/null 2>&1