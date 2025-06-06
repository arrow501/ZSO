#!/bin/bash

# Simple valgrind test without fancy options

echo "Simple Valgrind Test"
echo "==================="

# Check valgrind exists
if ! command -v valgrind &> /dev/null; then
    echo "Valgrind not found"
    exit 1
fi

# Build debug version
echo "Building..."
make clean >/dev/null 2>&1
make debug >/dev/null 2>&1
echo "✅ Built"

# Very short test
export NUM_MESSAGES_PER_SLAVE=5

echo
echo "Test 1: Basic memcheck"
echo "---------------------"

timeout 20s valgrind --tool=memcheck --leak-check=full ./main 1 2>memcheck.out

echo "Exit code: $?"
echo "Memory summary:"
grep -E "(ERROR SUMMARY|definitely lost|indirectly lost)" memcheck.out

echo
echo "Test 2: Simple helgrind"  
echo "----------------------"

timeout 20s valgrind --tool=helgrind ./main 1 2>helgrind.out

echo "Exit code: $?"
echo "Race summary:"
grep -E "(ERROR SUMMARY|data race)" helgrind.out

echo
echo "Done. Check memcheck.out and helgrind.out for details"

make clean >/dev/null 2>&1