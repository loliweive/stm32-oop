#!/usr/bin/env python3
"""
Firmware Signing Tool — HMAC-SHA256

Appends HMAC signature to firmware binary. The bootloader verifies this
signature before committing an OTA update to internal Flash.

Usage:
    # Sign with default dev key
    python3 tools/sign.py build/app/stm32-oop.bin

    # Sign with custom key
    python3 tools/sign.py build/app/stm32-oop.bin --key my_secret.key

    # Generate a random key
    python3 tools/sign.py --gen-key dev.key

Signed firmware format:
    [original firmware.bin] [4 bytes: original_size LE] [32 bytes: HMAC-SHA256]

The bootloader reads original_size to know how much firmware to commit,
then verifies the HMAC over firmware.bin[0..original_size].
"""

import argparse
import hashlib
import hmac
import os
import struct
import sys

# Default dev key (DO NOT use in production!)
DEFAULT_KEY = b"STM32F103-OTA-DEV-KEY-01234567"  # 32 bytes

HMAC_SIZE = 32
SIZE_FIELD = 4   # uint32 little-endian
SIG_APPEND = SIZE_FIELD + HMAC_SIZE  # 36 bytes appended


def sign_firmware(fw_path, key, output_path=None):
    """Sign firmware binary and write to output."""
    with open(fw_path, "rb") as f:
        firmware = f.read()

    fw_size = len(firmware)
    print(f"  Firmware: {fw_path} ({fw_size} bytes, {fw_size/1024:.1f} KB)")

    # HMAC-SHA256 over the firmware data
    h = hmac.new(key, firmware, hashlib.sha256)
    mac = h.digest()
    print(f"  HMAC:     {mac.hex()}")

    # Append: [4 bytes size LE] [32 bytes HMAC]
    output = output_path or fw_path
    with open(output, "wb") as f:
        f.write(firmware)
        f.write(struct.pack("<I", fw_size))
        f.write(mac)

    total = len(firmware) + SIG_APPEND
    print(f"  Signed:   {output} ({total} bytes)")
    return output


def generate_key(key_path: str):
    """Generate random 32-byte key."""
    key = os.urandom(32)
    with open(key_path, "wb") as f:
        f.write(key)
    print(f"Generated key: {key_path} ({key.hex()})")
    print("Keep this file secret! Lost key = cannot verify OTA updates.")


def main():
    parser = argparse.ArgumentParser(description="Firmware HMAC-SHA256 signing tool")
    parser.add_argument("firmware", nargs="?", help="Firmware .bin file to sign")
    parser.add_argument("--key", default=None, help="Key file (32 bytes, default: built-in dev key)")
    parser.add_argument("--gen-key", metavar="PATH", help="Generate a random key file")
    parser.add_argument("-o", "--output", default=None, help="Output file (default: overwrite input)")
    args = parser.parse_args()

    if args.gen_key:
        generate_key(args.gen_key)
        return

    if not args.firmware:
        parser.print_help()
        sys.exit(1)

    # Load key
    if args.key:
        with open(args.key, "rb") as f:
            key = f.read()
        if len(key) < 32:
            # Pad or hash to 32 bytes
            key = hashlib.sha256(key).digest()
        else:
            key = key[:32]
    else:
        key = DEFAULT_KEY
        print(f"  ⚠ Using built-in dev key (32 bytes)")
        print(f"    For production: python3 tools/sign.py --gen-key secret.key")

    sign_firmware(args.firmware, key, args.output)


if __name__ == "__main__":
    main()
