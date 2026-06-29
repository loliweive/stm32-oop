#!/usr/bin/env bash
# Serial monitor using macOS built-in tools (screen or stty+cat)
# Usage: ./scripts/serial_monitor.sh [duration_sec] [baud] [device]
set -euo pipefail
DURATION="${1:-5}"
BAUD="${2:-115200}"
DEVICE="${3:-/dev/cu.usbserial-3130}"
LOGFILE="build/logs/serial_$(date +%Y%m%d_%H%M%S).log"
mkdir -p build/logs

echo "=== Serial monitor: ${DEVICE} @ ${BAUD} for ${DURATION}s ==="

# Use macOS built-in 'screen' to capture serial output
# screen -L logs to screenlog.0 by default, so redirect instead
stty -f "${DEVICE}" "${BAUD}" cs8 -parenb -cstopb 2>/dev/null || true
screen -d -m "${DEVICE}" "${BAUD}" 2>/dev/null || true
sleep 0.5
timeout_cmd=""
if command -v gtimeout &>/dev/null; then
    timeout_cmd="gtimeout ${DURATION} cat ${DEVICE}"
else
    # macOS fallback: run cat in background, kill after duration
    timeout_cmd="cat ${DEVICE}"
fi
$timeout_cmd > "${LOGFILE}" 2>&1 &
PID=$!
sleep "${DURATION}"
kill $PID 2>/dev/null || true
wait $PID 2>/dev/null || true
echo "=== Log: ${LOGFILE} ==="
