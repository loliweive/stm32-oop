#!/bin/bash
# Flash STM32F103C8T6 via ST-Link / OpenOCD.
# Usage: ./scripts/flash.sh

set -e

ELF="build/target/stm32-oop.elf"

if [ ! -f "$ELF" ]; then
    echo "No firmware found at $ELF"
    echo "Run: cmake -B build/target -DTARGET=stm32f103 && cmake --build build/target"
    exit 1
fi

echo "Flashing $ELF ..."
openocd \
    -f interface/stlink.cfg \
    -f target/stm32f1x.cfg \
    -c "program $ELF verify reset exit"

echo "Done. Board should be running."
