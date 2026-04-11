#!/usr/bin/env python3
"""
EEPROM Hex Reader - Read and display EEPROM contents in hex format
Part of Loki's EEPROM management toolkit for NFC/RFID credit systems
"""

import sys
import os
import struct
import argparse
from pathlib import Path

# EEPROM I2C address and constants
EEPROM_I2C_ADDRESS = 0x50
EEPROM_SIZE = 256  # FT24C02A is 256 bytes
EEPROM_DEVICE_PATH = "/dev/i2c-0"

def read_eeprom_via_i2c(address=0, length=EEPROM_SIZE):
    """
    Read EEPROM contents via I2C interface
    """
    try:
        import smbus2
    except ImportError:
        print("[ERROR] smbus2 not installed. Install with: pip3 install smbus2", file=sys.stderr)
        return None
    
    try:
        bus = smbus2.SMBus(0)  # I2C bus 0
        data = bytearray()
        
        # Read in 32-byte chunks to avoid timeout
        chunk_size = 32
        for offset in range(address, address + length, chunk_size):
            read_len = min(chunk_size, address + length - offset)
            chunk = bus.read_i2c_block_data(EEPROM_I2C_ADDRESS, offset, read_len)
            data.extend(chunk)
        
        bus.close()
        return data
    except Exception as e:
        print(f"[ERROR] Failed to read EEPROM: {e}", file=sys.stderr)
        return None

def format_hex_dump(data, start_address=0):
    """
    Format data as hexdump (16 bytes per line)
    """
    output = []
    output.append("Offset  | Hex                                            | ASCII")
    output.append("-       | -                                              | -")
    
    for offset in range(0, len(data), 16):
        chunk = data[offset:offset+16]
        hex_str = " ".join(f"{b:02x}" for b in chunk)
        ascii_str = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        output.append(f"{start_address + offset:04x}    | {hex_str:<48} | {ascii_str}")
    
    return "\n".join(output)

def read_credit_value(data, offset=0):
    """
    Read 32-bit credit value from EEPROM at given offset
    """
    if offset + 4 > len(data):
        return None
    return struct.unpack("<I", data[offset:offset+4])[0]

def main():
    parser = argparse.ArgumentParser(
        description="Read and display EEPROM contents in hexadecimal format",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  eeprom_hex_reader.py                    # Read entire EEPROM
  eeprom_hex_reader.py --offset 0 --length 64  # Read first 64 bytes
  eeprom_hex_reader.py --credits          # Show embedded credit values
  eeprom_hex_reader.py --dump eeprom.bin  # Save to binary file
        """
    )
    parser.add_argument("--offset", type=int, default=0, help="Starting address (0-255)")
    parser.add_argument("--length", type=int, default=EEPROM_SIZE, help="Number of bytes to read")
    parser.add_argument("--credits", action="store_true", help="Display credit values at standard offsets")
    parser.add_argument("--dump", type=str, help="Save EEPROM contents to binary file")
    
    args = parser.parse_args()
    
    print("[*] Reading EEPROM from I2C device...")
    data = read_eeprom_via_i2c(args.offset, args.length)
    
    if data is None:
        sys.exit(1)
    
    print(f"[+] Read {len(data)} bytes from EEPROM\n")
    
    # Display hex dump
    print(format_hex_dump(data, args.offset))
    
    # Display credits if requested
    if args.credits:
        print("\n" + "="*70)
        print("CREDIT VALUES:")
        print("="*70)
        
        # Standard credit offset locations
        credit_offsets = {
            "Primary":   0x00,
            "Secondary": 0x04,
            "Cache":     0x08,
            "Pending":   0x0C,
        }
        
        for name, offset in credit_offsets.items():
            value = read_credit_value(data, offset)
            if value is not None:
                print(f"{name:12} (offset 0x{offset:02x}): {value:10} credits (0x{value:08x})")
    
    # Save to file if requested
    if args.dump:
        try:
            with open(args.dump, "wb") as f:
                f.write(data)
            print(f"\n[+] Saved EEPROM contents to {args.dump}")
        except IOError as e:
            print(f"[ERROR] Failed to save file: {e}", file=sys.stderr)
            sys.exit(1)
    
    print("\n[+] EEPROM read complete")

if __name__ == "__main__":
    main()
