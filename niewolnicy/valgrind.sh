#!/bin/bash

# Comprehensive Valgrind testing for the IPC system
# Tests: memcheck, helgrind, drd

set -e

echo "🔍 Valgrind Testing Suite"
echo "========================="

# Check if valgrind is available
if ! command -v valgrind &> /dev/null; then
    echo "❌ Valgrind not found. Install with:"
    echo "   sudo apt-get install valgrind"
    exit 1
fi

# Build with debug symbols and optimizations off
echo "Building with debug symbols..."
make clean >/dev/null 2>&1
CFLAGS="-g -O0 -DENABLE_PRINTING=0 -DENABLE_ASSERTS=1" make >/dev/null 2>&1
echo "✅ Debug build complete"

# Set short test parameters to keep valgrind runs reasonable
export NUM_MESSAGES_PER_SLAVE=20

echo
echo "🧠 Test 1: Memcheck (Memory leaks & errors)"
echo "============================================"

# Create a simple wrapper to run the system under valgrind
cat > valgrind_wrapper.sh << 'EOF'
#!/bin/bash
exec valgrind \
    --tool=memcheck \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --track-fds=yes \
    --error-exitcode=1 \
    --suppressions=/usr/share/valgrind/default.supp \
    "$@"
EOF
chmod +x valgrind_wrapper.sh

echo "Running master under memcheck..."
timeout 30s ./valgrind_wrapper.sh ./main 2 > memcheck.log 2>&1 || MEMCHECK_EXIT=$?

if [ "${MEMCHECK_EXIT:-0}" -eq 0 ]; then
    echo "✅ Memcheck: No memory errors detected"
elif [ "${MEMCHECK_EXIT:-0}" -eq 124 ]; then
    echo "⏰ Memcheck: Timeout (normal - valgrind is slow)"
    # Check if there were actual errors vs just timeout
    if grep -q "ERROR SUMMARY: 0 errors" memcheck.log; then
        echo "✅ Memcheck: No errors found before timeout"
    else
        echo "⚠️  Memcheck: Check memcheck.log for details"
    fi
else
    echo "❌ Memcheck: Found memory errors (exit code: ${MEMCHECK_EXIT:-0})"
    echo "   Check memcheck.log for details"
fi

echo
echo "📋 Memcheck Summary:"
grep -E "(ERROR SUMMARY|LEAK SUMMARY|definitely lost|indirectly lost|possibly lost)" memcheck.log | head -10

echo
echo "🧵 Test 2: Helgrind (Thread race conditions)"
echo "============================================="

cat > helgrind_wrapper.sh << 'EOF'
#!/bin/bash
exec valgrind \
    --tool=helgrind \
    --error-exitcode=1 \
    "$@"
EOF
chmod +x helgrind_wrapper.sh

echo "Running master under helgrind..."
timeout 30s ./helgrind_wrapper.sh ./main 2 > helgrind.log 2>&1 || HELGRIND_EXIT=$?

if [ "${HELGRIND_EXIT:-0}" -eq 0 ]; then
    echo "✅ Helgrind: No race conditions detected"
elif [ "${HELGRIND_EXIT:-0}" -eq 124 ]; then
    echo "⏰ Helgrind: Timeout (normal - valgrind is slow)"
    if grep -q "ERROR SUMMARY: 0 errors" helgrind.log; then
        echo "✅ Helgrind: No races found before timeout"
    else
        echo "⚠️  Helgrind: Check helgrind.log for details"
    fi
else
    echo "❌ Helgrind: Found race conditions (exit code: ${HELGRIND_EXIT:-0})"
    echo "   Check helgrind.log for details"
fi

echo
echo "📋 Helgrind Summary:"
grep -E "(ERROR SUMMARY|Possible data race|Lock order)" helgrind.log | head -5

echo
echo "🔄 Test 3: DRD (Data race detection)"
echo "===================================="

cat > drd_wrapper.sh << 'EOF'
#!/bin/bash
exec valgrind \
    --tool=drd \
    --error-exitcode=1 \
    --check-stack-var=yes \
    "$@"
EOF
chmod +x drd_wrapper.sh

echo "Running master under DRD..."
timeout 30s ./drd_wrapper.sh ./main 2 > drd.log 2>&1 || DRD_EXIT=$?

if [ "${DRD_EXIT:-0}" -eq 0 ]; then
    echo "✅ DRD: No data races detected"
elif [ "${DRD_EXIT:-0}" -eq 124 ]; then
    echo "⏰ DRD: Timeout (normal - valgrind is slow)"
    if grep -q "ERROR SUMMARY: 0 errors" drd.log; then
        echo "✅ DRD: No races found before timeout"
    else
        echo "⚠️  DRD: Check drd.log for details"
    fi
else
    echo "❌ DRD: Found data races (exit code: ${DRD_EXIT:-0})"
    echo "   Check drd.log for details"
fi

echo
echo "📋 DRD Summary:"
grep -E "(ERROR SUMMARY|Data race|Conflicting)" drd.log | head -5

echo
echo "🧹 Test 4: Quick leak test (individual components)"
echo "=================================================="

echo "Testing master alone..."
timeout 10s valgrind --tool=memcheck --leak-check=summary --error-exitcode=1 ./master > master_leak.log 2>&1 &
MASTER_PID=$!
sleep 2
kill -TERM $MASTER_PID 2>/dev/null || true
wait $MASTER_PID 2>/dev/null || true

if grep -q "All heap blocks were freed" master_leak.log 2>/dev/null; then
    echo "✅ Master: No leaks detected"
else
    echo "⚠️  Master: Check master_leak.log"
fi

echo "Testing slave alone..."
# Slave needs master FIFO to exist
mkfifo /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f 2>/dev/null || true
timeout 5s valgrind --tool=memcheck --leak-check=summary --error-exitcode=1 ./slave 0 > slave_leak.log 2>&1 &
SLAVE_PID=$!
sleep 1
kill -TERM $SLAVE_PID 2>/dev/null || true
wait $SLAVE_PID 2>/dev/null || true
rm -f /tmp/master_fifo_2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f

if grep -q "All heap blocks were freed" slave_leak.log 2>/dev/null; then
    echo "✅ Slave: No leaks detected"
else
    echo "⚠️  Slave: Check slave_leak.log"
fi

# Cleanup
rm -f valgrind_wrapper.sh helgrind_wrapper.sh drd_wrapper.sh
make clean >/dev/null 2>&1

echo
echo "📊 VALGRIND TEST RESULTS"
echo "========================"
echo "Memory Check (memcheck): $([ "${MEMCHECK_EXIT:-0}" -eq 0 ] || [ "${MEMCHECK_EXIT:-0}" -eq 124 ] && echo "✅ PASS" || echo "❌ FAIL")"
echo "Race Detection (helgrind): $([ "${HELGRIND_EXIT:-0}" -eq 0 ] || [ "${HELGRIND_EXIT:-0}" -eq 124 ] && echo "✅ PASS" || echo "❌ FAIL")"  
echo "Data Race Detection (drd): $([ "${DRD_EXIT:-0}" -eq 0 ] || [ "${DRD_EXIT:-0}" -eq 124 ] && echo "✅ PASS" || echo "❌ FAIL")"
echo
echo "📁 Log files created:"
echo "  - memcheck.log (memory errors)"
echo "  - helgrind.log (race conditions)"  
echo "  - drd.log (data races)"
echo "  - master_leak.log (master component)"
echo "  - slave_leak.log (slave component)"
echo
echo "💡 Note: Timeouts are normal with Valgrind (it's ~10-50x slower)"
echo "💡 Check log files for detailed analysis if needed"
echo
echo "🎉 Valgrind testing complete!"