#!/bin/bash
# Debug STM32F103C8T6 via ST-Link / OpenOCD + GDB.
# Usage: ./scripts/gdb.sh

set -e

echo "Starting OpenOCD in background..."
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg &
OPENOCD_PID=$!

# Wait for OpenOCD to be ready
sleep 2

echo "Starting GDB..."
arm-none-eabi-gdb build/target/stm32-oop.elf \
    -ex "target extended-remote localhost:3333" \
    -ex "monitor reset init" \
    -ex "break main" \
    -ex "continue"

# Cleanup
kill $OPENOCD_PID 2>/dev/null || true
