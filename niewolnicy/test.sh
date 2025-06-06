#!/bin/bash

# Comprehensive Test Suite for Master-Slave IPC System
# Tests functionality, edge cases, and stress scenarios

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
TEST_UUID="2e518cc1-6b7d-45c9-a7f6-1a7d35fcbb3f"
MASTER_FIFO="/tmp/master_fifo_${TEST_UUID}"
SLAVE_FIFO_PREFIX="/tmp/slave_fifo_${TEST_UUID}_"
MASTER_PID_FILE="/tmp/master_pid_${TEST_UUID}"
SHM_NAME="/master_stats_${TEST_UUID}"
SEM_NAME="/stats_ready_${TEST_UUID}"

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((TESTS_PASSED++))
}

log_error() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((TESTS_FAILED++))
}

log_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

start_test() {
    ((TESTS_RUN++))
    echo -e "\n${BLUE}=== Test $TESTS_RUN: $1 ===${NC}"
}

cleanup_test_files() {
    log_info "Cleaning up test files..."
    pkill -f "./main" 2>/dev/null || true
    pkill -f "./master" 2>/dev/null || true
    pkill -f "./slave" 2>/dev/null || true
    sleep 0.5
    
    rm -f "$MASTER_FIFO" 2>/dev/null || true
    rm -f "${SLAVE_FIFO_PREFIX}"* 2>/dev/null || true
    rm -f "$MASTER_PID_FILE" 2>/dev/null || true
    rm -f "/dev/shm$SHM_NAME" 2>/dev/null || true
    rm -f "/dev/shm/sem.stats_ready_${TEST_UUID}" 2>/dev/null || true
}

wait_for_file() {
    local file="$1"
    local timeout="${2:-10}"
    local count=0
    
    while [ ! -e "$file" ] && [ $count -lt $timeout ]; do
        sleep 0.1
        ((count++))
    done
    
    [ -e "$file" ]
}

wait_for_process_count() {
    local expected="$1"
    local timeout="${2:-10}"
    local count=0
    
    while [ $count -lt $timeout ]; do
        local current=$(pgrep -f "./main\|./master\|./slave" | wc -l)
        if [ "$current" -eq "$expected" ]; then
            return 0
        fi
        sleep 0.1
        ((count++))
    done
    return 1
}

# Test 1: Build system
test_build() {
    start_test "Build System"
    
    if [ ! -f "Makefile" ]; then
        log_error "Makefile not found"
        return 1
    fi
    
    # Clean build
    make clean >/dev/null 2>&1
    
    # Build all targets
    if make all >/dev/null 2>&1; then
        log_success "Build successful"
    else
        log_error "Build failed"
        return 1
    fi
    
    # Check if executables exist
    for binary in master slave main; do
        if [ -x "./$binary" ]; then
            log_success "Binary $binary created"
        else
            log_error "Binary $binary missing or not executable"
            return 1
        fi
    done
}

# Test 2: Basic functionality
test_basic_functionality() {
    start_test "Basic Functionality (1 slave, short run)"
    
    cleanup_test_files
    
    # Start with 1 slave, short message limit
    timeout 10s make test >/dev/null 2>&1 &
    local main_pid=$!
    
    sleep 2
    
    # Check if processes are running
    if pgrep -f "./main" >/dev/null && pgrep -f "./master" >/dev/null && pgrep -f "./slave" >/dev/null; then
        log_success "All processes started successfully"
    else
        log_error "Not all processes started"
        kill $main_pid 2>/dev/null || true
        return 1
    fi
    
    # Check if FIFO files are created
    if [ -p "$MASTER_FIFO" ]; then
        log_success "Master FIFO created"
    else
        log_error "Master FIFO not created"
    fi
    
    if [ -p "${SLAVE_FIFO_PREFIX}0" ]; then
        log_success "Slave FIFO created"
    else
        log_error "Slave FIFO not created"
    fi
    
    # Wait for natural completion
    wait $main_pid 2>/dev/null || true
    
    # Check if processes cleaned up
    sleep 1
    if ! pgrep -f "./main\|./master\|./slave" >/dev/null; then
        log_success "All processes terminated cleanly"
    else
        log_warning "Some processes still running after completion"
        cleanup_test_files
    fi
}

# Test 3: Multiple slaves
test_multiple_slaves() {
    start_test "Multiple Slaves (5 slaves)"
    
    cleanup_test_files
    
    # Start with 5 slaves
    timeout 15s ./main 5 >/dev/null 2>&1 &
    local main_pid=$!
    
    sleep 2
    
    # Check process count (1 main + 1 master + 5 slaves = 7)
    local process_count=$(pgrep -f "./main\|./master\|./slave" | wc -l)
    if [ "$process_count" -eq 7 ]; then
        log_success "Correct number of processes running ($process_count)"
    else
        log_error "Expected 7 processes, found $process_count"
    fi
    
    # Check if all slave FIFOs are created
    local fifo_count=0
    for i in {0..4}; do
        if [ -p "${SLAVE_FIFO_PREFIX}$i" ]; then
            ((fifo_count++))
        fi
    done
    
    if [ "$fifo_count" -eq 5 ]; then
        log_success "All slave FIFOs created"
    else
        log_error "Expected 5 slave FIFOs, found $fifo_count"
    fi
    
    # Send interrupt to main process
    kill -INT $main_pid 2>/dev/null || true
    sleep 2
    
    # Check cleanup
    if ! pgrep -f "./main\|./master\|./slave" >/dev/null; then
        log_success "All processes terminated after SIGINT"
    else
        log_error "Some processes still running after SIGINT"
        cleanup_test_files
    fi
}

# Test 4: Stats functionality
test_stats_functionality() {
    start_test "Stats Functionality"
    
    cleanup_test_files
    
    # Start system
    ./main 3 >/dev/null 2>&1 &
    local main_pid=$!
    
    sleep 3
    
    # Send stats request
    echo "Requesting stats..."
    echo "s" | timeout 5s ./main 3 > stats_output.tmp 2>&1 &
    local stats_pid=$!
    
    sleep 2
    kill -INT $stats_pid 2>/dev/null || true
    kill -INT $main_pid 2>/dev/null || true
    
    sleep 1
    
    # Check if stats output contains expected information
    if [ -f "stats_output.tmp" ]; then
        if grep -q "Master Statistics" stats_output.tmp && \
           grep -q "Slave Status" stats_output.tmp && \
           grep -q "Totals:" stats_output.tmp; then
            log_success "Stats output contains expected sections"
        else
            log_error "Stats output missing expected sections"
            cat stats_output.tmp
        fi
        rm -f stats_output.tmp
    else
        log_error "No stats output captured"
    fi
    
    cleanup_test_files
}

# Test 5: Edge cases
test_edge_cases() {
    start_test "Edge Cases"
    
    cleanup_test_files
    
    # Test with 0 slaves (should fail)
    if ./main 0 >/dev/null 2>&1; then
        log_error "Should reject 0 slaves"
    else
        log_success "Correctly rejects 0 slaves"
    fi
    
    # Test with too many slaves (should fail)
    if ./main 15 >/dev/null 2>&1; then
        log_error "Should reject >MAX_SLAVES"
    else
        log_success "Correctly rejects >MAX_SLAVES"
    fi
    
    # Test with invalid arguments
    if ./main abc >/dev/null 2>&1; then
        log_error "Should reject non-numeric arguments"
    else
        log_success "Correctly rejects non-numeric arguments"
    fi
    
    # Test without arguments
    if ./main >/dev/null 2>&1; then
        log_error "Should require arguments"
    else
        log_success "Correctly requires arguments"
    fi
}

# Test 6: Stress test
test_stress() {
    start_test "Stress Test (Max slaves, longer run)"
    
    cleanup_test_files
    
    # Compile with higher message count for stress test
    make clean >/dev/null 2>&1
    make NUM_MESSAGES_PER_SLAVE=100 >/dev/null 2>&1
    
    # Start with maximum slaves
    timeout 30s ./main 10 >/dev/null 2>&1 &
    local main_pid=$!
    
    sleep 5
    
    # Check if all processes are still running
    local process_count=$(pgrep -f "./main\|./master\|./slave" | wc -l)
    if [ "$process_count" -eq 12 ]; then  # 1 main + 1 master + 10 slaves
        log_success "All processes running under stress"
    else
        log_warning "Expected 12 processes, found $process_count"
    fi
    
    # Let it run and complete naturally
    wait $main_pid 2>/dev/null || true
    
    sleep 2
    
    # Check final cleanup
    if ! pgrep -f "./main\|./master\|./slave" >/dev/null; then
        log_success "Stress test completed and cleaned up"
    else
        log_error "Processes still running after stress test"
        cleanup_test_files
    fi
    
    # Rebuild with normal settings
    make clean >/dev/null 2>&1
    make >/dev/null 2>&1
}

# Test 7: Signal handling
test_signal_handling() {
    start_test "Signal Handling"
    
    cleanup_test_files
    
    # Test SIGTERM
    ./main 2 >/dev/null 2>&1 &
    local main_pid=$!
    
    sleep 2
    
    kill -TERM $main_pid
    sleep 2
    
    if ! pgrep -f "./main\|./master\|./slave" >/dev/null; then
        log_success "SIGTERM handled correctly"
    else
        log_error "SIGTERM not handled properly"
        cleanup_test_files
    fi
    
    # Test SIGQUIT
    ./main 2 >/dev/null 2>&1 &
    main_pid=$!
    
    sleep 2
    
    kill -QUIT $main_pid
    sleep 2
    
    if ! pgrep -f "./main\|./master\|./slave" >/dev/null; then
        log_success "SIGQUIT handled correctly"
    else
        log_error "SIGQUIT not handled properly"
        cleanup_test_files
    fi
}

# Test 8: Resource cleanup
test_resource_cleanup() {
    start_test "Resource Cleanup"
    
    cleanup_test_files
    
    # Start and stop system multiple times
    for i in {1..3}; do
        ./main 3 >/dev/null 2>&1 &
        local main_pid=$!
        
        sleep 1
        kill -INT $main_pid
        sleep 1
        
        # Check if resources are cleaned up
        if [ -p "$MASTER_FIFO" ] || [ -e "/dev/shm$SHM_NAME" ]; then
            log_error "Resources not cleaned up properly in iteration $i"
            cleanup_test_files
            return 1
        fi
    done
    
    log_success "Resources cleaned up properly in multiple iterations"
}

# Main test runner
run_all_tests() {
    echo -e "${BLUE}Starting IPC System Test Suite${NC}"
    echo "=================================="
    
    # Check if we're in the right directory
    if [ ! -f "Makefile" ] || [ ! -d "src" ] || [ ! -d "include" ]; then
        log_error "Please run this script from the project root directory"
        exit 1
    fi
    
    # Run tests
    test_build
    test_basic_functionality
    test_multiple_slaves
    test_stats_functionality
    test_edge_cases
    test_stress
    test_signal_handling
    test_resource_cleanup
    
    # Final cleanup
    cleanup_test_files
    
    # Print summary
    echo -e "\n${BLUE}=================================="
    echo "Test Suite Summary"
    echo "=================================="
    echo -e "Tests Run:    ${TESTS_RUN}"
    echo -e "Tests Passed: ${GREEN}${TESTS_PASSED}${NC}"
    echo -e "Tests Failed: ${RED}${TESTS_FAILED}${NC}"
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "\n${GREEN}🎉 All tests passed!${NC}"
        exit 0
    else
        echo -e "\n${RED}❌ Some tests failed.${NC}"
        exit 1
    fi
}

# Allow running individual tests
case "${1:-all}" in
    "build") test_build ;;
    "basic") test_basic_functionality ;;
    "multiple") test_multiple_slaves ;;
    "stats") test_stats_functionality ;;
    "edge") test_edge_cases ;;
    "stress") test_stress ;;
    "signals") test_signal_handling ;;
    "cleanup") test_resource_cleanup ;;
    "all"|*) run_all_tests ;;
esac