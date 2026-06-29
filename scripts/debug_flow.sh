#!/usr/bin/env bash
# Build → Flash → Monitor — AI debug loop (macOS built-in tools)
# Usage: ./scripts/debug_flow.sh [freertos|stm32f103]
set -euo pipefail
MODE="${1:-freertos}"
DEVICE="${2:-/dev/cu.usbserial-3130}"
BAUD="115200"
PROJ="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${PROJ}/build/${MODE}"
LOGDIR="${PROJ}/build/logs"
mkdir -p "${LOGDIR}"
LOGFILE="${LOGDIR}/debug_$(date +%Y%m%d_%H%M%S).log"

echo "╔══════════════════════════════════════════════╗"
echo "║  STM32-oop Debug Flow — ${MODE}               ║"
echo "╚══════════════════════════════════════════════╝"

echo ""
echo "── [1/3] Building ${MODE} ──────────────────────────"
cd "${PROJ}"
cmake -B "${BUILD_DIR}" -DBUILD_MODE="${MODE}" > /dev/null 2>&1
cmake --build "${BUILD_DIR}" 2>&1 | grep -E "error|Built target|text" | tail -3 || true
SIZE=$(arm-none-eabi-size "${BUILD_DIR}/stm32-oop" 2>/dev/null | tail -1)
echo "     Size: ${SIZE}"

echo ""
echo "── [2/3] Flashing via OpenOCD ─────────────────────"
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
    -c "program ${BUILD_DIR}/stm32-oop verify reset exit" 2>&1 | grep -iE "verified|error" || echo "     (done)"
echo "     Done."

echo ""
echo "── [3/3] Serial output (10s capture, macOS built-in) ──"
echo "     Device: ${DEVICE} @ ${BAUD}"
echo "     Log:    ${LOGFILE}"
stty -f "${DEVICE}" "${BAUD}" cs8 -parenb -cstopb 2>/dev/null || true
cat "${DEVICE}" > "${LOGFILE}" 2>&1 &
PID=$!
sleep 10
kill $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

# Show captured output
if [ -s "${LOGFILE}" ]; then
    echo ""
    cat "${LOGFILE}" | head -60
else
    echo ""
    echo "  ⚠️  No serial output. Check:"
    echo "     - Adapter RX → MCU TX (PA9), Adapter TX → MCU RX (PA10)"
    echo "     - GND connected"
    echo "     - BOOT0 = LOW (not bootloader mode)"
fi

echo ""
echo "── Done ──────────────────────────────────────────"
echo "  Log: ${LOGFILE}"
