# Credits Management Guide

This guide shows you how to easily view and edit Loki credits from the web UI or SSH terminal.

## Overview

Credits represent your account balance in the NFC/RFID system. You can:
- 👁️ View current balance
- ➕ Add credits
- ➖ Subtract credits  
- 🎯 Set to specific amount
- 💾 Persist to EEPROM automatically

## 🌐 Web UI Method (Easiest)

### View Your Balance

Open your browser and navigate to:
```
http://loki.local:8080
http://<your-pi-ip>:8080
```

The **Credits Vault** card on the home page shows your current balance prominently in large gold text (¢ value).

### Quick Adjustments

Use the quick-buttons with pre-set amounts:
- **+ 10 ¢** - Add 10 credits
- **+ 50 ¢** - Add 50 credits  
- **- 10 ¢** - Subtract 10 credits
- **- 50 ¢** - Subtract 50 credits

Just click any button and the balance updates immediately!

### Manual Entry

For custom amounts:
1. Click **"Manual Entry"** button
2. A text box appears
3. Enter your desired amount
4. Click **"Set"** button
5. Balance updates to the exact value

**Example:**
```
Current: ¢ 250.00
→ Enter: 1000
→ New: ¢ 1000.00
```

----

## 🖥️ SSH / Terminal Method

### Interactive Menu

Connect via SSH and run the credits manager:

```bash
ssh pi@loki.local
python3 /opt/loki/actions/credits_cli.py
```

Or run it from the Loki web UI by selecting the **"Credits Manager"** tool.

This opens an interactive menu:

```
============================================================
🎮 Loki Credits Manager
============================================================

Current Balance:
  ¢ 250.50

Choose an action:
  [1] View balance
  [2] Set balance
  [3] Add credits
  [4] Subtract credits
  [5] Reset to 100
  [6] Reset to 0
  [0] Exit

Enter choice: _
```

### Quick Commands

**View current balance:**
```bash
python3 /opt/loki/actions/credits_cli.py --get
# Output: ¢ 250.50
```

**Set balance to 500:**
```bash
python3 /opt/loki/actions/credits_cli.py --set 500
# Output: ✓ Set to ¢ 500.00
```

**Add 100 credits:**
```bash
python3 /opt/loki/actions/credits_cli.py --add 100
# Output: ✓ Added ¢ 100.00, new balance: ¢ 600.00
```

**Subtract 50 credits:**
```bash
python3 /opt/loki/actions/credits_cli.py --sub 50
# Output: ✓ Subtracted ¢ 50.00, new balance: ¢ 550.00
```

### JSON Output

For scripting, get machine-readable JSON:

```bash
python3 /opt/loki/actions/credits_cli.py --get --json
# {"credits": 550.0}

python3 /opt/loki/actions/credits_cli.py --set 1000 --json
# {"ok": true, "credits": 1000.0}
```

### Custom Host/Port

If your Loki runs on a different IP or port:

```bash
python3 /opt/loki/actions/credits_cli.py --get --host 192.168.1.100 --port 8080
```

----|

## 💾 Persistence & Storage

All credit changes are:
- ✅ Automatically saved to I2C EEPROM (address 0x50)
- ✅ Persisted across reboots  
- ✅ Multi-account capable (primary, secondary, cache, lifetime)
- ✅ Transaction history tracked with timestamps

The credits system stores data at fixed memory locations:
```
Primary Credits   @ 0x00 (4 bytes)
Secondary Credits @ 0x04 (4 bytes)
Cache Credits     @ 0x08 (4 bytes)
Lifetime Earned   @ 0x0C (4 bytes)
NFC Enable Flag   @ 0x10 (1 byte)
RFID Enable Flag  @ 0x11 (1 byte)
Last Transaction  @ 0x12 (4 bytes)
Transaction Count @ 0x16 (4 bytes)
User ID           @ 0x18 (6 bytes)
```

----

## 🔌 REST API

Advanced users can directly call the API endpoints:

### GET /api/credits
Retrieve current balance:
```bash
curl http://loki.local:8080/api/credits
# {"credits":550.0}
```

### PATCH /api/credits
Update balance:
```bash
curl -X PATCH http://loki.local:8080/api/credits \
  -H "Content-Type: application/json" \
  -d '{"credits":1000}'
# {"ok":true,"credits":1000.0}
```

### POST /api/credits
Same as PATCH:
```bash
curl -X POST http://loki.local:8080/api/credits \
  -H "Content-Type: application/json" \
  -d '{"value":500}'
# {"ok":true,"credits":500.0}
```

Both `"credits"` and `"value"` field names are accepted.

----

## 🔍 Advanced: EEPROM Inspection

To view raw EEPROM contents with hex dump:

```bash
python3 /opt/loki/actions/eeprom_hex_reader.py
```

To modify EEPROM directly (advanced users only):

```bash
python3 /opt/loki/actions/eeprom_hex_writer.py
```

Warning: Modifying EEPROM directly can corrupt data if you don't know the memory layout!

----

## 📝 Common Workflows

### Initialize New Account with 1000 Credits
```bash
# Via CLI
python3 /opt/loki/actions/nfc_credit_manager.py --init --primary 1000

# Via Web UI - Select "NFC/RFID Credit Initialize" tool
```

### Transfer Between Accounts
```bash
python3 /opt/loki/actions/nfc_credit_manager.py --transfer primary secondary 100
```

### Export Credits to JSON Backup
```bash
python3 /opt/loki/actions/nfc_credit_manager.py --export credits_backup.json
```

### Restore from JSON Backup
```bash
python3 /opt/loki/actions/nfc_credit_manager.py --import credits_backup.json
```

### Enable/Disable NFC or RFID
```bash
# Enable NFC
python3 /opt/loki/actions/nfc_credit_manager.py --nfc-enabled

# Disable NFC
python3 /opt/loki/actions/nfc_credit_manager.py --nfc-disabled

# Enable RFID
python3 /opt/loki/actions/nfc_credit_manager.py --rfid-enabled
```

----

## 🧪 Troubleshooting

### Can't connect to web UI
- Check if Loki is running: `sudo systemctl status loki`
- Verify network: `ping loki.local`
- Check port: Try `http://<ip>:8080` instead of hostname

### Can't connect via SSH  
```bash
ssh pi@loki.local  # Use default password or configured SSH key
```

### CLI script fails with "Connection refused"
- Verify Loki service is running
- Check that web UI is enabled in loki.conf: `web_ui_enabled=true`
- Try connecting to localhost on the device itself

### Permissions denied running Python script
```bash
# Make script executable
chmod +x /opt/loki/actions/credits_cli.py

# Or run explicitly with Python
python3 /opt/loki/actions/credits_cli.py
```

### EEPROM changes not persisting
- Check I2C communication: `i2cdetect -y 0`
  - Should show device at address `50` 
- Verify EEPROM chip is present and soldered correctly
- Check dmesg for I2C errors: `sudo dmesg | grep -i "i2c\|eeprom"`

----

## 📚 Related Documentation

- [FULL_SETUP_GUIDE.md](FULL_SETUP_GUIDE.md) - Complete hardware setup and deployment
- [EEPROM Hex Tools Guide](FULL_SETUP_GUIDE.md#eeprom-tools) - Low-level EEPROM inspection
- [NFC/RFID Credit Manager](FULL_SETUP_GUIDE.md#nfc-credit-system) - Advanced credit operations
- [API Reference](FULL_SETUP_GUIDE.md#rest-api) - Complete REST endpoints

Happy crediting! 🎯
