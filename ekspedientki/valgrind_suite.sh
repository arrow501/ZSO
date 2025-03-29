#!/bin/bash

# Enhanced Valgrind Analysis Suite
# 
# MIT Zero Clause License (ZCL)
# 
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# Define colors for better readability
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Default program path
PROGRAM="./bin/ekspedientki"

# Create logs directory if it doesn't exist
mkdir -p ./valgrind_logs

# Function to display help
show_help() {
    echo -e "${BOLD}Enhanced Valgrind Analysis Suite${NC}"
    echo ""
    echo -e "This script runs a comprehensive analysis of an executable using multiple Valgrind tools,"
    echo -e "saves the results to log files, and provides a detailed summary."
    echo ""
    echo -e "${BOLD}Usage:${NC}"
    echo -e "  $0 [OPTIONS]"
    echo ""
    echo -e "${BOLD}Options:${NC}"
    echo -e "  -h, --help            Display this help message"
    echo -e "  -p, --program PATH    Specify the path to the program to analyze (default: $PROGRAM)"
    echo ""
    echo -e "${BOLD}Tools Used:${NC}"
    echo -e "  - Memcheck: Memory error detector"
    echo -e "  - Helgrind: Thread execution error detector"
    echo -e "  - DRD: Thread synchronization error detector"
    echo -e "  - Callgrind: Function call profiler"
    echo -e "  - Cachegrind: Cache usage analyzer"
    echo -e "  - Massif: Heap memory usage profiler"
    echo ""
    echo -e "${BOLD}Output:${NC}"
    echo -e "  All logs and output files are saved to the ./valgrind_logs/ directory."
    echo -e "  A summary of findings is displayed after all tools have completed."
    echo ""
    echo -e "${BOLD}License:${NC}"
    echo -e "  MIT Zero Clause License (ZCL) - Free for any use."
    exit 0
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            ;;
        -p|--program)
            PROGRAM="$2"
            shift 2
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo -e "Use --help to see available options."
            exit 1
            ;;
    esac
done

# Check if the program exists
if [ ! -f "$PROGRAM" ]; then
    echo -e "${RED}Error: Program $PROGRAM not found!${NC}"
    exit 1
fi

echo -e "${BOLD}===============================================${NC}"
echo -e "${BOLD}Running Comprehensive Valgrind Suite on ${CYAN}$PROGRAM${NC}${BOLD}...${NC}"
echo -e "${BOLD}===============================================${NC}"

# Function to run a tool and save its output
run_tool() {
    local tool=$1
    local display_name=$2
    local options=$3
    local log_file="./valgrind_logs/${tool}.log"
    local output_file="./valgrind_logs/${tool}.out"
    
    echo -e "\n${YELLOW}Running ${BOLD}${display_name}${NC}${YELLOW}...${NC}"
    echo -e "${BLUE}Options: ${options}${NC}"
    echo -e "${BLUE}Log will be saved to: ${log_file}${NC}"
    
    # This captures both stdout and stderr to the log file
    valgrind --tool=$tool $options "$PROGRAM" > "$log_file" 2>&1
    
    echo -e "${GREEN}✓ ${display_name} analysis completed${NC}"
}

# Run Memcheck - memory error detector
run_tool "memcheck" "Memcheck" "--leak-check=full --show-leak-kinds=all --track-origins=yes --verbose"

# Run Helgrind - thread error detector
run_tool "helgrind" "Helgrind" "--history-level=full"

# Run DRD - thread synchronization detector
run_tool "drd" "DRD" "--check-stack-var=yes"

# Run Callgrind - call profiler
run_tool "callgrind" "Callgrind" "--cache-sim=yes --branch-sim=yes"

# Run Cachegrind - cache profiler
run_tool "cachegrind" "Cachegrind" "--branch-sim=yes"

# Run Massif - heap profiler (special handling for output)
echo -e "\n${YELLOW}Running ${BOLD}Massif${NC}${YELLOW}...${NC}"
echo -e "${BLUE}Options: --detailed-freq=10 --threshold=0.1${NC}"
echo -e "${BLUE}Output will be saved to: ./valgrind_logs/massif.out${NC}"

valgrind --tool=massif --detailed-freq=10 --threshold=0.1 \
    --massif-out-file="./valgrind_logs/massif.out" \
    "$PROGRAM" > "./valgrind_logs/massif_run.log" 2>&1

# Generate human-readable output from massif.out
ms_print "./valgrind_logs/massif.out" > "./valgrind_logs/massif.log"
echo -e "${GREEN}✓ Massif analysis completed${NC}"

# Move output files to logs directory
echo -e "\n${PURPLE}Moving output files to logs directory...${NC}"

# Function to safely move output files
safe_move_outputs() {
    local pattern=$1
    local destination=$2
    
    if ls $pattern >/dev/null 2>&1; then
        echo -e "${BLUE}Found ${pattern} files, moving most recent to ${destination}${NC}"
        mv -f $(ls -t $pattern | head -1) "$destination"
        
        # If there are more files matching the pattern, move them to a backup location
        remaining=$(ls $pattern 2>/dev/null)
        if [ -n "$remaining" ]; then
            echo -e "${BLUE}Moving remaining ${pattern} files to ./valgrind_logs/additional/${NC}"
            mkdir -p "./valgrind_logs/additional"
            mv $pattern "./valgrind_logs/additional/" 2>/dev/null
        fi
    else
        echo -e "${YELLOW}No ${pattern} files found${NC}"
    fi
}

# Move the output files
safe_move_outputs "callgrind.out.*" "./valgrind_logs/callgrind.out"
safe_move_outputs "cachegrind.out.*" "./valgrind_logs/cachegrind.out"

# Print a nice separator
echo -e "\n${BOLD}${CYAN}===============================================${NC}"
echo -e "${BOLD}${CYAN}              ANALYSIS SUMMARY                ${NC}"
echo -e "${BOLD}${CYAN}===============================================${NC}"

# Function to extract and print error summary for tools with ERROR SUMMARY
print_error_summary() {
    local tool=$1
    local display_name=$2
    local log_file="./valgrind_logs/${tool}.log"
    
    echo -e "\n${YELLOW}${BOLD}${display_name} Summary:${NC}"
    
    if [ -f "$log_file" ]; then
        local summary=$(grep "ERROR SUMMARY" "$log_file" | tail -1)
        if [ -n "$summary" ]; then
            echo -e "${BOLD}$summary${NC}"
        else
            echo -e "${RED}No error summary found in log${NC}"
        fi
        echo -e "${BLUE}📄 Detailed log: ${log_file}${NC}"
    else
        echo -e "${RED}Log file not found${NC}"
    fi
}

# Print summaries for tools with standard error summaries
print_error_summary "memcheck" "Memcheck"
print_error_summary "helgrind" "Helgrind"
print_error_summary "drd" "DRD"

# Function to extract and print performance summary for Callgrind and Cachegrind
print_performance_summary() {
    local tool=$1
    local display_name=$2
    local log_file="./valgrind_logs/${tool}.log"
    
    echo -e "\n${YELLOW}${BOLD}${display_name} Summary:${NC}"
    
    if [ -f "$log_file" ]; then
        # For Callgrind and Cachegrind, we need to look for specific patterns
        if [[ "$tool" == "callgrind" || "$tool" == "cachegrind" ]]; then
            # Try to find I refs (instruction references)
            local i_refs=$(grep "I\s*refs:" "$log_file" | tail -1)
            # Try to find D refs (data references)
            local d_refs=$(grep "D\s*refs:" "$log_file" | tail -1)
            # Try to find cache misses
            local cache_misses=$(grep "D1\s*misses:" "$log_file" | tail -1)
            # Try to find branch predictions
            local branch_pred=$(grep "Branches:" "$log_file" | tail -1)
            
            if [[ -n "$i_refs" || -n "$d_refs" || -n "$cache_misses" || -n "$branch_pred" ]]; then
                echo -e "${CYAN}Performance Statistics:${NC}"
                [[ -n "$i_refs" ]] && echo -e "${CYAN}$i_refs${NC}"
                [[ -n "$d_refs" ]] && echo -e "${CYAN}$d_refs${NC}"
                [[ -n "$cache_misses" ]] && echo -e "${CYAN}$cache_misses${NC}"
                [[ -n "$branch_pred" ]] && echo -e "${CYAN}$branch_pred${NC}"
            else
                # If no stats found in the log, try to parse the raw output file
                local out_file="./valgrind_logs/${tool}.out"
                if [ -f "$out_file" ]; then
                    echo -e "${CYAN}Raw performance data available in output file.${NC}"
                    echo -e "${CYAN}Use ${YELLOW}${tool}_annotate ${out_file}${CYAN} for detailed analysis.${NC}"
                else
                    echo -e "${RED}No performance stats found in log or output file${NC}"
                fi
            fi
        else
            # For other tools, use a more generic approach
            local stats=$(grep -E "summary:|Summary:" "$log_file" | tail -5)
            if [ -n "$stats" ]; then
                echo -e "${CYAN}${stats}${NC}"
            else
                echo -e "${RED}No performance stats found in log${NC}"
            fi
        fi
        
        echo -e "${BLUE}📄 Detailed log: ${log_file}${NC}"
        echo -e "${BLUE}📊 Raw data: ./valgrind_logs/${tool}.out${NC}"
    else
        echo -e "${RED}Log file not found${NC}"
    fi
}

# Print performance summaries
print_performance_summary "callgrind" "Callgrind"
print_performance_summary "cachegrind" "Cachegrind"

# Extract and print Massif summary
echo -e "\n${YELLOW}${BOLD}Massif Summary:${NC}"
if [ -f "./valgrind_logs/massif.log" ]; then
    # Extract peak memory usage
    peak=$(grep -A 1 "Peak heap usage" "./valgrind_logs/massif.log" | tail -1)
    echo -e "${PURPLE}Peak heap usage: ${peak}${NC}"
    
    # Extract last few memory snapshots to show trend
    echo -e "${CYAN}Memory usage trend (most recent snapshots):${NC}"
    grep -A 15 "Detailed snapshots" "./valgrind_logs/massif.log" | head -16 | sed 's/^/  /'
    
    echo -e "${BLUE}📄 Detailed log: ./valgrind_logs/massif.log${NC}"
    echo -e "${BLUE}📊 Raw data: ./valgrind_logs/massif.out${NC}"
else
    echo -e "${RED}Massif log file not found${NC}"
fi

# Final message
echo -e "\n${BOLD}${GREEN}===============================================${NC}"
echo -e "${BOLD}${GREEN} Valgrind analysis complete!${NC}"
echo -e "${BOLD}${GREEN} All logs available in ./valgrind_logs/ directory${NC}"
echo -e "${BOLD}${GREEN}===============================================${NC}"
