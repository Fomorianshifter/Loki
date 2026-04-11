#!/usr/bin/env python3
"""
EEPROM Hex Writer - Write data to EEPROM with verification
Part of Loki's EEPROM management toolkit for NFC/RFID credit systems
"""

import sys
import os
import struct
import argparse
import time
from pathlib import Path

# EEPROM I2C address and constants
EEPROM_I2C_ADDRESS = 0x50
EEPROM_SIZE = 256
EEPROM_PAGE_SIZE = 32  # Write page size for FT24C02A
EEPROM_WRITE_DELAY_MS = 5  # Delay between writes

def write_eeprom_via_i2c(address, data, verify=True):
    """
    Write data to EEPROM via I2C interface with optional verification
    """
    try:
        import smbus2
    except ImportError:
        print("[ERROR] smbus2 not installed. Install with: pip3 install smbus2", file=sys.stderr)
        return False
    
    if address + len(data) > EEPROM_SIZE:
        print(f"[ERROR] Write would exceed EEPROM bounds (max {EEPROM_SIZE} bytes)", file=sys.stderr)
        return False
    
    try:
        bus = smbus2.SMBus(0)
        
        # Write data in page-sized chunks
        for offset in range(0, len(data), EEPROM_PAGE_SIZE):
            chunk = data[offset:offset + EEPROM_PAGE_SIZE]
            chunk_addr = address + offset
            
            try:
                bus.write_i2c_block_data(EEPROM_I2C_ADDRESS, chunk_addr, list(chunk))
                print(f"  [+] Wrote {len(chunk)} bytes at offset 0x{chunk_addr:02x}")
                time.sleep(EEPROM_WRITE_DELAY_MS / 1000.0)  # Wait for write cycle
            except Exception as e:
                print(f"[ERROR] Write failed at offset 0x{chunk_addr:02x}: {e}", file=sys.stderr)
                bus.close()
                return False
        
        # Verify if requested
        if verify:
            print("\n[*] Verifying written data...")
            time.sleep(0.1)
            
            try:
                verify_data = bytearray()
                for offset in range(address, address + len(data), EEPROM_PAGE_SIZE):
                    read_len = min(EEPROM_PAGE_SIZE, address + len(data) - offset)
                    chunk = bus.read_i2c_block_data(EEPROM_I2C_ADDRESS, offset, read_len)
                    verify_data.extend(chunk)
                
                if verify_data == data:
                    print("  [+] Verification successful - data matches")
                else:
                    print("[WARNING] Verification failed - written data does not match", file=sys.stderr)
                    bus.close()
                    return False
            except Exception as e:
                print(f"[ERROR] Verification read failed: {e}", file=sys.stderr)
                bus.close()
                return False
        
        bus.close()
        return True
    except Exception as e:
        print(f"[ERROR] Failed to write EEPROM: {e}", file=sys.stderr)
        return False

def write_credit_value(address_offset, credit_value):
    """
    Write a 32-bit credit value to EEPROM
    """
    return struct.pack("<I", credit_value)

def main():
    parser = argparse.ArgumentParser(
        description="Write data to EEPROM with verification",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  eeprom_hex_writer.py --offset 0 --file data.bin           # Write file contents
  eeprom_hex_writer.py --offset 0 --hex "48656c6c6f"       # Write hex string
  eeprom_hex_writer.py --offset 0 --credit 1000             # Write credit value
  eeprom_hex_writer.py --offset 0 --credit 5000 --no-verify # Write without verification
        """
    )
    parser.add_argument("--offset", type=int, default=0, help="Starting address (0-255)")
    parser.add_argument("--file", type=str, help="Read data from binary file")
    parser.add_argument("--hex", type=str, help="Hex string to write (e.g., '48656c6c6f')")
    parser.add_argument("--credit", type=int, help="Credit value to write as 32-bit integer")
    parser.add_argument("--no-verify", action="store_true", help="Skip verification after write")
    parser.add_argument("--force", action="store_true", help="Skip confirmation prompt")
    
    args = parser.parse_args()
    
    if not (args.file or args.hex or args.credit is not None):
        parser.print_help()
        print("\n[ERROR] Must specify --file, --hex, or --credit", file=sys.stderr)
        sys.exit(1)
    
    # Prepare data to write
    data = None
    
    if args.file:
        try:
            with open(args.file, "rb") as f:
                data = f.read()
            print(f"[+] Read {len(data)} bytes from {args.file}")
        except IOError as e:
            print(f"[ERROR] Failed to read file: {e}", file=sys.stderr)
            sys.exit(1)
    
    elif args.hex:
        try:
            data = bytes.fromhex(args.hex)
            print(f"[+] Parsed hex string: {len(data)} bytes")
        except ValueError as e:
            print(f"[ERROR] Invalid hex string: {e}", file=sys.stderr)
            sys.exit(1)
    
    elif args.credit is not None:
        data = write_credit_value(0, args.credit)
        print(f"[+] Ready to write credit value: {args.credit} (0x{args.credit:08x})")
    
    # Confirmation
    if not args.force:
        print(f"\n[!] This will write {len(data)} bytes to EEPROM at offset 0x{args.offset:02x}")
        print(f"    WARNING: This may affect NFC/RFID credit systems!")
        response = input("Continue? [y/N]: ")
        if response.lower() != "y":
            print("[*] Cancelled")
            sys.exit(0)
    
    # Write to EEPROM
    print(f"\n[*] Writing {len(data)} bytes to EEPROM...")
    if write_eeprom_via_i2c(args.offset, data, verify=not args.no_verify):
        print("[+] Write successful")
    else:
        print("[ERROR] Write failed", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
