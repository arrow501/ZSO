#!/bin/bash

# Valgrind IPC Test Suite for Master-Slave System
# Specialized for multi-process pthread/semaphore testing
# 
# MIT-0 License
# 
# Copyright (c) 2025 Arrow
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Configuration
TEST_DURATION=10        # How long to run each test scenario (seconds)
NUM_SLAVES=3           # Number of slave processes to spawn
LOGS_DIR="./valgrind_logs"
MASTER_BIN="./master"
SLAVE_BIN="./slave"
STATS_BIN="./stats_reader"

# Create logs directory
mkdir -p "$LOGS_DIR"

# Cleanup function - ensures all processes are terminated
cleanup_processes() {
    echo -e "\n${YELLOW}Cleaning up processes...${NC}"
    pkill -f "$MASTER_BIN" 2>/dev/null || true
    pkill -f "$SLAVE_BIN" 2>/dev/null || true
    pkill -f "$STATS_BIN" 2>/dev/null || true
    pkill -f "valgrind.*master" 2>/dev/null || true
    pkill -f "valgrind.*slave" 2>/dev/null || true
    pkill -f "valgrind.*stats_reader" 2>/dev/null || true
    sleep 2
    
    # Clean up IPC resources
    rm -f /tmp/master_fifo /tmp/slave_fifo_* 2>/dev/null || true
    rm -f /dev/shm/master_stats /dev/shm/sem.stats_ready 2>/dev/null || true
    
    echo -e "${GREEN}✓ Cleanup complete${NC}"
}

# Set trap for cleanup
trap cleanup_processes EXIT INT TERM

# Function to check if binaries exist
check_binaries() {
    echo -e "${BLUE}Checking required binaries...${NC}"
    
    for binary in "$MASTER_BIN" "$SLAVE_BIN" "$STATS_BIN"; do
        if [[ ! -x "$binary" ]]; then
            echo -e "${RED}Error: $binary not found or not executable${NC}"
            echo -e "${YELLOW}Run 'make debug' to build the binaries${NC}"
            exit 1
        fi
    done
    
    echo -e "${GREEN}✓ All binaries found${NC}"
}

# Function to wait for process startup
wait_for_startup() {
    local process_name="$1"
    local timeout=5
    local count=0
    
    while [[ $count -lt $timeout ]]; do
        if pgrep -f "$process_name" >/dev/null; then
            return 0
        fi
        sleep 1
        ((count++))
    done
    
    echo -e "${RED}Warning: $process_name may not have started properly${NC}"
    return 1
}

# Function to run memory leak test with Valgrind memcheck
run_memcheck_test() {
    echo -e "\n${BOLD}${CYAN}================================${NC}"
    echo -e "${BOLD}${CYAN}    MEMCHECK ANALYSIS TEST      ${NC}"
    echo -e "${BOLD}${CYAN}================================${NC}"
    
    cleanup_processes
    
    local master_log="$LOGS_DIR/memcheck_master.log"
    local slave_logs=()
    local stats_log="$LOGS_DIR/memcheck_stats.log"
    
    echo -e "${YELLOW}Starting master under Valgrind memcheck...${NC}"
    valgrind --tool=memcheck \
             --leak-check=full \
             --show-leak-kinds=all \
             --track-origins=yes \
             --log-file="$master_log" \
             "$MASTER_BIN" &
    local master_pid=$!
    
    # Wait for master to initialize
    sleep 3
    wait_for_startup "valgrind.*master"
    
    echo -e "${YELLOW}Starting $NUM_SLAVES slaves under Valgrind memcheck...${NC}"
    for i in $(seq 0 $((NUM_SLAVES - 1))); do
        local slave_log="$LOGS_DIR/memcheck_slave_$i.log"
        slave_logs+=("$slave_log")
        
        valgrind --tool=memcheck \
                 --leak-check=full \
                 --show-leak-kinds=all \
                 --track-origins=yes \
                 --log-file="$slave_log" \
                 "$SLAVE_BIN" "$i" &
        
        sleep 1  # Stagger slave startup
    done
    
    # Wait for slaves to register
    sleep 2
    
    echo -e "${YELLOW}Starting stats reader under Valgrind memcheck...${NC}"
    valgrind --tool=memcheck \
             --leak-check=full \
             --show-leak-kinds=all \
             --track-origins=yes \
             --log-file="$stats_log" \
             "$STATS_BIN" &
    
    echo -e "${BLUE}Letting system run for $TEST_DURATION seconds...${NC}"
    
    # Trigger stats updates during test
    for i in $(seq 1 3); do
        sleep $((TEST_DURATION / 3))
        echo -e "${BLUE}Triggering stats update $i/3...${NC}"
        pkill -USR1 -f "valgrind.*master" 2>/dev/null || true
    done
    
    echo -e "${YELLOW}Terminating processes gracefully...${NC}"
    pkill -TERM -f "valgrind.*stats_reader" 2>/dev/null || true
    pkill -TERM -f "valgrind.*slave" 2>/dev/null || true
    sleep 2
    pkill -TERM -f "valgrind.*master" 2>/dev/null || true
    
    # Wait for processes to finish
    wait 2>/dev/null || true
    
    echo -e "${GREEN}✓ Memcheck analysis complete${NC}"
    
    # Analyze results
    echo -e "\n${PURPLE}${BOLD}Memcheck Results Summary:${NC}"
    analyze_memcheck_results "$master_log" "Master" "${slave_logs[@]}" "$stats_log"
}

# Function to run thread safety test with Helgrind
run_helgrind_test() {
    echo -e "\n${BOLD}${CYAN}================================${NC}"
    echo -e "${BOLD}${CYAN}    HELGRIND ANALYSIS TEST      ${NC}"
    echo -e "${BOLD}${CYAN}================================${NC}"
    
    cleanup_processes
    
    local master_log="$LOGS_DIR/helgrind_master.log"
    local slave_logs=()
    local stats_log="$LOGS_DIR/helgrind_stats.log"
    
    echo -e "${YELLOW}Starting master under Helgrind...${NC}"
    valgrind --tool=helgrind \
             --history-level=full \
             --log-file="$master_log" \
             "$MASTER_BIN" &
    
    sleep 3
    wait_for_startup "valgrind.*master"
    
    echo -e "${YELLOW}Starting $NUM_SLAVES slaves under Helgrind...${NC}"
    for i in $(seq 0 $((NUM_SLAVES - 1))); do
        local slave_log="$LOGS_DIR/helgrind_slave_$i.log"
        slave_logs+=("$slave_log")
        
        valgrind --tool=helgrind \
                 --history-level=full \
                 --log-file="$slave_log" \
                 "$SLAVE_BIN" "$i" &
        
        sleep 1
    done
    
    sleep 2
    
    echo -e "${YELLOW}Starting stats reader under Helgrind...${NC}"
    valgrind --tool=helgrind \
             --history-level=full \
             --log-file="$stats_log" \
             "$STATS_BIN" &
    
    echo -e "${BLUE}Generating concurrent load for $TEST_DURATION seconds...${NC}"
    
    # Create more aggressive concurrent activity for race detection
    for i in $(seq 1 5); do
        sleep $((TEST_DURATION / 5))
        echo -e "${BLUE}Concurrent activity burst $i/5...${NC}"
        pkill -USR1 -f "valgrind.*master" 2>/dev/null || true
        # Kill and restart a slave to test registration races
        if [[ $i -eq 3 ]]; then
            echo -e "${BLUE}Testing slave reconnection...${NC}"
            pkill -TERM -f "valgrind.*slave.*[[:space:]]1$" 2>/dev/null || true
            sleep 1
            valgrind --tool=helgrind \
                     --history-level=full \
                     --log-file="$LOGS_DIR/helgrind_slave_1_reconnect.log" \
                     "$SLAVE_BIN" 1 &
        fi
    done
    
    echo -e "${YELLOW}Terminating processes gracefully...${NC}"
    pkill -TERM -f "valgrind.*stats_reader" 2>/dev/null || true
    pkill -TERM -f "valgrind.*slave" 2>/dev/null || true
    sleep 2
    pkill -TERM -f "valgrind.*master" 2>/dev/null || true
    
    wait 2>/dev/null || true
    
    echo -e "${GREEN}✓ Helgrind analysis complete${NC}"
    
    # Analyze results
    echo -e "\n${PURPLE}${BOLD}Helgrind Results Summary:${NC}"
    analyze_helgrind_results "$master_log" "${slave_logs[@]}" "$stats_log"
}

# Function to run DRD analysis
run_drd_test() {
    echo -e "\n${BOLD}${CYAN}================================${NC}"
    echo -e "${BOLD}${CYAN}       DRD ANALYSIS TEST        ${NC}"
    echo -e "${BOLD}${CYAN}================================${NC}"
    
    cleanup_processes
    
    local master_log="$LOGS_DIR/drd_master.log"
    local slave_logs=()
    local stats_log="$LOGS_DIR/drd_stats.log"
    
    echo -e "${YELLOW}Starting master under DRD...${NC}"
    valgrind --tool=drd \
             --check-stack-var=yes \
             --log-file="$master_log" \
             "$MASTER_BIN" &
    
    sleep 3
    wait_for_startup "valgrind.*master"
    
    echo -e "${YELLOW}Starting $NUM_SLAVES slaves under DRD...${NC}"
    for i in $(seq 0 $((NUM_SLAVES - 1))); do
        local slave_log="$LOGS_DIR/drd_slave_$i.log"
        slave_logs+=("$slave_log")
        
        valgrind --tool=drd \
                 --check-stack-var=yes \
                 --log-file="$slave_log" \
                 "$SLAVE_BIN" "$i" &
        
        sleep 1
    done
    
    sleep 2
    
    echo -e "${YELLOW}Starting stats reader under DRD...${NC}"
    valgrind --tool=drd \
             --check-stack-var=yes \
             --log-file="$stats_log" \
             "$STATS_BIN" &
    
    echo -e "${BLUE}Running stress test for $TEST_DURATION seconds...${NC}"
    
    # DRD-specific stress testing
    for i in $(seq 1 6); do
        sleep $((TEST_DURATION / 6))
        echo -e "${BLUE}DRD stress burst $i/6...${NC}"
        
        # Rapid stats requests to stress mutex operations
        for j in $(seq 1 3); do
            pkill -USR1 -f "valgrind.*master" 2>/dev/null || true
            sleep 0.5
        done
        
        # Test rapid slave disconnections/reconnections
        if [[ $i -eq 2 ]]; then
            echo -e "${BLUE}Testing rapid slave lifecycle...${NC}"
            pkill -TERM -f "valgrind.*slave.*[[:space:]]2$" 2>/dev/null || true
            sleep 0.5
            valgrind --tool=drd \
                     --check-stack-var=yes \
                     --log-file="$LOGS_DIR/drd_slave_2_rapid.log" \
                     "$SLAVE_BIN" 2 &
        fi
    done
    
    echo -e "${YELLOW}Terminating processes gracefully...${NC}"
    pkill -TERM -f "valgrind.*stats_reader" 2>/dev/null || true
    pkill -TERM -f "valgrind.*slave" 2>/dev/null || true
    sleep 2
    pkill -TERM -f "valgrind.*master" 2>/dev/null || true
    
    wait 2>/dev/null || true
    
    echo -e "${GREEN}✓ DRD analysis complete${NC}"
    
    # Analyze results
    echo -e "\n${PURPLE}${BOLD}DRD Results Summary:${NC}"
    analyze_drd_results "$master_log" "${slave_logs[@]}" "$stats_log"
}

# Function to analyze memcheck results
analyze_memcheck_results() {
    local master_log="$1"
    shift
    local stats_log="${@: -1}"  # Last argument
    local slave_logs=("${@:1:$(($#-1))}")  # All but last argument
    
    echo -e "${CYAN}Master Process:${NC}"
    if [[ -f "$master_log" ]]; then
        local heap_summary=$(grep -A 5 "HEAP SUMMARY" "$master_log" | head -6)
        local leak_summary=$(grep -A 10 "LEAK SUMMARY" "$master_log" | head -11)
        local error_summary=$(grep "ERROR SUMMARY" "$master_log")
        
        if [[ -n "$heap_summary" ]]; then
            echo -e "${GREEN}$heap_summary${NC}"
        fi
        if [[ -n "$leak_summary" ]]; then
            echo -e "${GREEN}$leak_summary${NC}"
        fi
        if [[ -n "$error_summary" ]]; then
            echo -e "${BOLD}$error_summary${NC}"
        fi
    else
        echo -e "${RED}Master log not found${NC}"
    fi
    
    echo -e "\n${CYAN}Slave Processes:${NC}"
    for i in "${!slave_logs[@]}"; do
        local slave_log="${slave_logs[$i]}"
        if [[ -f "$slave_log" ]]; then
            local error_summary=$(grep "ERROR SUMMARY" "$slave_log")
            local definitely_lost=$(grep "definitely lost:" "$slave_log")
            echo -e "${BLUE}Slave $i: ${error_summary:-No error summary}${NC}"
            if [[ -n "$definitely_lost" ]]; then
                echo -e "${YELLOW}  $definitely_lost${NC}"
            fi
        else
            echo -e "${RED}Slave $i log not found${NC}"
        fi
    done
    
    echo -e "\n${CYAN}Stats Reader:${NC}"
    if [[ -f "$stats_log" ]]; then
        local error_summary=$(grep "ERROR SUMMARY" "$stats_log")
        echo -e "${BLUE}${error_summary:-No error summary}${NC}"
    else
        echo -e "${RED}Stats reader log not found${NC}"
    fi
}

# Function to analyze helgrind results
analyze_helgrind_results() {
    local master_log="$1"
    shift
    local stats_log="${@: -1}"
    local slave_logs=("${@:1:$(($#-1))}")
    
    echo -e "${CYAN}Thread Safety Analysis:${NC}"
    
    local total_errors=0
    
    # Check master
    if [[ -f "$master_log" ]]; then
        local master_errors=$(grep "ERROR SUMMARY" "$master_log" | grep -o '[0-9]\+' | head -1 || echo "0")
        total_errors=$((total_errors + ${master_errors:-0}))
        echo -e "${BLUE}Master: ${master_errors:-0} thread errors${NC}"
        
        # Look for specific race conditions
        local races=$(grep -c "Possible data race" "$master_log" 2>/dev/null || echo "0")
        if [[ $races -gt 0 ]]; then
            echo -e "${YELLOW}  ⚠  $races possible data races detected${NC}"
        fi
    fi
    
    # Check slaves
    for i in "${!slave_logs[@]}"; do
        local slave_log="${slave_logs[$i]}"
        if [[ -f "$slave_log" ]]; then
            local slave_errors=$(grep "ERROR SUMMARY" "$slave_log" | grep -o '[0-9]\+' | head -1 || echo "0")
            total_errors=$((total_errors + ${slave_errors:-0}))
            echo -e "${BLUE}Slave $i: ${slave_errors:-0} thread errors${NC}"
        fi
    done
    
    # Check stats reader
    if [[ -f "$stats_log" ]]; then
        local stats_errors=$(grep "ERROR SUMMARY" "$stats_log" | grep -o '[0-9]\+' | head -1 || echo "0")
        total_errors=$((total_errors + ${stats_errors:-0}))
        echo -e "${BLUE}Stats Reader: ${stats_errors:-0} thread errors${NC}"
    fi
    
    if [[ $total_errors -eq 0 ]]; then
        echo -e "${GREEN}✓ No thread safety issues detected${NC}"
    else
        echo -e "${RED}⚠  Total: $total_errors thread safety issues detected${NC}"
    fi
}

# Function to analyze DRD results
analyze_drd_results() {
    local master_log="$1"
    shift
    local stats_log="${@: -1}"
    local slave_logs=("${@:1:$(($#-1))}")
    
    echo -e "${CYAN}Data Race Detection Analysis:${NC}"
    
    local total_errors=0
    local total_races=0
    
    # Function to check a single log
    check_drd_log() {
        local log_file="$1"
        local process_name="$2"
        
        if [[ -f "$log_file" ]]; then
            local errors=$(grep "ERROR SUMMARY" "$log_file" | grep -o '[0-9]\+' | head -1)
            local races=$(grep -c "Data race" "$log_file" 2>/dev/null || echo "0")
            
            total_errors=$((total_errors + ${errors:-0}))
            total_races=$((total_races + races))
            
            echo -e "${BLUE}$process_name: ${errors:-0} errors, $races data races${NC}"
            
            if [[ $races -gt 0 ]]; then
                echo -e "${YELLOW}  ⚠  Data races detected - check log for details${NC}"
            fi
        else
            echo -e "${RED}$process_name log not found${NC}"
        fi
    }
    
    check_drd_log "$master_log" "Master"
    
    for i in "${!slave_logs[@]}"; do
        check_drd_log "${slave_logs[$i]}" "Slave $i"
    done
    
    check_drd_log "$stats_log" "Stats Reader"
    
    echo -e "\n${PURPLE}${BOLD}DRD Summary:${NC}"
    if [[ $total_errors -eq 0 && $total_races -eq 0 ]]; then
        echo -e "${GREEN}✓ No data races or synchronization issues detected${NC}"
    else
        echo -e "${RED}⚠  Total: $total_errors errors, $total_races data races detected${NC}"
        echo -e "${YELLOW}Review individual log files for detailed analysis${NC}"
    fi
}

# Function to show help
show_help() {
    echo -e "${BOLD}Valgrind IPC Test Suite for Master-Slave System${NC}"
    echo ""
    echo -e "This script runs comprehensive Valgrind analysis on the multi-process"
    echo -e "master-slave IPC system using memcheck, helgrind, and DRD tools."
    echo ""
    echo -e "${BOLD}Usage:${NC}"
    echo -e "  $0 [OPTIONS]"
    echo ""
    echo -e "${BOLD}Options:${NC}"
    echo -e "  -h, --help            Display this help message"
    echo -e "  -d, --duration TIME   Test duration in seconds (default: $TEST_DURATION)"
    echo -e "  -s, --slaves NUM      Number of slave processes (default: $NUM_SLAVES)"
    echo -e "  -m, --memcheck        Run only memcheck analysis"
    echo -e "  -t, --threads         Run only thread analysis (helgrind + DRD)"
    echo -e "  -a, --all             Run all tests (default)"
    echo ""
    echo -e "${BOLD}Tests Performed:${NC}"
    echo -e "  1. ${CYAN}Memcheck${NC}: Memory leak detection and error checking"
    echo -e "  2. ${CYAN}Helgrind${NC}: Thread synchronization error detection"  
    echo -e "  3. ${CYAN}DRD${NC}: Data race detection with enhanced sensitivity"
    echo ""
    echo -e "${BOLD}Output:${NC}"
    echo -e "  All logs are saved to the $LOGS_DIR/ directory"
    echo -e "  Each process (master/slaves/stats_reader) gets separate log files"
    echo ""
    echo -e "${BOLD}Prerequisites:${NC}"
    echo -e "  - Run 'make debug' to build binaries with debug symbols"
    echo -e "  - Valgrind must be installed on the system"
    echo ""
}

# Main execution
main() {
    # Parse command line arguments
    local run_memcheck=false
    local run_threads=false
    local run_all=true
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -d|--duration)
                TEST_DURATION="$2"
                shift 2
                ;;
            -s|--slaves)
                NUM_SLAVES="$2"
                shift 2
                ;;
            -m|--memcheck)
                run_memcheck=true
                run_all=false
                shift
                ;;
            -t|--threads)
                run_threads=true
                run_all=false
                shift
                ;;
            -a|--all)
                run_all=true
                shift
                ;;
            *)
                echo -e "${RED}Unknown option: $1${NC}"
                echo -e "Use --help to see available options."
                exit 1
                ;;
        esac
    done
    
    echo -e "${BOLD}${PURPLE}=========================================${NC}"
    echo -e "${BOLD}${PURPLE}  Valgrind IPC Analysis Suite${NC}"
    echo -e "${BOLD}${PURPLE}  Master-Slave pthread System${NC}"
    echo -e "${BOLD}${PURPLE}=========================================${NC}"
    
    echo -e "${BLUE}Configuration:${NC}"
    echo -e "  Test duration: ${TEST_DURATION}s"
    echo -e "  Number of slaves: ${NUM_SLAVES}"
    echo -e "  Logs directory: ${LOGS_DIR}"
    
    check_binaries
    
    # Initial cleanup
    cleanup_processes
    
    # Run selected tests
    if [[ "$run_all" == true ]]; then
        run_memcheck_test
        run_helgrind_test
        run_drd_test
    else
        if [[ "$run_memcheck" == true ]]; then
            run_memcheck_test
        fi
        if [[ "$run_threads" == true ]]; then
            run_helgrind_test
            run_drd_test
        fi
    fi
    
    # Final summary
    echo -e "\n${BOLD}${GREEN}=========================================${NC}"
    echo -e "${BOLD}${GREEN}  Analysis Complete${NC}"
    echo -e "${BOLD}${GREEN}=========================================${NC}"
    echo -e "${GREEN}All log files available in: ${LOGS_DIR}/${NC}"
    echo -e "${GREEN}Use individual log files for detailed debugging${NC}"
    
    # List generated files
    echo -e "\n${BLUE}Generated files:${NC}"
    find "$LOGS_DIR" -name "*.log" -type f | sort | while read -r file; do
        echo -e "  📄 $file"
    done
}

# Run main function with all arguments
main "$@"
