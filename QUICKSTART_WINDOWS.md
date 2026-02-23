# Getting Started with Loki on Windows

## Quick Overview

The Loki project is ready to build and deploy to Orange Pi Zero 2W. Since you're on Windows, follow these steps:

## Step 1: Install ARM Cross-Compiler

### Option A: Using ARM's Official Toolchain (Recommended)

1. Go to: https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads
2. Download: **Arm GNU Toolchain** (not EABI - you want `arm-linux-gnueabihf`)
   - Look for: `arm-linux-gnueabihf` version
3. Extract to: `C:\Program Files\arm-toolchain`
4. Add to Windows PATH:
   - Search for "Edit environment variables"
   - Add: `C:\Program Files\arm-toolchain\bin`
   - Click OK, restart terminal
5. Verify:
   ```powershell
   arm-linux-gnueabihf-gcc --version
   ```

### Option B: Using Chocolatey (Easier if you have it)

```powershell
choco install gcc-arm-none-eabi
```

## Step 2: Verify SSH Connectivity

Before building, test SSH to your Orange Pi:

```powershell
# Test SSH connection
ssh pi@orange-pi.local

# Or with IP address
ssh pi@192.168.1.100

# If it works, you'll get a password prompt
# Default password for Armbian: 1234
```

If SSH fails:
- Make sure Orange Pi is powered on
- Find IP address from your router
- Check that SSH is enabled on Orange Pi

## Step 3: Build the Project

### Using PowerShell (Recommended)

```powershell
# Navigate to project directory
cd C:\Users\nlane\Desktop\Loki

# Debug build (with logging and debugging symbols)
.\build.ps1 -Mode debug

# Release build (optimized for size/speed)
.\build.ps1 -Mode release

# Build and immediately install to Orange Pi
.\build.ps1 -Mode release -Install -HostName orange-pi.local -User pi
```

### Using CMD.exe (Alternative)

```batch
cd C:\Users\nlane\Desktop\Loki

REM Debug build
build.bat debug

REM Release build  
build.bat release

REM Build and install
build.bat release orange-pi.local pi --install
```

## Step 4: Deploy to Orange Pi

After a successful build, deploy the binary:

```powershell
# Recommended: Build and install in one command
.\build.ps1 -Mode release -Install -HostName orange-pi.local -User pi

# Or deploy manually after building
scp build\release\loki_app pi@orange-pi.local:/tmp/
ssh pi@orange-pi.local "chmod +x /tmp/loki_app"
```

## Step 5: Run on Orange Pi

```powershell
# Run remotely
ssh pi@orange-pi.local "sudo /tmp/loki_app"

# Or SSH in and run locally
ssh pi@orange-pi.local
sudo /tmp/loki_app
```

You should see output like:
```
[14:23:45] core/main.c:45 in main(): System ready
[14:23:46] drivers/tft/tft_driver.c:92: TFT display initialized
[14:23:46] drivers/sdcard/sdcard_driver.c:65: SD card detected
```

## Troubleshooting

### "arm-linux-gnueabihf-gcc not found"

**Solution**: Add ARM toolchain to Windows PATH

1. Download and install ARM toolchain
2. Find location (e.g., `C:\Program Files\ARM GNU Toolchain\bin`)
3. Open Settings > Environment Variables > Edit PATH
4. Add the bin folder
5. Restart PowerShell/CMD and try again

### "SSH connection failed"

**Solution**: Verify Orange Pi connectivity

```powershell
# Check if Orange Pi is reachable
ping orange-pi.local

# If that fails, find IP address:
# 1. Check your router's DHCP client list
# 2. Or use nmap:
nmap -p 22 192.168.1.0/24

# Use IP address directly
.\build.ps1 -Mode release -Install -HostName 192.168.1.100
```

### Build succeeds but deploy fails

**SSH Issues**:
- Ensure SSH is enabled on Orange Pi
- Try: `ssh pi@orange-pi.local` (should work)
- Default password: `1234`

**Permissions**:
- Make sure you have permissions to /tmp on Orange Pi
- Or use a different path: `/home/pi/`

### "Permission denied" when running

**Solution**: Use `sudo`
```
ssh pi@orange-pi.local "sudo /tmp/loki_app"
```

GPIO/SPI/I2C access requires root privileges.

## Project Structure

Generated files after build:
```
Loki/
├── build/
│   ├── debug/
│   │   ├── loki_app         (Debug binary - larger, full symbols)
│   │   ├── hal/gpio/gpio.o
│   │   ├── drivers/tft/tft_driver.o
│   │   └── ... (all .o files)
│   └── release/
│       ├── loki_app         (Release binary - smaller, optimized)
│       └── ... (all .o files)
├── source files... (unchanged)
└── documentation...
```

## Understanding Build Modes

### Debug Mode (default)
- **Compiler flags**: `-g -O0` (debugging symbols, no optimization)
- **Binary size**: ~500 KB
- **Features**: Full logging, memory tracking, assertions
- **Log level**: DEBUG (show everything)
- **Use for**: Development, debugging, learning

### Release Mode
- **Compiler flags**: `-O3` (fully optimized)
- **Binary size**: ~200 KB  
- **Features**: Minimal logging (errors only)
- **Log level**: INFO (warnings and errors only)
- **Use for**: Production deployment, performance critical

## Deployment Options

### Option 1: One-Command Build & Deploy (Easiest)
```powershell
.\build.ps1 -Mode release -Install -HostName orange-pi.local -User pi
```
Builds and deploys in one command.

### Option 2: Build Once, Deploy Later
```powershell
# Build once
.\build.ps1 -Mode release

# Test locally or redeploy multiple times
scp build\release\loki_app pi@orange-pi.local:/tmp/
```

### Option 3: Keep in Service
Create systemd service on Orange Pi (see [DEPLOYMENT.md](DEPLOYMENT.md)):
```bash
# On Orange Pi, create /etc/systemd/system/loki.service
# Then enable auto-start on boot
sudo systemctl enable loki
```

## Next Steps After Successful Build

1. **Review [README.md](README.md)** - API reference and examples
2. **Read [BUILD.md](BUILD.md)** - Detailed development guide
3. **Check [DEPLOYMENT.md](DEPLOYMENT.md)** - Production deployment guide
4. **Study [CONTRIBUTING.md](CONTRIBUTING.md)** - Code style and extending the project
5. **Explore [FILE_REFERENCE.md](FILE_REFERENCE.md)** - Complete file inventory

## Common Commands

```powershell
# Debug build (with logs and symbols)
.\build.ps1 -Mode debug

# Release build (optimized)
.\build.ps1 -Mode release

# Build and deploy
.\build.ps1 -Mode release -Install -HostName orange-pi.local

# Deploy to specific IP
.\build.ps1 -Mode release -Install -HostName 192.168.1.50 -User pi

# SSH into Orange Pi
ssh pi@orange-pi.local

# Copy file to Orange Pi
scp myfile.txt pi@orange-pi.local:/tmp/

# Run application
ssh pi@orange-pi.local "sudo /tmp/loki_app"

# View Orange Pi system info
ssh pi@orange-pi.local "cat /proc/cpuinfo"
```

## Getting Help

- **Build errors**: See the script output for compiler/linker errors
- **SSH errors**: Test `ssh pi@orange-pi.local` directly
- **Runtime errors**: Check [DEPLOYMENT.md](DEPLOYMENT.md) troubleshooting
- **Code questions**: Review [README.md](README.md) API documentation

## Hardware Requirements

- Orange Pi Zero 2W (or compatible)
- Armbian or Orange Pi OS installed
- SSH enabled
- All hardware wiring complete (see [README.md](README.md) wiring reference)

## What's Included

The Loki project includes:

- ✅ Complete HAL (GPIO, SPI, I2C, UART, PWM)
- ✅ 5 Device drivers (TFT, SD Card, Flash, EEPROM, Flipper)
- ✅ Professional logging framework
- ✅ Memory safety utilities  
- ✅ Error recovery system
- ✅ Comprehensive documentation
- ✅ Cross-platform build scripts
- ✅ 100% ready for deployment

---

**Ready to build?** Start with:
```powershell
.\build.ps1 -Mode debug
```

Need help? Check [BUILD_WINDOWS.md](BUILD_WINDOWS.md) for detailed platform-specific guide.
