# Loki Deployment Guide

Complete instructions for deploying Loki to Orange Pi Zero 2W hardware.

## Version Information

- **Project**: Loki Orange Pi Zero 2W Embedded System
- **Target Hardware**: Orange Pi Zero 2W (ARMv7 Cortex-A7)
- **OS**: Armbian or Orange Pi OS (Linux-based)
- **Status**: Ready for deployment (v1.0)
- **Updated**: February 2026

---

## Pre-Deployment Checklist

### ✅ Development Machine Setup
- [ ] ARM cross-compiler installed: `arm-linux-gnueabihf-gcc`
  ```bash
  # Ubuntu/Debian
  sudo apt-get install gcc-arm-linux-gnueabihf
  
  # Verify installation
  arm-linux-gnueabihf-gcc --version
  ```
- [ ] GNU Make installed: `make`
  ```bash
  sudo apt-get install make
  ```
- [ ] Doxygen installed (optional, for documentation): `doxygen`
  ```bash
  sudo apt-get install doxygen
  ```
- [ ] cppcheck installed (optional, for analysis): `cppcheck`
  ```bash
  sudo apt-get install cppcheck
  ```

### ✅ Orange Pi Zero 2W Hardware Setup
- [ ] Orange Pi running Armbian or Orange Pi OS
- [ ] SSH access enabled and working
  ```bash
  # Test SSH connection
  ssh pi@orange-pi.local

  # Or if DHCP not working, use IP address from router
  ssh pi@192.168.1.XXX
  ```
- [ ] Development tools available on Orange Pi:
  ```bash
  # On Orange Pi - install cross-compilation runtime support
  sudo apt-get update
  sudo apt-get install libc6-armhf-cross

  # Optional: GDB for remote debugging
  sudo apt-get install gdbserver
  ```
- [ ] Wiring verified:
  - [ ] SPI0 (TFT): Pins 23, 19, 21, 24
  - [ ] SPI1 (SD): Pins 29, 31, 33, 32
  - [ ] SPI2 (Flash): Pins 13, 11, 12, 15
  - [ ] I2C0 (EEPROM): Pins 3, 5
  - [ ] UART1 (Flipper): Pins 8, 10
  - [ ] PWM (Backlight): Pin 7
  - [ ] Power, GND, and all control pins

### ✅ Code Validation
- [ ] Project builds locally:
  ```bash
  make clean
  make DEBUG=1
  ```
- [ ] Static analysis passes:
  ```bash
  make analyze
  ```
- [ ] No compiler warnings:
  ```bash
  make 2>&1 | grep -i "warning"
  ```
- [ ] Binary size acceptable:
  ```bash
  make size
  ```

---

## Deployment Steps

### Step 1: Clone/Transfer Project to Development Machine

```bash
# Option A: If already cloned
cd ~/Loki

# Option B: Download from repository
git clone https://github.com/your-org/Loki.git
cd Loki
```

### Step 2: Verify Cross-Compiler Configuration

Edit **Makefile** and verify cross-compiler path and Orange Pi hostname:

```makefile
# Makefile (lines ~10-15)
ARM_CROSS_COMPILE ?= arm-linux-gnueabihf-

# Check your cross-compiler
which arm-linux-gnueabihf-gcc  # Should show path

# Verify Orange Pi hostname/IP
# Change CROSS_HOST if needed:
CROSS_HOST ?= orange-pi.local
# Or use IP address:
CROSS_HOST ?= 192.168.1.100
```

### Step 3: Build Release Binary

```bash
# Clean previous builds
make clean-all

# Build optimized release binary
make DEBUG=0

# Verify binary was created
ls -lah loki_app
```

### Step 4: Run Static Analysis (Optional)

```bash
# Check code quality
make analyze

# Review any warnings and fix if critical
```

### Step 5: Generate Documentation (Optional)

```bash
# Create API documentation
make docs

# Open documentation in browser
open docs/html/index.html
```

### Step 6: Deploy Binary to Orange Pi

```bash
# Copy binary to Orange Pi home directory
make install CROSS_USER=pi CROSS_HOST=orange-pi.local

# Behind a proxy? Use explicit SSH:
scp loki_app pi@orange-pi.local:/home/pi/

# Verify transfer
ssh pi@orange-pi.local ls -la loki_app
```

### Step 7: Set Executable Permissions

```bash
# On development machine:
ssh pi@orange-pi.local chmod +x /home/pi/loki_app

# Verify
ssh pi@orange-pi.local ls -la loki_app
# Should show: -rwxr-xr-x (755)
```

### Step 8: Run Application on Orange Pi

```bash
# Option 1: Via remote SSH (from dev machine)
ssh pi@orange-pi.local "cd ~ && sudo ./loki_app"

# Option 2: SSH into Orange Pi then run
ssh pi@orange-pi.local
cd ~
sudo ./loki_app

# Why sudo? GPIO/SPI/I2C access requires root privileges
```

### Step 9: Verify Initialization

You should see output like:

```
[14:23:45] using log level: 4 (DEBUG)
[14:23:45] core/system.c:45 in system_init(): Starting system initialization
[14:23:46] hal/gpio/gpio.c:32 in gpio_init(): GPIO HAL initialized
[14:23:46] hal/spi/spi.c:38 in spi_init(): SPI0 bus initialized at 40 MHz
[14:23:46] hal/spi/spi.c:48 in spi_init(): SPI1 bus initialized at 25 MHz
[14:23:46] hal/spi/spi.c:58 in spi_init(): SPI2 bus initialized at 20 MHz
[14:23:46] hal/i2c/i2c.c:52 in i2c_init(): I2C0 bus initialized at 100 kHz
[14:23:46] hal/uart/uart.c:61 in uart_init(): UART1 initialized at 115200 baud
[14:23:46] drivers/tft/tft_driver.c:92 in tft_init(): TFT display initialized
[14:23:46] drivers/tft/tft_driver.c:98 in tft_set_brightness(): Setting brightness to 80%
[14:23:46] drivers/sdcard/sdcard_driver.c:65 in sdcard_init(): SD card detected and initialized
[14:23:46] drivers/flash/flash_driver.c:48 in flash_init(): W25Q40 flash initialized (JEDEC: 0xEF4013)
[14:23:46] drivers/eeprom/eeprom_driver.c:42 in eeprom_init(): EEPROM ready
[14:23:46] drivers/flipper_uart/flipper_uart.c:35 in flipper_uart_init(): Waiting for Flipper handshake
[14:23:47] core/system.c:128 in system_init(): All subsystems initialized successfully
[14:23:47] core/main.c:45 in main(): System ready. Waiting for Flipper commands...
```

---

## Troubleshooting Deployment

### Build Issues

#### Compiler not found
```bash
arm-linux-gnueabihf-gcc: command not found

# Solution: Install cross-compiler
sudo apt-get install gcc-arm-linux-gnueabihf
```

#### Header file not found
```bash
fatal error: types.h: No such file or directory

# Solution: Ensure CFLAGS includes project directories
# In Makefile, check:
CFLAGS += -I$(PWD)/includes -I$(PWD)/config
```

#### Linking errors
```bash
undefined reference to `spi_write'

# Solution: Ensure all .c files are compiled
# Check that object files exist:
ls -la *.o   # Should have gpio.o, spi.o, i2c.o, etc.
```

### Deployment Issues

#### SSH connection refused
```bash
ssh: connect to host orange-pi.local port 22: Connection refused

# Solution 1: Find Orange Pi IP address
# From router admin page or use nmap:
nmap -p 22 192.168.1.0/24

# Solution 2: Use IP directly
make install CROSS_HOST=192.168.1.100
```

#### Permission denied on Orange Pi
```bash
pi@orange-pi: permission denied: ./loki_app

# Solution: Ensure execute permission
ssh pi@orange-pi.local chmod +x ~/loki_app
```

#### GPIO/SPI/I2C permission errors
```
/sys/class/gpio/export: Permission denied

# Solution: Run with sudo
sudo ./loki_app

# Or configure udev rules (more complex)
# Allow specific user to access GPIO without sudo
```

### Runtime Issues

#### TFT display not initializing
```
[ERROR] TFT display initialization failed

# Check 1: Wiring
- Verify SPI0 pins: 23 (SCK), 19 (MOSI), 21 (MISO), 24 (CS)
- Verify DC pin: 18 (GPIO18)
- Verify RST pin: 22 (GPIO22)
- Verify power: 3.3V to display

# Check 2: SPI bus permissions
sudo cat /dev/spidev0.0  # Should work with sudo

# Check 3: Enable SPI in Orange Pi
sudo raspi-config  # Or equivalent config tool
# Enable SPI interface
```

#### SD Card not detected
```
[ERROR] SD card not detected

# Check 1: Wiring
- Verify SPI1 pins: 29 (SCK), 31 (MOSI), 33 (MISO), 32 (CS)
- Verify card detect pin: 40 (GPIO40)
- Verify power: 3.3V to SD module

# Check 2: Card status
ssh pi@orange-pi.local "cat /proc/cmdline | grep -i sd"

# Check 3: Try with debug logging
make DEBUG=1
# Look for SD card detection messages
```

#### Flipper Zero not communicating
```
[WARN] No Flipper handshake detected

# Check 1: Wiring
- Verify UART pins: 8 (TX), 10 (RX)
- Verify Flipper is powered and connected
- TX on Orange Pi → RX on Flipper
- RX on Orange Pi → TX on Flipper

# Check 2: UART configuration
# Verify /dev/ttyS1 exists:
ssh pi@orange-pi.local "ls -la /dev/ttyS1"

# Check 3: Baud rate
# 115200 baud is correct, verify in flipper_uart.h
```

#### Application crashes with segmentation fault
```
Segmentation fault (core dumped)

# Enable debugging symbols
make DEBUG=1  # Recompile with debuggable binary

# Option 1: Run under gdb on Orange Pi
ssh pi@orange-pi.local
sudo gdb ~/loki_app
(gdb) run
# Will show crash location

# Option 2: Use gdbserver for remote debugging
# On Orange Pi:
sudo gdbserver :5005 ~/loki_app

# On dev machine:
arm-linux-gnueabihf-gdb loki_app
(gdb) target remote orange-pi.local:5005
(gdb) list
(gdb) backtrace  # Show crash stack
```

### Performance Issues

#### Application runs too slowly
```bash
# Check clock speed
ssh pi@orange-pi.local "cat /proc/cpuinfo | grep MHz"

# Profile application with perf
ssh pi@orange-pi.local "sudo apt-get install linux-tools"
ssh pi@orange-pi.local "sudo perf record ~/loki_app"
ssh pi@orange-pi.local "sudo perf report"
```

#### Display refresh too slow
```bash
# Check SPI0 clock speed (should be 40 MHz)
# In config/board_config.h:
#define SPI0_SPEED_HZ  40000000  // MHz

# Increase if supported by display:
#define SPI0_SPEED_HZ  50000000  // Try 50 MHz
make DEBUG=0  # Recompile
make install  # Redeploy
```

#### Memory usage too high
```bash
# Check memory usage on Orange Pi
ssh pi@orange-pi.local "free -h"

# Enable memory leak detection
make DEBUG=1  # Recompile
# At application exit:
# Memory report: X allocations, Y bytes leaking

# Review log output for malloc_safe failures
```

---

## Persistent Deployment

For production, you want Loki to run automatically on Orange Pi boot:

### Using systemd Service

Create `/etc/systemd/system/loki.service`:

```ini
[Unit]
Description=Loki Orange Pi Zero 2W System
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi
ExecStart=/usr/local/bin/loki_app

# Restart on failure
Restart=on-failure
RestartSec=10

# Log output
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

Install and enable:

```bash
# Copy binary to system location
sudo cp loki_app /usr/local/bin/
sudo chmod +x /usr/local/bin/loki_app

# Copy service file
sudo cp loki.service /etc/systemd/system/
sudo systemctl daemon-reload

# Enable service
sudo systemctl enable loki.service

# Start service
sudo systemctl start loki.service

# Check status
sudo systemctl status loki.service

# View logs
sudo journalctl -u loki.service -f
```

### Using cron for Periodic Execution

If not running continuously, add to crontab:

```bash
# Run every hour
0 * * * * /home/pi/loki_app

# Run every 5 minutes
*/5 * * * * /home/pi/loki_app

# Run on boot
@reboot /home/pi/loki_app
```

---

## Performance Optimization for Deployment

### Release Build
```bash
# Already configured in Makefile
make DEBUG=0  # Compiler flags: -O2

# Output: Optimized binary, ~50-70% smaller than DEBUG
```

### Reduce Binary Size
```bash
# Strip debug symbols
make DEBUG=0
arm-linux-gnueabihf-strip loki_app

# Check size reduction
ls -la loki_app
# Before: ~500 KB
# After:  ~200 KB
```

### Disable Unused Features

In `includes/types.h`, you can disable features:

```c
// If not using retry logic operations
#define HAVE_RETRY_LOGIC 1    // Set to 0 to disable

// If not using memory leak detection
#define HAVE_MEMORY_TRACKING 1  // Set to 0 to disable

// Recompile after changes
make clean && make DEBUG=0
```

### Profiling Before Deployment

Identify bottlenecks:

```bash
# On Orange Pi, compile with profiling
make DEBUG=0 CFLAGS+="-pg"

# Run application
./loki_app

# Analyze results
gprof ./loki_app gmon.out | head -30
```

---

## Rollback Procedure

If deployment fails and you need to revert:

```bash
# Stop current application
ssh pi@orange-pi.local "sudo killall loki_app"

# Revert to previous version
ssh pi@orange-pi.local "cp ~/loki_app.backup ~/loki_app"

# Or redeploy from dev machine with previous build
git checkout v0.9
make clean && make DEBUG=0
make install
```

---

## Maintenance After Deployment

### Monitor Logs
```bash
# Real-time log monitoring
ssh pi@orange-pi.local "sudo journalctl -u loki.service -f"

# Check for errors
ssh pi@orange-pi.local "dmesg | tail -20"
```

### Check Hardware Health
```bash
# Orange Pi system status
ssh pi@orange-pi.local "cat /proc/cpuinfo"
ssh pi@orange-pi.local "cat /proc/meminfo"
ssh pi@orange-pi.local "cat /proc/uptime"

# Temperature
ssh pi@orange-pi.local "sudo cat /sys/class/thermal/thermal_zone0/temp"
```

### Update Firmware
```bash
# Keep Orange Pi OS updated
ssh pi@orange-pi.local "sudo apt-get update && sudo apt-get upgrade"

# Recompile Loki if kernel changed
make clean
make DEBUG=0
make install
```

---

## Final Verification Checklist

After deployment, verify:

- [ ] Application starts without errors
- [ ] TFT display shows output (colored test pattern)
- [ ] SD card is detected and readable
- [ ] Flash JEDEC ID reads correctly (W25Q40)
- [ ] EEPROM responds to I2C reads
- [ ] Flipper Zero connects and receives commands
- [ ] Logging output shows correct timestamps
- [ ] No memory leaks displayed at shutdown
- [ ] Application survives 1-hour stress test
- [ ] Orange Pi temperature stays below 70°C under load

---

## Next Steps

Once deployment is successful:

1. **Development**: Begin application-layer development (Loki game logic, XP system)
2. **Optimization**: Profile application, optimize hot paths
3. **Testing**: Integration testing with Flipper Zero hardware
4. **Documentation**: Document deployment procedure for your team
5. **Monitoring**: Set up monitoring for production deployments
6. **Scaling**: Plan for multiple Orange Pi devices if needed

---

## Support Resources

- **Build Errors**: See [BUILD.md](BUILD.md) troubleshooting section
- **Code Issues**: See [CONTRIBUTING.md](CONTRIBUTING.md) code standards
- **Improvements**: See [IMPROVEMENTS.md](IMPROVEMENTS.md) for architecture overview
- **API Reference**: Generate with `make docs` and open `docs/html/index.html`

---

**Last Updated**: February 2026  
**Deployment Version**: v1.0  
**Status**: Ready for production
