#!/usr/bin/env python3
"""
Credits Management CLI Tool
Easy interactive management of Loki credits system via SSH or terminal
"""

import os
import sys
import json
import argparse
import requests
from datetime import datetime

# Colors for terminal output
class Colors:
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    RESET = '\033[0m'
    BOLD = '\033[1m'

def print_header(msg):
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}{msg}{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.RESET}\n")

def print_success(msg):
    print(f"{Colors.GREEN}✓ {msg}{Colors.RESET}")

def print_error(msg):
    print(f"{Colors.RED}✗ {msg}{Colors.RESET}")

def print_info(msg):
    print(f"{Colors.BLUE}ℹ {msg}{Colors.RESET}")

def get_api_url(host="localhost", port=8080):
    """Build API URL"""
    return f"http://{host}:{port}/api"

def get_credits(host="localhost", port=8080):
    """Fetch current credits from API"""
    try:
        url = f"{get_api_url(host, port)}/credits"
        response = requests.get(url, timeout=5)
        if response.status_code == 200:
            data = response.json()
            return data.get('credits', 0.0)
        else:
            print_error(f"API returned status {response.status_code}")
            return None
    except Exception as e:
        print_error(f"Failed to get credits: {e}")
        return None

def set_credits(amount, host="localhost", port=8080):
    """Set credits via API"""
    try:
        url = f"{get_api_url(host, port)}/credits"
        payload = {"credits": float(amount)}
        response = requests.patch(url, json=payload, timeout=5)
        if response.status_code == 200:
            data = response.json()
            if data.get('ok'):
                return data.get('credits', 0.0)
            else:
                print_error(f"API error: {data.get('error', 'unknown')}")
                return None
        else:
            print_error(f"API returned status {response.status_code}")
            return None
    except Exception as e:
        print_error(f"Failed to set credits: {e}")
        return None

def add_credits(amount, host="localhost", port=8080):
    """Add to current credits"""
    current = get_credits(host, port)
    if current is not None:
        new_amount = current + float(amount)
        return set_credits(new_amount, host, port)
    return None

def subtract_credits(amount, host="localhost", port=8080):
    """Subtract from current credits"""
    current = get_credits(host, port)
    if current is not None:
        new_amount = max(0, current - float(amount))
        return set_credits(new_amount, host, port)
    return None

def interactive_menu(host="localhost", port=8080):
    """Interactive credits management menu"""
    print_header("🎮 Loki Credits Manager")
    
    while True:
        print(f"{Colors.BOLD}Current Balance:{Colors.RESET}")
        current = get_credits(host, port)
        if current is None:
            print_error("Cannot connect to Loki")
            break
        
        print(f"  ¢ {Colors.BOLD}{current:.2f}{Colors.RESET}\n")
        
        print("Choose an action:")
        print("  [1] View balance")
        print("  [2] Set balance")
        print("  [3] Add credits")
        print("  [4] Subtract credits")
        print("  [5] Reset to 100")
        print("  [6] Reset to 0")
        print("  [0] Exit\n")
        
        choice = input(f"{Colors.CYAN}Enter choice: {Colors.RESET}").strip()
        
        if choice == '0':
            print_success("Goodbye!")
            break
        elif choice == '1':
            print_success(f"Current balance: ¢ {current:.2f}\n")
        elif choice == '2':
            try:
                amount = float(input(f"{Colors.CYAN}Enter new balance: ¢ {Colors.RESET}"))
                new_val = set_credits(amount, host, port)
                if new_val is not None:
                    print_success(f"Balance updated to ¢ {new_val:.2f}\n")
            except ValueError:
                print_error("Invalid amount\n")
        elif choice == '3':
            try:
                amount = float(input(f"{Colors.CYAN}Enter amount to add: ¢ {Colors.RESET}"))
                new_val = add_credits(amount, host, port)
                if new_val is not None:
                    print_success(f"Added ¢ {amount:.2f}, new balance: ¢ {new_val:.2f}\n")
            except ValueError:
                print_error("Invalid amount\n")
        elif choice == '4':
            try:
                amount = float(input(f"{Colors.CYAN}Enter amount to subtract: ¢ {Colors.RESET}"))
                new_val = subtract_credits(amount, host, port)
                if new_val is not None:
                    print_success(f"Subtracted ¢ {amount:.2f}, new balance: ¢ {new_val:.2f}\n")
            except ValueError:
                print_error("Invalid amount\n")
        elif choice == '5':
            new_val = set_credits(100, host, port)
            if new_val is not None:
                print_success(f"Reset to ¢ {new_val:.2f}\n")
        elif choice == '6':
            new_val = set_credits(0, host, port)
            if new_val is not None:
                print_success(f"Reset to ¢ {new_val:.2f}\n")
        else:
            print_error("Invalid choice\n")

def main():
    parser = argparse.ArgumentParser(
        description="Loki Credits Management CLI",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  credentials_cli.py --host localhost --port 8080      # Interactive mode
  credentials_cli.py --get --host localhost              # View balance
  credentials_cli.py --set 500 --host 192.168.1.100     # Set balance to 500
  credentials_cli.py --add 50 --port 8080                # Add 50 credits
  credentials_cli.py --sub 25                            # Subtract 25 credits
        """)
    
    parser.add_argument('--host', default='localhost', help='Loki API host (default: localhost)')
    parser.add_argument('--port', type=int, default=8080, help='Loki API port (default: 8080)')
    parser.add_argument('--get', action='store_true', help='Get current balance')
    parser.add_argument('--set', type=float, metavar='AMOUNT', help='Set balance to AMOUNT')
    parser.add_argument('--add', type=float, metavar='AMOUNT', help='Add AMOUNT to balance')
    parser.add_argument('--sub', type=float, metavar='AMOUNT', help='Subtract AMOUNT from balance')
    parser.add_argument('--json', action='store_true', help='Output JSON instead of text')
    
    args = parser.parse_args()
    
    # No arguments = interactive mode
    if len(sys.argv) == 1:
        interactive_menu(args.host, args.port)
        return
    
    # Command-line mode
    if args.get:
        balance = get_credits(args.host, args.port)
        if balance is not None:
            if args.json:
                print(json.dumps({"credits": balance}))
            else:
                print(f"¢ {balance:.2f}")
    elif args.set is not None:
        result = set_credits(args.set, args.host, args.port)
        if result is not None:
            if args.json:
                print(json.dumps({"ok": True, "credits": result}))
            else:
                print(f"✓ Set to ¢ {result:.2f}")
    elif args.add is not None:
        result = add_credits(args.add, args.host, args.port)
        if result is not None:
            if args.json:
                print(json.dumps({"ok": True, "credits": result}))
            else:
                print(f"✓ Added ¢ {args.add:.2f}, new balance: ¢ {result:.2f}")
    elif args.sub is not None:
        result = subtract_credits(args.sub, args.host, args.port)
        if result is not None:
            if args.json:
                print(json.dumps({"ok": True, "credits": result}))
            else:
                print(f"✓ Subtracted ¢ {args.sub:.2f}, new balance: ¢ {result:.2f}")
    else:
        # Default to interactive
        interactive_menu(args.host, args.port)

if __name__ == '__main__':
    main()
