#!/bin/sh
echo "Compiling with print and asserts enabled"
if make debug; then
    echo "Done compiling! Run ./bin/ekspedientki to run the program"
    exit 0
else
    echo "Compilation failed!"
    exit 1
fi