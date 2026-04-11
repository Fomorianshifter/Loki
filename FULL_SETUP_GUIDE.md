# 🚀 Loki Full Setup Guide - Getting Everything Running

This guide walks you through building, deploying, and running Loki with all tools including EEPROM management and NFC/RFID credit systems.

## Prerequisites

### Hardware Required
- **Orange Pi Zero 2W** or Raspberry Pi 3/4
- **MicroSD Card** (16GB+ recommended)
- **UPS HAT** (optional but recommended for stability)
- **ST7789 LCD HAT 1.3"** (optional for display)
- **I2C EEPROM Module (FT24C02A)** - for NFC/RFID credits
- **RS485 Serial Module** (optional, for Flipper Zero serial)
- **Power Supply** (5V/3A minimum)

### Software Requirements

#### On Your Build Machine (Windows/Mac/Linux):
```bash
# For Linux/Mac users:
sudo apt-get install gcc-arm-linux-gnueabihf git ssh

# For Windows users:
# - Install WSL Ubuntu: https://learn.microsoft.com/windows/wsl/install
# - Or use the PowerShell/CMD scripts
```

#### On Orange Pi Target:
```bash
# SSH into your Orange Pi and install dependencies:
sudo apt-get update
sudo apt-get install -y python3 python3-pip i2c-tools

# Install Python packages for the tools:
pip3 install smbus2 requests
```

---

## Step 1: Build Loki Natively

### Option A: Linux/Mac Cross-Compilation

```bash
cd /path/to/Loki

# Debug build (with symbols for debugging):
make DEBUG=1 PLATFORM=LINUX ARCH=32

# Release build (optimized):
make DEBUG=0 PLATFORM=LINUX ARCH=32

# Check the binary:
ls -lh build/release/loki_app
```

### Option B: Windows PowerShell

```powershell
cd C:\path\to\Loki

# Using PowerShell script:
.\build.ps1 -Mode release

# Check the binary:
Get-Item .\build\release\loki_app.exe
```

### Verify Build Success

```bash
# File should exist and have reasonable size:
build/release/loki_app
# Expected: ~150-300 KB depending on options
```

---

## Step 2: Prepare Orange Pi Target

### Initial Setup (One-time)

```bash
# 1. Flash Armbian to MicroSD (on your build machine):
# Download from: https://www.armbian.com/orange-pi-zero-2w/
# Use: balena etcher, rufus, or dd command

# 2. Insert MicroSD and power on Orange Pi
# 3. Wait for boot (2-3 minutes)

# 4. SSH into the device:
ssh root@orange-pi.local
# or: ssh pi@192.168.X.X
# Default Armbian password: 1234 or 12345
```

### Network Configuration

```bash
# If Ethernet isn't available, set up WiFi:
sudo nmtui  # Interactive network manager
# or manually:
sudo nano /etc/network/interfaces

# Add WiFi config:
auto wlan0
iface wlan0 inet dhcp
    wpa-ssid "YOUR_NETWORK"
    wpa-psk "YOUR_PASSWORD"

# Restart networking:
sudo systemctl restart networking
ping google.com  # Test connectivity
```

### Enable I2C and Serial

```bash
# Install I2C support:
sudo apt-get install -y i2c-tools libi2c-dev

# Enable I2C in device tree overlays:
sudo nano /boot/armbianEnv.txt

# Add this line (if not present):
overlays=i2c0

# Reboot:
sudo reboot
```

---

## Step 3: Deploy Loki to Orange Pi

### Create Installation Directory

```bash
ssh pi@orange-pi.local "sudo mkdir -p /opt/loki && sudo chown pi:pi /opt/loki"
```

### Copy Binary and Tools

```bash
# From your build machine:
SCP_HOST=orange-pi.local
SCP_USER=pi

# Copy main binary:
scp build/release/loki_app ${SCP_USER}@${SCP_HOST}:/opt/loki/

# Copy all Python tools:
scp actions/*.py ${SCP_USER}@${SCP_HOST}:/opt/loki/actions/

# Copy configuration:
scp loki.conf ${SCP_USER}@${SCP_HOST}:/opt/loki/

# Make executable:
ssh ${SCP_USER}@${SCP_HOST} "chmod +x /opt/loki/loki_app /opt/loki/actions/*.py"
```

### Or Use the Build Script

```bash
# Linux/Mac users can use the install target:
make install CROSS_HOST=orange-pi.local CROSS_USER=pi CROSS_PATH=/opt/loki

# This handles copying and permissions automatically
```

---

## Step 4: Configure EEPROM for NFC/RFID

### Detect I2C Devices

```bash
ssh pi@orange-pi.local

# List I2C buses:
i2cdetect -l

# Scan I2C bus 0 for EEPROM (address 0x50):
i2cdetect -y 0

# You should see: 50  (the EEPROM address)
```

### Initialize Credit System

```bash
# SSH into Orange Pi:
ssh pi@orange-pi.local
cd /opt/loki

# Initialize with 5000 credits:
python3 actions/nfc_credit_manager.py --init --primary 5000

# Check status:
python3 actions/nfc_credit_manager.py --status
```

### Expected Output

```
============================================================
NFC/RFID CREDIT SYSTEM STATUS
============================================================
  Primary         :       5,000 credits
  Secondary       :           0 credits
  Cache           :       5,000 credits
  Lifetime        :       5,000 credits
  Nfc_enabled     :         ENABLED
  Rfid_enabled    :         ENABLED
  User ID         : 0x12345678
  Transactions    :           0
============================================================
```

---

## Step 5: Test EEPROM Tools

### Read EEPROM Hex

```bash
# SSH into Orange Pi:
cd /opt/loki

# Read and display entire EEPROM:
python3 actions/eeprom_hex_reader.py

# Read specific range:
python3 actions/eeprom_hex_reader.py --offset 0 --length 32

# Show embedded credit values:
python3 actions/eeprom_hex_reader.py --credits
```

### Write Test Data

```bash
# Write hex string to EEPROM at offset 0x20:
python3 actions/eeprom_hex_writer.py --offset 32 --hex "DEADBEEF"

# Verify by reading:
python3 actions/eeprom_hex_reader.py --offset 32 --length 4
```

### Export/Import Credits

```bash
# Export current credits to JSON:
python3 actions/nfc_credit_manager.py --export credits.json

# View the file:
cat credits.json

# Modify the file manually:
nano credits.json  # Change "primary": 5000 to "primary": 10000

# Import back:
python3 actions/nfc_credit_manager.py --import credits.json

# Verify:
python3 actions/nfc_credit_manager.py --status
```

---

## Step 6: Start the Web UI and Services

### Manual Test Run

```bash
ssh pi@orange-pi.local
cd /opt/loki

# Run Loki with verbose output:
./loki_app

# Expected output:
# [INIT] Loki Dragon System v1.0
# [INFO] Dragon AI initialized
# [INFO] Web UI ready on http://0.0.0.0:8080
# [INFO] EEPROM driver initialized
# ...
```

### Access Web Interface

From your build machine or any device on the same network:

```bash
# Open browser to:
http://orange-pi.local:8080
# or
http://192.168.X.X:8080  # Replace with actual IP
```

### Expected Web UI Features

- 🐉 Animated dragon sprite (Egg/Hatchling/Adult stages)
- 💬 Dynamic dialogue based on mood and action
- 📊 Live telemetry dashboard
- 🛠️ Tool runner interface
- 📱 Mobile-responsive design
- ✨ Credit balance display

---

## Step 7: Run All Tools

### Via Web UI

1. Open http://orange-pi.local:8080
2. Go to **"Tools"** tab
3. Click any tool to execute:
   - **Status Feed** - Shows current status
   - **Wardrive Journal** - WiFi scanning logs
   - **GPS Console** - Location data
   - **EEPROM Hex Reader** - Read EEPROM contents
   - **NFC/RFID Credit Manager** - Check/manage credits
   - **Bjorn's Tools** - SSH, FTP, SMB, RDP, Telnet, SQL connectors

### Via Command Line

```bash
# SSH into Orange Pi:
ssh pi@orange-pi.local
cd /opt/loki/actions

# Examples:

# 1. Read EEPROM status:
python3 eeprom_hex_reader.py --credits

# 2. Transfer credits between accounts:
python3 nfc_credit_manager.py --transfer 100

# 3. Add credits:
python3 nfc_credit_manager.py --add 500

# 4. Run SSH connector:
python3 ssh_connector.py --help

# 5. Run FTP connector:
python3 ftp_connector.py --help
```

---

## Step 8: Setup Systemd Service (Optional)

### Create Service File

```bash
ssh pi@orange-pi.local

# Create service file:
sudo tee /etc/systemd/system/loki.service > /dev/null << EOF
[Unit]
Description=Loki Dragon System
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/opt/loki
ExecStart=/opt/loki/loki_app
Restart=on-failure
RestartSec=5s

[Install]
WantedBy=multi-user.target
EOF

# Enable and start:
sudo systemctl daemon-reload
sudo systemctl enable loki
sudo systemctl start loki

# Check status:
sudo systemctl status loki

# View logs:
sudo journalctl -u loki -f
```

### Monitor the Service

```bash
# Check if running:
systemctl is-active loki

# Restart:
sudo systemctl restart loki

# Stop:
sudo systemctl stop loki

# See recent logs:
journalctl -u loki -n 50
```

---

## Step 9: EEPROM Hardware Wiring

### I2C EEPROM Connection (FT24C02A)

```
Orange Pi GPIO Header          FT24C02A Module
===========================    ================
Pin 1  (3.3V)        -------->  VCC (pin 8)
Pin 6  (GND)         -------->  GND (pins 4, 5)
Pin 3  (SDA / GPIO26) -------->  SDA (pin 5)
Pin 5  (SCL / GPIO5)  -------->  SCL (pin 6)

Optional Pull-ups (if not on module):
- 10kΩ resistor from SDA to 3.3V
- 10kΩ resistor from SCL to 3.3V
```

### Verify Connection

```bash
# After wiring, verify the EEPROM is detected:
i2cdetect -y 0

# Should show address 0x50 present
```

---

## Step 10: Troubleshooting

### EEPROM Not Detected

```bash
# 1. Check I2C buses:
i2cdetect -l

# 2. Scan for devices:
i2cdetect -y 0
i2cdetect -y 1

# 3. Check power and connections
# 4. Try pulling down pins manually for 1 second
i2cset -y 0 0x50 0x00 0x00  # Write dummy value
```

### I2C Permissions Denied

```bash
# Add user to i2c group:
sudo usermod -a -G i2c pi
# Log out and back in to take effect
```

### Tools Not Executable

```bash
# Ensure executable:
chmod +x /opt/loki/actions/*.py

# Test Python import:
python3 -c "import smbus2; print('OK')"
```

### Web UI Not Responding

```bash
# Check if running:
ps aux | grep loki_app

# Check port:
sudo netstat -tlnp | grep 8080

# View logs:
tail -100 /var/log/loki.log 2>/dev/null || dmesg | tail -50
```

---

## Step 11: Common Tasks

### Add Custom Tools

Edit `/opt/loki/loki.conf`:

```ini
[tool_my_custom_tool]
enabled=true
name=My Custom Tool
description=This is my custom tool
command=/path/to/script.sh --arg1 value1
```

Then restart Loki.

### Backup Credits

```bash
# Export to USB drive:
python3 /opt/loki/actions/nfc_credit_manager.py --export /media/usb/credits_backup.json

# Restore from backup:
python3 /opt/loki/actions/nfc_credit_manager.py --import /media/usb/credits_backup.json
```

### Monitor Dragon Growth

```bash
# Watch debug output:
sudo journalctl -u loki -f | grep dragon

# Or check web UI telemetry tab
```

### Update Loki

```bash
# On your build machine:
cd /path/to/Loki
git pull origin loki/runtime-ui-finish
make DEBUG=0 PLATFORM=LINUX ARCH=32

# Deploy:
make install CROSS_HOST=orange-pi.local CROSS_USER=pi

# Restart service:
ssh pi@orange-pi.local sudo systemctl restart loki
```

---

## Security Notes ⚠️

1. **Change Default Credentials**: Orange Pi default password is weak
   ```bash
   ssh pi@orange-pi.local
   passwd
   sudo passwd root
   ```

2. **Restrict Web UI Access**: Add firewall rules
   ```bash
   sudo ufw enable
   sudo ufw allow 22/tcp   # SSH
   sudo ufw allow 8080/tcp # Web UI from trusted networks only
   ```

3. **Secure EEPROM Access**: Only trusted processes should modify credits
   - Run credit operations as root only
   - Use permission system to restrict who can run tools

4. **Enable SSH Key Authentication**: Don't rely on password SSH
   ```bash
   ssh-copy-id pi@orange-pi.local
   ```

---

## Performance Tips 🚀

1. **Use Release Build**: `make DEBUG=0` for better performance
2. **Enable Caching**: Reduce I2C reads for frequently accessed data
3. **Monitor Resource Usage**: `top`, `free`, `df` commands
4. **Set Appropriate Logging Level**: `LOG_LEVEL=2` for production

---

## Next Steps

1. ✅ Build and deploy Loki
2. ✅ Initialize NFC/RFID credit system
3. ✅ Verify all hardware connections
4. ✅ Test tools from web UI
5. 🔜 Create custom tools for your use case
6. 🔜 Integrate with external systems (NFC readers, databases, etc.)
7. 🔜 Set up monitoring and alerting

---

## Support & Resources

- **GitHub**: https://github.com/Fomorianshifter/Loki
- **Documentation**: See README.md and individual .md files
- **Armbian Docs**: https://docs.armbian.com/
- **Orange Pi Wiki**: http://wiki.orangepi.org/

---

**Happy hacking! Your Loki dragon system is ready to go! 🐉✨**
