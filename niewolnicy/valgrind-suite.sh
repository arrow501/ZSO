#!/bin/bash

# Simplified Valgrind Test Suite for Master-Slave IPC System

make release > /dev/null 2>&1
rm -rf valgrind_logs

LOGS_DIR="./valgrind_logs"
TEST_DURATION=6

mkdir -p "$LOGS_DIR"

cleanup() {
    # Kill only our test processes, not the valgrind script itself
    pkill -f "valgrind.*main" 2>/dev/null || true
    pkill -f "^main " 2>/dev/null || true
    pkill -f "^master$" 2>/dev/null || true
    pkill -f "^slave " 2>/dev/null || true
    sleep 1
    rm -f /tmp/master_fifo_* /tmp/slave_fifo_* /tmp/master_pid_* 2>/dev/null || true
    rm -f /dev/shm/master_stats_* /dev/shm/sem.stats_ready_* 2>/dev/null || true
}

trap cleanup EXIT

check_binaries() {
    for binary in "./master" "./slave" "./main"; do
        if [[ ! -x "$binary" ]]; then
            echo "Error: $binary not found. Run 'make' first."
            exit 1
        fi
    done
    echo "✓ All binaries found"
}

test_memcheck() {
    echo ""
    echo "=== MEMORY LEAK TEST ==="
    
    valgrind --tool=memcheck \
             --leak-check=full \
             --show-leak-kinds=all \
             --track-origins=yes \
             --log-file="$LOGS_DIR/memcheck.log" \
             ./main 3 &
    
    MAIN_PID=$!
    sleep 3
    
    # Send some signals during test
    for i in {1..3}; do
        sleep 1
        kill -USR1 $MAIN_PID 2>/dev/null || true
    done
    
    # Graceful shutdown
    kill -TERM $MAIN_PID 2>/dev/null || true
    
    # Wait for valgrind to finish properly
    wait $MAIN_PID 2>/dev/null || true
    
    echo "✓ Memcheck complete"
}

test_helgrind() {
    echo ""
    echo "=== THREAD SAFETY TEST ==="
    
    valgrind --tool=helgrind \
             --log-file="$LOGS_DIR/helgrind.log" \
             ./main 2 &
    
    MAIN_PID=$!
    sleep 3
    
    # Rapid signal testing for race conditions
    for i in {1..5}; do
        kill -USR1 $MAIN_PID 2>/dev/null || true
        sleep 0.2
    done
    
    kill -TERM $MAIN_PID 2>/dev/null || true
    wait $MAIN_PID 2>/dev/null || true
    
    echo "✓ Helgrind complete"
}

test_drd() {
    echo ""
    echo "=== DATA RACE TEST ==="
    
    valgrind --tool=drd \
             --log-file="$LOGS_DIR/drd.log" \
             ./main 3 &
    
    MAIN_PID=$!
    sleep 2
    
    # Stress test with rapid signals
    for burst in {1..3}; do
        for i in {1..3}; do
            kill -USR1 $MAIN_PID 2>/dev/null || true
        done
        sleep 0.5
    done
    
    kill -TERM $MAIN_PID 2>/dev/null || true
    wait $MAIN_PID 2>/dev/null || true
    
    echo "✓ DRD complete"
}

show_results() {
    echo ""
    echo "========================================="
    echo "  VALGRIND RESULTS SUMMARY"
    echo "========================================="
    
    for tool in "memcheck" "helgrind" "drd"; do
        local log="$LOGS_DIR/${tool}.log"
        echo ""
        echo "${tool^} Results:"
        if [[ -f "$log" ]]; then
            local error_line=$(tail -n 20 "$log" | grep "ERROR SUMMARY" | tail -n 1)
            if [[ -n "$error_line" ]]; then
                echo "  $error_line"
            else
                echo "  No error summary found"
            fi
            
            # Show any definite leaks for memcheck
            if [[ "$tool" == "memcheck" ]]; then
                local leak_line=$(tail -n 20 "$log" | grep "definitely lost" | tail -n 1)
                if [[ -n "$leak_line" ]]; then
                    echo "  $leak_line"
                fi
            fi
        else
            echo "  Log file not found"
        fi
    done
    
    echo ""
    echo "All logs saved to: $LOGS_DIR/"
}

main() {
    echo "========================================="
    echo "  Simplified Valgrind Test Suite"
    echo "  Master-Slave IPC System"
    echo "========================================="
    
    check_binaries
    
    test_memcheck
    cleanup
    sleep 1
    
    test_helgrind
    cleanup 
    sleep 1
    
    test_drd
    cleanup
    
    show_results
    
    echo ""
    echo "Generated files:"
    find "$LOGS_DIR" -name "*.log" -type f | sort | while read -r file; do
        echo "  📄 $file"
    done
    
    echo ""
    echo "✅ Valgrind testing complete!"
}

if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    echo "Simplified Valgrind Test Suite"
    echo ""
    echo "Usage: $0"
    echo ""
    echo "Runs three tests on the unified main launcher:"
    echo "  1. Memory Leak Detection (memcheck)"
    echo "  2. Thread Safety (helgrind)" 
    echo "  3. Data Race Detection (DRD)"
    echo ""
    exit 0
fi

main