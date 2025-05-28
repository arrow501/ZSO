#!/bin/bash

# Simple Valgrind Test Suite for Master-Slave IPC System
# This script runs comprehensive tests using Valgrind tools to ensure memory and thread safety

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Configuration
LOGS_DIR="./valgrind_logs"
TEST_DURATION=8

# Binaries
MASTER_BIN="./master"
SLAVE_BIN="./slave"
STATS_BIN="./stats_reader"

# Create logs directory
mkdir -p "$LOGS_DIR"

# Cleanup function
cleanup() {
    # Prevent recursive cleanup
    if [[ "${CLEANUP_RUNNING:-}" == "true" ]]; then
        return 0
    fi
    export CLEANUP_RUNNING=true
    
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    # Be more specific to avoid killing this script
    pkill -f "./master" 2>/dev/null || true
    pkill -f "./slave" 2>/dev/null || true  
    pkill -f "./stats_reader" 2>/dev/null || true
    pkill -f "valgrind.*master" 2>/dev/null || true
    pkill -f "valgrind.*slave" 2>/dev/null || true
    pkill -f "valgrind.*stats_reader" 2>/dev/null || true
    sleep 1
    rm -f /tmp/master_fifo /tmp/slave_fifo_* 2>/dev/null || true
    rm -f /dev/shm/master_stats /dev/shm/sem.stats_ready 2>/dev/null || true
    echo -e "${GREEN}✓ Cleanup complete${NC}"
    
    export CLEANUP_RUNNING=false
}

# Set trap for cleanup on exit
trap cleanup EXIT

# Check if binaries exist
check_binaries() {
    for binary in "$MASTER_BIN" "$SLAVE_BIN" "$STATS_BIN"; do
        if [[ ! -x "$binary" ]]; then
            echo -e "${RED}Error: $binary not found${NC}"
            echo -e "${YELLOW}Run 'make' to build the binaries${NC}"
            exit 1
        fi
    done
    echo -e "${GREEN}✓ All binaries found${NC}"
}

# Test 1: Memory Leak Detection
test_memcheck() {
    echo -e "\n${BOLD}${CYAN}=== MEMORY LEAK TEST ===${NC}"
    sleep 1
    
    echo -e "${YELLOW}Starting master with memcheck...${NC}"
    valgrind --tool=memcheck \
             --leak-check=full \
             --show-leak-kinds=all \
             --log-file="$LOGS_DIR/master_memcheck.log" \
             "$MASTER_BIN" &
    
    sleep 3
    
    echo -e "${YELLOW}Starting 2 slaves with memcheck...${NC}"
    valgrind --tool=memcheck \
             --leak-check=full \
             --show-leak-kinds=all \
             --log-file="$LOGS_DIR/slave0_memcheck.log" \
             "$SLAVE_BIN" 0 &
    
    sleep 1
    
    valgrind --tool=memcheck \
             --leak-check=full \
             --show-leak-kinds=all \
             --log-file="$LOGS_DIR/slave1_memcheck.log" \
             "$SLAVE_BIN" 1 &
    
    sleep 2
    
    echo -e "${YELLOW}Starting stats reader with memcheck...${NC}"
    valgrind --tool=memcheck \
             --leak-check=full \
             --show-leak-kinds=all \
             --log-file="$LOGS_DIR/stats_memcheck.log" \
             "$STATS_BIN" &
    
    echo -e "${BLUE}Running for $TEST_DURATION seconds...${NC}"
    sleep $TEST_DURATION
    
    echo -e "${YELLOW}Stopping processes...${NC}"
    pkill -TERM -f "valgrind.*stats_reader" 2>/dev/null || true
    pkill -TERM -f "valgrind.*slave" 2>/dev/null || true
    sleep 2
    pkill -TERM -f "valgrind.*master" 2>/dev/null || true
    
    sleep 3
    echo -e "${GREEN}✓ Memcheck complete${NC}"
}

# Test 2: Thread Safety with Helgrind
test_helgrind() {
    echo -e "\n${BOLD}${CYAN}=== THREAD SAFETY TEST ===${NC}"
    sleep 1
    
    echo -e "${YELLOW}Starting master with helgrind...${NC}"
    valgrind --tool=helgrind \
             --log-file="$LOGS_DIR/master_helgrind.log" \
             "$MASTER_BIN" &
    
    sleep 3
    
    echo -e "${YELLOW}Starting 2 slaves with helgrind...${NC}"
    valgrind --tool=helgrind \
             --log-file="$LOGS_DIR/slave0_helgrind.log" \
             "$SLAVE_BIN" 0 &
    
    sleep 1
    
    valgrind --tool=helgrind \
             --log-file="$LOGS_DIR/slave1_helgrind.log" \
             "$SLAVE_BIN" 1 &
    
    sleep 2
    
    echo -e "${YELLOW}Starting stats reader with helgrind...${NC}"
    valgrind --tool=helgrind \
             --log-file="$LOGS_DIR/stats_helgrind.log" \
             "$STATS_BIN" &
    
    echo -e "${BLUE}Running concurrent operations for $TEST_DURATION seconds...${NC}"
    
    # Generate some concurrent activity
    for i in {1..3}; do
        sleep $(($TEST_DURATION / 3))
        echo -e "${BLUE}Triggering stats update $i/3...${NC}"
        pkill -USR1 -f "valgrind.*master" 2>/dev/null || true
    done
    
    echo -e "${YELLOW}Stopping processes...${NC}"
    pkill -TERM -f "valgrind.*stats_reader" 2>/dev/null || true
    pkill -TERM -f "valgrind.*slave" 2>/dev/null || true
    sleep 2
    pkill -TERM -f "valgrind.*master" 2>/dev/null || true
    
    sleep 3
    echo -e "${GREEN}✓ Helgrind complete${NC}"
}

# Test 3: Data Race Detection with DRD
test_drd() {
    echo -e "\n${BOLD}${CYAN}=== DATA RACE TEST ===${NC}"
    sleep 1
    
    echo -e "${YELLOW}Starting master with DRD...${NC}"
    valgrind --tool=drd \
             --log-file="$LOGS_DIR/master_drd.log" \
             "$MASTER_BIN" &
    
    sleep 3
    
    echo -e "${YELLOW}Starting 2 slaves with DRD...${NC}"
    valgrind --tool=drd \
             --log-file="$LOGS_DIR/slave0_drd.log" \
             "$SLAVE_BIN" 0 &
    
    sleep 1
    
    valgrind --tool=drd \
             --log-file="$LOGS_DIR/slave1_drd.log" \
             "$SLAVE_BIN" 1 &
    
    sleep 2
    
    echo -e "${YELLOW}Starting stats reader with DRD...${NC}"
    valgrind --tool=drd \
             --log-file="$LOGS_DIR/stats_drd.log" \
             "$STATS_BIN" &
    
    echo -e "${BLUE}Running stress test for $TEST_DURATION seconds...${NC}"
    
    # More aggressive testing for race conditions
    for i in {1..4}; do
        sleep $(($TEST_DURATION / 4))
        echo -e "${BLUE}Stress burst $i/4...${NC}"
        # Rapid stats requests
        pkill -USR1 -f "valgrind.*master" 2>/dev/null || true
        sleep 0.1
        pkill -USR1 -f "valgrind.*master" 2>/dev/null || true
    done
    
    echo -e "${YELLOW}Stopping processes...${NC}"
    pkill -TERM -f "valgrind.*stats_reader" 2>/dev/null || true
    pkill -TERM -f "valgrind.*slave" 2>/dev/null || true
    sleep 2
    pkill -TERM -f "valgrind.*master" 2>/dev/null || true
    
    sleep 3
    echo -e "${GREEN}✓ DRD complete${NC}"
}

# Simple result reporting function
show_results() {
    echo -e "\n${BOLD}${CYAN}========================================${NC}"
    echo -e "${BOLD}${CYAN}  VALGRIND RESULTS SUMMARY${NC}"
    echo -e "${BOLD}${CYAN}========================================${NC}"
    
    # Memory check results
    echo -e "\n${BOLD}${YELLOW}Memory Check (memcheck):${NC}"
    for process in "master" "slave0" "slave1" "stats"; do
        local log="$LOGS_DIR/${process}_memcheck.log"
        if [[ -f "$log" ]]; then
            local error_line=$(tail -n 10 "$log" | grep "ERROR SUMMARY" | tail -n 1)
            if [[ -n "$error_line" ]]; then
                echo -e "  ${CYAN}${process}:${NC} $error_line"
            else
                echo -e "  ${CYAN}${process}:${NC} No error summary found"
            fi
        else
            echo -e "  ${CYAN}${process}:${NC} Log file not found"
        fi
    done
    
    # Thread safety results
    echo -e "\n${BOLD}${YELLOW}Thread Safety (helgrind):${NC}"
    for process in "master" "slave0" "slave1" "stats"; do
        local log="$LOGS_DIR/${process}_helgrind.log"
        if [[ -f "$log" ]]; then
            local error_line=$(tail -n 10 "$log" | grep "ERROR SUMMARY" | tail -n 1)
            if [[ -n "$error_line" ]]; then
                echo -e "  ${CYAN}${process}:${NC} $error_line"
            else
                echo -e "  ${CYAN}${process}:${NC} No error summary found"
            fi
        else
            echo -e "  ${CYAN}${process}:${NC} Log file not found"
        fi
    done
    
    # Data race results
    echo -e "\n${BOLD}${YELLOW}Data Race Detection (DRD):${NC}"
    for process in "master" "slave0" "slave1" "stats"; do
        local log="$LOGS_DIR/${process}_drd.log"
        if [[ -f "$log" ]]; then
            local error_line=$(tail -n 10 "$log" | grep "ERROR SUMMARY" | tail -n 1)
            if [[ -n "$error_line" ]]; then
                echo -e "  ${CYAN}${process}:${NC} $error_line"
            else
                echo -e "  ${CYAN}${process}:${NC} No error summary found"
            fi
        else
            echo -e "  ${CYAN}${process}:${NC} Log file not found"
        fi
    done
    
    echo -e "\n${GREEN}All logs saved to: $LOGS_DIR/${NC}"
}

# Main execution
main() {
    echo -e "${BOLD}${CYAN}========================================${NC}"
    echo -e "${BOLD}${CYAN}  Simple Valgrind Test Suite${NC}"
    echo -e "${BOLD}${CYAN}  Master-Slave IPC System${NC}"
    echo -e "${BOLD}${CYAN}========================================${NC}"
    
    check_binaries
    
    # Run all tests
    test_memcheck
    test_helgrind
    test_drd
    
    # Show all results at the end
    show_results
    
    # List generated files
    echo -e "\n${BLUE}Generated files:${NC}"
    find "$LOGS_DIR" -name "*.log" -type f | sort | while read -r file; do
        echo -e "  📄 $file"
    done
    
    echo -e "\n${BOLD}${GREEN}Testing Complete!${NC}"
}

# Show help if requested
if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    echo -e "${BOLD}Simple Valgrind Test Suite${NC}"
    echo ""
    echo -e "Usage: $0"
    echo ""
    echo -e "Runs three comprehensive tests:"
    echo -e "  1. ${CYAN}Memory Leak Detection${NC} (memcheck)"
    echo -e "  2. ${CYAN}Thread Safety${NC} (helgrind)"
    echo -e "  3. ${CYAN}Data Race Detection${NC} (DRD)"
    echo ""
    echo -e "All tests use hardcoded parameters optimized for this project."
    echo -e "Logs are saved to $LOGS_DIR/"
    echo ""
    exit 0
fi

# Run the tests
main
