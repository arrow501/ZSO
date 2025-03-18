#!/bin/sh
echo "Compiling without any print or assert statements"
if make release; then
    echo "Done compiling! Run ./bin/ekspedientki to run the program"
    exit 0
else
    echo "Compilation failed!"
    exit 1
fi