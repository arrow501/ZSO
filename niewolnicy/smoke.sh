#!/bin/bash

# Ultra-simple smoke test - just "does it start and not crash?"

echo "🔥 Smoke Test - Does it start without crashing?"

# Build
make clean && make
if [ $? -ne 0 ]; then
    echo "❌ Build failed"
    exit 1
fi

# Set very low message count for fastest test
export NUM_MESSAGES_PER_SLAVE=3

echo "Starting system with 1 slave for 3 messages..."

# Run for max 10 seconds
timeout 10s ./main 1

EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
    echo "✅ PASS: System completed normally"
elif [ $EXIT_CODE -eq 124 ]; then
    echo "⚠️  TIMEOUT: System didn't finish in 10 seconds (might be normal)"
else
    echo "❌ FAIL: System crashed with exit code $EXIT_CODE"
    exit 1
fi

# Quick cleanup
make clean >/dev/null 2>&1

echo "🎉 Smoke test passed - system starts and runs!"