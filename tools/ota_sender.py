#!/usr/bin/env python3
"""
OTA Firmware Sender for STM32-oop Bootloader.

Sends firmware binary to STM32F103 over UART using the OTA protocol.
Communicates with either the bootloader or the application-side OTA client.

Protocol frames (little-endian):
    ┌──────┬────────┬──────┬──────┬──────────┬────────┐
    │ STX  │  LEN   │ TYPE │ SEQ  │ PAYLOAD  │ CRC16  │
    │ 0xAA │ uint16 │ uint8│ uint8│ 0..256B  │ uint16 │
    └──────┴────────┴──────┴──────┴──────────┴────────┘

Usage:
    python3 ota_sender.py firmware.bin --port /dev/tty.usbserial-*

Requirements:
    pip install pyserial
"""

import argparse
import struct
import sys
import time
from pathlib import Path

try:
    import serial
except ImportError:
    print("Error: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

# ═══════════════════════════════════════════════════════════════
#  Protocol Constants (must match ota_config.h)
# ═══════════════════════════════════════════════════════════════

STX = 0xAA
MAX_PAYLOAD = 256
FRAME_OVERHEAD = 6  # STX(1) + LEN(2) + TYPE(1) + SEQ(1) + CRC(2)
ACK_TIMEOUT = 0.5   # seconds
MAX_RETRIES = 3

# Commands
CMD_HELLO     = 0x01
CMD_HELLO_ACK = 0x02
CMD_DATA      = 0x03
CMD_DATA_ACK  = 0x04
CMD_VERIFY    = 0x05
CMD_VERIFY_ACK= 0x06
CMD_BOOT      = 0x07
CMD_BOOT_ACK  = 0x08
CMD_ERROR     = 0x0E
CMD_RESET     = 0x0F

# Error codes
ERR_NONE         = 0
ERR_CRC          = 1
ERR_FLASH_ERASE  = 2
ERR_FLASH_WRITE  = 3
ERR_FLASH_VERIFY = 4
ERR_TIMEOUT      = 5
ERR_SEQUENCE     = 6
ERR_SIZE         = 7

ERROR_NAMES = {
    0: "None",
    1: "CRC Error",
    2: "Flash Erase Error",
    3: "Flash Write Error",
    4: "Flash Verify Error",
    5: "Timeout",
    6: "Sequence Error",
    7: "Size Too Large",
}

# ═══════════════════════════════════════════════════════════════
#  CRC16-CCITT (matches ota_protocol.c — polynomial 0x1021)
# ═══════════════════════════════════════════════════════════════

def _make_crc16_table():
    table = []
    for i in range(256):
        crc = i << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if (crc & 0x8000) else (crc << 1)
        table.append(crc & 0xFFFF)
    return table

CRC16_TABLE = _make_crc16_table()

def crc16(data: bytes) -> int:
    """Compute CRC16-CCITT (matching ota_crc16 in C)."""
    crc = 0xFFFF
    for byte in data:
        crc = ((crc << 8) ^ CRC16_TABLE[((crc >> 8) ^ byte) & 0xFF]) & 0xFFFF
    return crc

# ═══════════════════════════════════════════════════════════════
#  Frame Encoding / Decoding
# ═══════════════════════════════════════════════════════════════

def encode_frame(cmd: int, seq: int, payload: bytes = b"") -> bytes:
    """Encode an OTA protocol frame."""
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"Payload too large: {len(payload)} > {MAX_PAYLOAD}")

    inner = struct.pack("<BB", cmd, seq) + payload
    crc = crc16(inner)
    length = len(inner) + 2  # TYPE+SEQ+PAYLOAD+CRC (not STX+LEN)
    return struct.pack("<BHB", STX, length, cmd) + struct.pack("<B", seq) + payload + struct.pack("<H", crc)


def decode_frame(data: bytes) -> tuple[int, int, bytes] | None:
    """Decode an OTA protocol frame. Returns (cmd, seq, payload) or None."""
    if len(data) < FRAME_OVERHEAD:
        return None
    if data[0] != STX:
        return None

    length = struct.unpack_from("<H", data, 1)[0]
    if length < 4:  # TYPE(1) + SEQ(1) + CRC(2) minimum
        return None
    if length + 3 > len(data):  # +3 for STX + LEN(2)
        return None

    cmd, seq = struct.unpack_from("<BB", data, 3)
    payload_len = length - 4  # minus TYPE + SEQ + CRC
    payload = data[5:5 + payload_len]
    received_crc = struct.unpack_from("<H", data, 5 + payload_len)[0]

    inner = data[3:5 + payload_len]
    computed_crc = crc16(inner)
    if received_crc != computed_crc:
        return None

    return (cmd, seq, payload)


# ═══════════════════════════════════════════════════════════════
#  OTA Sender
# ═══════════════════════════════════════════════════════════════

class OtaSender:
    """Handles the OTA send protocol over a serial port."""

    def __init__(self, port: str, baudrate: int = 115200):
        self.ser = serial.Serial(port, baudrate, timeout=ACK_TIMEOUT)
        self.seq = 0
        self.verbose = False

    def close(self):
        self.ser.close()

    def _send_frame(self, cmd: int, payload: bytes = b"") -> bytes:
        """Send a frame and return the raw response."""
        frame = encode_frame(cmd, self.seq, payload)
        self.ser.write(frame)
        self.ser.flush()
        if self.verbose:
            print(f"  → TX: cmd=0x{cmd:02X} seq={self.seq} len={len(frame)}")

        # Read response frame
        time.sleep(0.05)  # give MCU time to process
        response = self.ser.read(FRAME_OVERHEAD + MAX_PAYLOAD)
        if not response:
            raise TimeoutError(f"No response for cmd=0x{cmd:02X}")
        return response

    def _expect_ack(self, expected_cmd: int, cmd_name: str) -> bool:
        """Send a command and expect its ACK. Returns True on success."""
        for retry in range(MAX_RETRIES):
            try:
                response = self._send_frame(expected_cmd)
                result = decode_frame(response)
                if result:
                    cmd, seq, payload = result
                    expected_ack = expected_cmd + 1  # ACK is cmd+1
                    if cmd == expected_ack:
                        self.seq = (self.seq + 1) & 0xFF
                        return True
                    elif cmd == CMD_ERROR:
                        err_code = payload[0] if payload else 0xFF
                        err_name = ERROR_NAMES.get(err_code, f"Unknown({err_code})")
                        print(f"  ✘ Device error: {err_name}")
                        return False
                    else:
                        print(f"  ✘ Unexpected response: cmd=0x{cmd:02X}")
                else:
                    print(f"  ✘ Invalid frame (CRC fail or format error)")
            except TimeoutError:
                print(f"  ⚠ Timeout, retry {retry + 1}/{MAX_RETRIES}...")
                time.sleep(0.2)

            self.seq = (self.seq + 1) & 0xFF
        return False

    def handshake(self, firmware_size: int) -> bool:
        """Perform HELLO handshake, exchange capabilities."""
        print(f"Handshake: firmware={firmware_size} bytes...")
        payload = struct.pack("<BBBH", 1, MAX_PAYLOAD & 0xFF, (MAX_PAYLOAD >> 8) & 0xFF, firmware_size)
        # The protocol expects HELLO payload: version(1) + max_payload(2) + app_size(1 low)
        hello_payload = struct.pack("<BBH", 1, MAX_PAYLOAD & 0xFF, firmware_size & 0xFFFF)
        response = self._send_frame(CMD_HELLO, hello_payload)
        result = decode_frame(response)
        if result:
            cmd, seq, payload = result
            if cmd == CMD_HELLO_ACK and len(payload) >= 4:
                remote_max_size = struct.unpack_from("<I", payload, 0)[0]
                print(f"  Device ready. Max app size: {remote_max_size} bytes")
                self.seq = (seq + 1) & 0xFF
                if firmware_size > remote_max_size:
                    print(f"  ✘ Firmware ({firmware_size}B) exceeds device max ({remote_max_size}B)")
                    return False
                return True
            elif cmd == CMD_ERROR:
                err = payload[0] if payload else 0xFF
                print(f"  ✘ Device error: {ERROR_NAMES.get(err, f'Unknown({err})')}")
                return False
        print("  ✘ Handshake failed")
        return False

    def send_firmware(self, firmware: bytes, show_progress: bool = True) -> bool:
        """Send firmware in DATA frames."""
        total = len(firmware)
        sent = 0
        errors = 0
        last_progress = -1

        print(f"Sending {total} bytes "
              f"({total // 1024} KB) in {(total + MAX_PAYLOAD - 1) // MAX_PAYLOAD} frames...")

        while sent < total:
            chunk_size = min(MAX_PAYLOAD, total - sent)
            chunk = firmware[sent:sent + chunk_size]

            # Send DATA frame
            frame = encode_frame(CMD_DATA, self.seq, chunk)
            self.ser.write(frame)
            self.ser.flush()

            # Wait for ACK
            response = self.ser.read(FRAME_OVERHEAD + MAX_PAYLOAD)
            result = decode_frame(response)

            if result and result[0] == CMD_DATA_ACK and result[1] == self.seq:
                sent += chunk_size
                self.seq = (self.seq + 1) & 0xFF
                errors = 0

                # Progress
                if show_progress:
                    pct = (sent * 100) // total
                    if pct != last_progress:
                        bar = "█" * (pct // 2) + "░" * (50 - pct // 2)
                        print(f"\r  [{bar}] {pct:3d}%  {sent}/{total} bytes", end="", flush=True)
                        last_progress = pct
            elif result and result[0] == CMD_ERROR:
                err = result[2][0] if result[2] else 0xFF
                errors += 1
                if errors >= MAX_RETRIES:
                    print(f"\n  ✘ Too many errors (last: {ERROR_NAMES.get(err, f'Unknown({err})')})")
                    return False
                self.seq = (self.seq + 1) & 0xFF
            else:
                errors += 1
                if errors >= MAX_RETRIES:
                    print(f"\n  ✘ Too many communication errors")
                    return False

        if show_progress:
            print()  # newline after progress bar
        print(f"  ✓ Sent {sent} bytes")
        return True

    def verify(self, firmware_size: int, expected_crc32: int) -> bool:
        """Send VERIFY command and check CRC32."""
        print(f"Verifying firmware (CRC32=0x{expected_crc32:08X})...")
        response = self._send_frame(CMD_VERIFY, struct.pack("<II", firmware_size, expected_crc32))
        result = decode_frame(response)
        if result and result[0] == CMD_VERIFY_ACK:
            print("  ✓ Verification passed")
            self.seq = (self.seq + 1) & 0xFF
            return True
        print("  ✘ Verification failed")
        return False

    def boot(self) -> bool:
        """Send BOOT command to trigger firmware switch."""
        print("Sending BOOT command...")
        try:
            response = self._send_frame(CMD_BOOT)
            result = decode_frame(response)
            if result and result[0] == CMD_BOOT_ACK:
                print("  ✓ Device will now reset into new firmware")
                return True
        except TimeoutError:
            # BOOT may not ACK if device resets immediately
            print("  ✓ BOOT sent (device resetting)")
            return True
        return False


# ═══════════════════════════════════════════════════════════════
#  CRC32 for firmware verification (matches ota_flash_crc32)
# ═══════════════════════════════════════════════════════════════

def crc32(data: bytes) -> int:
    """Compute CRC32 matching ota_flash_crc32."""
    crc = 0xFFFFFFFF
    for byte in data:
        crc = (crc >> 8) ^ _CRC32_TABLE[(crc ^ byte) & 0xFF]
    return (crc ^ 0xFFFFFFFF) & 0xFFFFFFFF

def _make_crc32_table():
    table = []
    for i in range(256):
        crc = i
        for _ in range(8):
            crc = (crc >> 1) ^ 0xEDB88320 if (crc & 1) else (crc >> 1)
        table.append(crc & 0xFFFFFFFF)
    return table

_CRC32_TABLE = _make_crc32_table()


# ═══════════════════════════════════════════════════════════════
#  CLI
# ═══════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(
        description="OTA Firmware Sender for STM32-oop Bootloader",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python3 ota_sender.py firmware.bin --port /dev/tty.usbserial-0001
  python3 ota_sender.py firmware.bin --port COM3 --baud 115200
  python3 ota_sender.py firmware.bin --port /dev/ttyUSB0 --no-verify
  python3 ota_sender.py firmware.bin --port /dev/ttyUSB0 --verbose
        """
    )
    parser.add_argument("firmware", help="Firmware binary file (.bin)")
    parser.add_argument("--port", "-p", required=True, help="Serial port device")
    parser.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--no-verify", action="store_true", help="Skip CRC32 verification step")
    parser.add_argument("--no-boot", action="store_true", help="Don't send BOOT command after transfer")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose frame-level logging")
    parser.add_argument("--retries", type=int, default=MAX_RETRIES, help=f"Max retries (default: {MAX_RETRIES})")
    args = parser.parse_args()

    # Read firmware
    fw_path = Path(args.firmware)
    if not fw_path.exists():
        print(f"Error: Firmware file not found: {args.firmware}")
        sys.exit(1)

    firmware = fw_path.read_bytes()
    fw_size = len(firmware)
    fw_crc32 = crc32(firmware)

    print(f"╔══════════════════════════════════════════════╗")
    print(f"║   STM32-oop OTA Firmware Sender             ║")
    print(f"╠══════════════════════════════════════════════╣")
    print(f"║  Firmware: {fw_path.name:<32s} ║")
    print(f"║  Size:     {fw_size:>6} bytes ({fw_size/1024:.1f} KB)          ║")
    print(f"║  CRC32:    0x{fw_crc32:08X}                      ║")
    print(f"║  Port:     {args.port:<32s} ║")
    print(f"║  Baud:     {args.baud}                              ║")
    print(f"╚══════════════════════════════════════════════╝")
    print()

    # Connect
    sender = OtaSender(args.port, args.baud)
    sender.verbose = args.verbose
    print(f"Connected to {args.port} @ {args.baud} baud")

    try:
        # 1. Handshake
        if not sender.handshake(fw_size):
            print("\n✘ OTA failed at handshake phase")
            sys.exit(1)
        print()

        # 2. Send firmware
        if not sender.send_firmware(firmware):
            print("\n✘ OTA failed during transfer")
            sys.exit(1)
        print()

        # 3. Verify
        if not args.no_verify:
            if not sender.verify(fw_size, fw_crc32):
                print("\n✘ OTA failed at verification")
                sys.exit(1)
            print()

        # 4. Boot
        if not args.no_boot:
            sender.boot()
            print()

        print("╔══════════════════════════════════════════════╗")
        print("║   ✓ OTA Update Complete!                     ║")
        print("╚══════════════════════════════════════════════╝")

    except KeyboardInterrupt:
        print("\n\n✘ Cancelled by user")
        sys.exit(1)
    except serial.SerialException as e:
        print(f"\n✘ Serial error: {e}")
        sys.exit(1)
    finally:
        sender.close()


if __name__ == "__main__":
    main()
