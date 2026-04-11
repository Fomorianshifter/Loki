#!/usr/bin/env python3
"""
NFC/RFID Credit Manager - Manage credits stored in EEPROM for NFC/RFID systems
Handles credit initialization, transfers, and NFC transaction simulation
"""

import sys
import struct
import time
import argparse
import json
from datetime import datetime

EEPROM_I2C_ADDRESS = 0x50
EEPROM_SIZE = 256

# Credit storage layout (configurable)
CREDIT_LAYOUT = {
    "primary":       {"offset": 0x00, "size": 4},  # Main credit account
    "secondary":     {"offset": 0x04, "size": 4},  # Secondary/backup account
    "cache":         {"offset": 0x08, "size": 4},  # Cache balance for quick access
    "lifetime":      {"offset": 0x0C, "size": 4},  # Lifetime credits earned
    "nfc_enabled":   {"offset": 0x10, "size": 1},  # NFC enabled flag
    "rfid_enabled":  {"offset": 0x11, "size": 1},  # RFID enabled flag
    "last_txn_time": {"offset": 0x12, "size": 4},  # Last transaction Unix timestamp
    "txn_count":     {"offset": 0x16, "size": 2},  # Transaction count
    "user_id":       {"offset": 0x18, "size": 4},  # User ID for NFC tag
    "reserved":      {"offset": 0x1C, "size": 4},  # Reserved for future use
}

class EEPROMCreditManager:
    def __init__(self):
        self.data = bytearray(EEPROM_SIZE)
        self.smbus = None
        self.i2c_available = False
        self._init_i2c()
    
    def _init_i2c(self):
        """Initialize I2C connection"""
        try:
            import smbus2
            self.smbus = smbus2.SMBus(0)
            self.i2c_available = True
            print("[+] I2C interface initialized")
        except ImportError:
            print("[!] smbus2 not installed - using simulation mode", file=sys.stderr)
        except Exception as e:
            print(f"[!] Failed to open I2C: {e} - using simulation mode", file=sys.stderr)
    
    def read_from_eeprom(self):
        """Read entire EEPROM contents"""
        if not self.i2c_available:
            print("[*] Using simulated EEPROM (no I2C device)")
            return True
        
        try:
            for offset in range(0, EEPROM_SIZE, 32):
                chunk = self.smbus.read_i2c_block_data(EEPROM_I2C_ADDRESS, offset, 32)
                self.data[offset:offset+32] = chunk
            return True
        except Exception as e:
            print(f"[ERROR] Failed to read EEPROM: {e}", file=sys.stderr)
            return False
    
    def write_to_eeprom(self):
        """Write entire EEPROM contents"""
        if not self.i2c_available:
            print("[*] Using simulated EEPROM (no I2C device)")
            return True
        
        try:
            for offset in range(0, EEPROM_SIZE, 32):
                chunk = list(self.data[offset:offset+32])
                self.smbus.write_i2c_block_data(EEPROM_I2C_ADDRESS, offset, chunk)
                time.sleep(0.005)
            return True
        except Exception as e:
            print(f"[ERROR] Failed to write EEPROM: {e}", file=sys.stderr)
            return False
    
    def get_credit(self, account="primary"):
        """Get credit value from specified account"""
        if account not in CREDIT_LAYOUT:
            return None
        
        layout = CREDIT_LAYOUT[account]
        offset = layout["offset"]
        size = layout["size"]
        
        if size == 4:
            return struct.unpack("<I", self.data[offset:offset+4])[0]
        elif size == 2:
            return struct.unpack("<H", self.data[offset:offset+2])[0]
        elif size == 1:
            return self.data[offset]
        return None
    
    def set_credit(self, account, value):
        """Set credit value in specified account"""
        if account not in CREDIT_LAYOUT:
            return False
        
        layout = CREDIT_LAYOUT[account]
        offset = layout["offset"]
        size = layout["size"]
        
        try:
            if size == 4:
                struct.pack_into("<I", self.data, offset, value & 0xFFFFFFFF)
            elif size == 2:
                struct.pack_into("<H", self.data, offset, value & 0xFFFF)
            elif size == 1:
                self.data[offset] = value & 0xFF
            return True
        except Exception as e:
            print(f"[ERROR] Failed to set credit: {e}", file=sys.stderr)
            return False
    
    def transfer_credits(self, amount, from_account="primary", to_account="secondary"):
        """Transfer credits between accounts"""
        from_balance = self.get_credit(from_account)
        if from_balance is None or from_balance < amount:
            print(f"[ERROR] Insufficient balance in {from_account}", file=sys.stderr)
            return False
        
        to_balance = self.get_credit(to_account)
        if to_balance is None:
            print(f"[ERROR] Invalid destination account {to_account}", file=sys.stderr)
            return False
        
        self.set_credit(from_account, from_balance - amount)
        self.set_credit(to_account, to_balance + amount)
        
        # Log transaction
        self.set_credit("last_txn_time", int(time.time()))
        txn_count = self.get_credit("txn_count")
        self.set_credit("txn_count", (txn_count + 1) & 0xFFFF)
        
        print(f"[+] Transferred {amount} credits from {from_account} to {to_account}")
        return True
    
    def show_status(self):
        """Display current credit status"""
        print("\n" + "="*60)
        print("NFC/RFID CREDIT SYSTEM STATUS")
        print("="*60)
        
        for account, layout in CREDIT_LAYOUT.items():
            if account in ["primary", "secondary", "cache", "lifetime"]:
                value = self.get_credit(account)
                print(f"  {account.capitalize():15} : {value:>10,} credits")
            elif account in ["nfc_enabled", "rfid_enabled"]:
                value = self.get_credit(account)
                status = "ENABLED" if value else "DISABLED"
                print(f"  {account.capitalize():15} : {status:>10}")
        
        last_txn = self.get_credit("last_txn_time")
        txn_count = self.get_credit("txn_count")
        user_id = self.get_credit("user_id")
        
        print(f"  {'User ID':15} : 0x{user_id:08x}")
        print(f"  {'Transactions':15} : {txn_count:>10}")
        
        if last_txn > 0:
            last_txn_dt = datetime.fromtimestamp(last_txn)
            print(f"  {'Last Transaction':15} : {last_txn_dt.strftime('%Y-%m-%d %H:%M:%S')}")
        
        print("="*60 + "\n")
    
    def initialize_credits(self, primary=1000, secondary=0, user_id=0x12345678):
        """Initialize credit accounts"""
        print(f"[*] Initializing credit accounts...")
        self.set_credit("primary", primary)
        self.set_credit("secondary", secondary)
        self.set_credit("cache", primary)
        self.set_credit("lifetime", primary)
        self.set_credit("nfc_enabled", 1)
        self.set_credit("rfid_enabled", 1)
        self.set_credit("user_id", user_id)
        self.set_credit("last_txn_time", int(time.time()))
        self.set_credit("txn_count", 0)
        
        print(f"[+] Initialized: Primary={primary}, Secondary={secondary}, UserID=0x{user_id:08x}")
        return True

def main():
    parser = argparse.ArgumentParser(
        description="Manage NFC/RFID credits stored in EEPROM",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  nfc_credit_manager.py --status                              # Show current status
  nfc_credit_manager.py --init --primary 5000 --user 0x99887766  # Initialize
  nfc_credit_manager.py --transfer 500                         # Transfer from primary to secondary
  nfc_credit_manager.py --add 100                              # Add credits to primary
  nfc_credit_manager.py --set primary 2000                     # Set primary account to 2000
  nfc_credit_manager.py --export credits.json                  # Export to JSON
  nfc_credit_manager.py --import credits.json                  # Import from JSON
        """
    )
    parser.add_argument("--status", action="store_true", help="Show credit status")
    parser.add_argument("--init", action="store_true", help="Initialize credit accounts")
    parser.add_argument("--primary", type=int, default=1000, help="Initial primary credits")
    parser.add_argument("--secondary", type=int, default=0, help="Initial secondary credits")
    parser.add_argument("--user", type=lambda x: int(x, 0), help="User ID (hex or decimal)")
    parser.add_argument("--transfer", type=int, help="Transfer amount from primary to secondary")
    parser.add_argument("--add", type=int, help="Add credits to primary account")
    parser.add_argument("--set", type=str, nargs=2, metavar=("ACCOUNT", "VALUE"), help="Set account to value")
    parser.add_argument("--export", type=str, help="Export credits to JSON file")
    parser.add_argument("--import", type=str, dest="import_file", help="Import credits from JSON file")
    
    args = parser.parse_args()
    
    # Create manager and read current state
    manager = EEPROMCreditManager()
    if not manager.read_from_eeprom():
        sys.exit(1)
    
    # Process commands
    if args.init:
        user_id = args.user if args.user is not None else 0x12345678
        if manager.initialize_credits(args.primary, args.secondary, user_id):
            manager.write_to_eeprom()
    
    elif args.transfer is not None:
        if manager.transfer_credits(args.transfer, "primary", "secondary"):
            manager.write_to_eeprom()
    
    elif args.add is not None:
        primary = manager.get_credit("primary")
        manager.set_credit("primary", primary + args.add)
        manager.write_to_eeprom()
        print(f"[+] Added {args.add} credits to primary account (new balance: {primary + args.add})")
    
    elif args.set:
        account, value = args.set
        value = int(value)
        if manager.set_credit(account, value):
            manager.write_to_eeprom()
            print(f"[+] Set {account} to {value}")
    
    elif args.export:
        data = {
            "primary": manager.get_credit("primary"),
            "secondary": manager.get_credit("secondary"),
            "lifetime": manager.get_credit("lifetime"),
            "user_id": f"0x{manager.get_credit('user_id'):08x}",
            "nfc_enabled": bool(manager.get_credit("nfc_enabled")),
            "rfid_enabled": bool(manager.get_credit("rfid_enabled")),
        }
        try:
            with open(args.export, "w") as f:
                json.dump(data, f, indent=2)
            print(f"[+] Exported credits to {args.export}")
        except IOError as e:
            print(f"[ERROR] Failed to export: {e}", file=sys.stderr)
            sys.exit(1)
    
    elif args.import_file:
        try:
            with open(args.import_file, "r") as f:
                data = json.load(f)
            manager.set_credit("primary", data.get("primary", 1000))
            manager.set_credit("secondary", data.get("secondary", 0))
            manager.write_to_eeprom()
            print(f"[+] Imported credits from {args.import_file}")
        except (IOError, json.JSONDecodeError) as e:
            print(f"[ERROR] Failed to import: {e}", file=sys.stderr)
            sys.exit(1)
    
    else:
        # Default to showing status
        args.status = True
    
    if args.status:
        manager.show_status()

if __name__ == "__main__":
    main()
