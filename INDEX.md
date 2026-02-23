# 📋 Loki Project - Complete Documentation Index

## 🚨 You Had an Issue
```
make : The term 'make' is not recognized
Exit Code 1
```
✅ **FIXED** - See below for solution

---

## 🚀 Where to Start (Pick Your Path)

### 👤 I'm a Windows User
1. **First**: Read [**QUICKSTART_WINDOWS.md**](QUICKSTART_WINDOWS.md) ← START HERE
2. **Then**: Run `.\build.ps1 -Mode debug`
3. **Deploy**: `.\build.ps1 -Mode release -Install`
4. **Reference**: Keep [**QUICK_REFERENCE.md**](QUICK_REFERENCE.md) handy

### 👤 I'm a Linux/Mac User  
1. **First**: Read [**README.md**](README.md)
2. **Then**: `make`
3. **Deploy**: `make DEBUG=0 install`
4. **Reference**: Use `Makefile` directly

### 👤 I'm Deploying to Production
1. Read [**DEPLOYMENT.md**](DEPLOYMENT.md)
2. Follow deployment checklist
3. Set up systemd service
4. Monitor with journalctl

### 👤 I'm Contributing Code
1. Read [**CONTRIBUTING.md**](CONTRIBUTING.md)
2. Follow code style guide
3. Use logging macros (not printf)
4. Add Doxygen comments

---

## 📚 Documentation by Purpose

### Getting Started
| Document | Purpose | Read Time |
|----------|---------|-----------|
| [**QUICKSTART_WINDOWS.md**](QUICKSTART_WINDOWS.md) | Windows setup guide | 10 min |
| [**QUICK_REFERENCE.md**](QUICK_REFERENCE.md) | Command cheat sheet | 5 min |
| [**README.md**](README.md) | Project overview & API docs | 15 min |

### Building & Deployment
| Document | Purpose | Read Time |
|----------|---------|-----------|
| [**build.ps1**](build.ps1) | Windows PowerShell builder | N/A |
| [**build.bat**](build.bat) | Windows CMD builder | N/A |
| [**Makefile**](Makefile) | Linux/Mac builder | N/A |
| [**BUILD.md**](BUILD.md) | Build system guide | 15 min |
| [**BUILD_WINDOWS.md**](BUILD_WINDOWS.md) | Windows platform guide | 10 min |
| [**DEPLOYMENT.md**](DEPLOYMENT.md) | Production deployment | 20 min |

### Code & Architecture
| Document | Purpose | Read Time |
|----------|---------|-----------|
| [**CONTRIBUTING.md**](CONTRIBUTING.md) | Code style & standards | 15 min |
| [**FILE_REFERENCE.md**](FILE_REFERENCE.md) | File inventory | 10 min |
| [**IMPROVEMENTS.md**](IMPROVEMENTS.md) | Feature details | 20 min |

### Fixes & Changes
| Document | Purpose | Read Time |
|----------|---------|-----------|
| [**FIX_GUIDE.md**](FIX_GUIDE.md) | What was fixed | 5 min |
| [**FIX_SUMMARY.md**](FIX_SUMMARY.md) | Technical details | 10 min |

---

## ⚡ Quick Start (3 Steps)

### Step 1: Install ARM Compiler
```powershell
# Download from: https://developer.arm.com
# Add \bin folder to Windows PATH
# Verify: arm-linux-gnueabihf-gcc --version
```

### Step 2: Build
```powershell
cd C:\Users\nlane\Desktop\Loki
.\build.ps1 -Mode release
```

### Step 3: Deploy
```powershell
.\build.ps1 -Mode release -Install -HostName orange-pi.local
```

✅ **Done!** Your app is deployed to Orange Pi

---

## 📦 What's in This Project

```
Loki/                          Complete embedded system
├── Hardware Abstraction (HAL)
│   ├── hal/gpio/              GPIO pin control
│   ├── hal/spi/               3 SPI buses
│   ├── hal/i2c/               I2C communication
│   ├── hal/uart/              Serial UART
│   └── hal/pwm/               PWM backlight
│
├── Device Drivers
│   ├── drivers/tft/           3.5" Display (480×320)
│   ├── drivers/sdcard/        SD Card storage
│   ├── drivers/flash/         W25Q40 SPI flash
│   ├── drivers/eeprom/        FT24C02A config
│   └── drivers/flipper_uart/  Flipper Zero link
│
├── Core System
│   ├── core/main.c            Entry point
│   ├── core/system.c          Init/shutdown
│   └── utils/                 Logging, memory, retry
│
├── Build System
│   ├── Makefile               Linux/Mac
│   ├── build.ps1              Windows PowerShell
│   ├── build.bat              Windows CMD
│   └── Doxyfile               Documentation
│
└── Documentation (You Are Here)
    ├── README.md              Overview & API
    ├── QUICKSTART_WINDOWS.md  Windows setup
    ├── BUILD.md               Build guide
    ├── DEPLOYMENT.md          Production guide
    ├── CONTRIBUTING.md        Code standards
    ├── FILE_REFERENCE.md      File inventory
    ├── IMPROVEMENTS.md        Features
    ├── FIX_GUIDE.md          What's fixed
    └── FIX_SUMMARY.md        Technical details
```

---

## 🛠️ Build Commands Quick Reference

### Windows PowerShell
```powershell
.\build.ps1                              # Debug build
.\build.ps1 -Mode release                # Release build
.\build.ps1 -Mode release -Install       # Build + deploy
```

### Windows CMD
```batch
build.bat debug                          # Debug build
build.bat release                        # Release build
build.bat release [host] [user] --install # Build + deploy
```

### Linux/Mac
```bash
make                                     # Debug build
make DEBUG=0                             # Release build
make install                             # Deploy
make run                                 # Build and run
make docs                                # Generate API docs
make analyze                             # Code analysis
```

---

## 📊 Project Statistics

| Metric | Value |
|--------|-------|
| **Total Files** | 40+ |
| **Lines of Code** | ~6,400 |
| **Documentation** | 12 guides |
| **Build Scripts** | 3 (Makefile, build.ps1, build.bat) |
| **Hardware Support** | 5 device drivers |
| **Platforms** | Linux, Mac, Windows |
| **Status** | ✅ Production Ready |

---

## ✨ Features

- ✅ Complete HAL for Orange Pi Zero 2W
- ✅ Professional logging system (5 levels)
- ✅ Memory safety with leak detection  
- ✅ Automatic error recovery
- ✅ Cross-platform builds
- ✅ API documentation (Doxygen)
- ✅ Static analysis integration
- ✅ Comprehensive guides
- ✅ SSH deployment automation
- ✅ Windows PowerShell + Batch support

---

## 🎯 Common Tasks

### Task: Build in Debug Mode
```powershell
.\build.ps1 -Mode debug
```
**Output**: `build/debug/loki_app` (full debugging, logs)

### Task: Build for Production
```powershell
.\build.ps1 -Mode release
```
**Output**: `build/release/loki_app` (optimized, smaller)

### Task: Deploy to Orange Pi
```powershell
.\build.ps1 -Mode release -Install
```
**Does**: Build → Verify SSH → Copy → Make executable

### Task: SSH into Orange Pi
```powershell
ssh pi@orange-pi.local
```
**Or**: `ssh pi@192.168.1.100` (if DHCP fails)

### Task: Run App on Orange Pi
```powershell
ssh pi@orange-pi.local "sudo /tmp/loki_app"
```

### Task: View Live Logs
```bash
# On Orange Pi
tail -f /tmp/loki_app.log

# Or in systemd
sudo journalctl -u loki -f
```

### Task: Generate API Docs (Linux/Mac only)
```bash
make docs
# Open: docs/html/index.html
```

---

## ❓ FAQ

**Q: Why do I need ARM toolchain?**
A: Orange Pi runs ARM CPU, so you need ARM compiler to compile for it.

**Q: Where do I get the ARM toolchain?**
A: https://developer.arm.com - Look for "GNU Toolchain"

**Q: Can I build on Windows?**
A: Yes! Use `build.ps1` or `build.bat` (new Windows support added)

**Q: How do I deploy?**
A: Use `.\build.ps1 -Install` or `make install` to copy binary via SSH

**Q: What if SSH fails?**
A: Check Orange Pi is on, try `ping orange-pi.local`, or use IP address

**Q: Will it work on my macOS?**
A: Yes! Use the standard `make` command

**Q: Can I use this in production?**
A: Yes! Follow [DEPLOYMENT.md](DEPLOYMENT.md) for systemd setup

---

## 🔗 External Resources

| Resource | Purpose |
|----------|---------|
| [ARM Developer](https://developer.arm.com) | Get cross-compiler |
| [Orange Pi](http://orangepi.org) | Hardware info |
| [Armbian](https://www.armbian.com) | Orange Pi OS |
| [Flipper Zero](https://flipperzero.one) | Target device |

---

## 📞 Getting Help

| Problem | Solution |
|---------|----------|
| **Can't build** | Read [QUICKSTART_WINDOWS.md](QUICKSTART_WINDOWS.md) |
| **SSH fails** | See [DEPLOYMENT.md](DEPLOYMENT.md#troubleshooting-deployment) |
| **App crashes** | Check [README.md](README.md#troubleshooting) |
| **Code style** | Review [CONTRIBUTING.md](CONTRIBUTING.md) |
| **How it works** | See [FILE_REFERENCE.md](FILE_REFERENCE.md) |

---

## ✅ Status

| Component | Status |
|-----------|--------|
| Windows Build | ✅ Fixed |
| Linux Build | ✅ Working |
| Mac Build | ✅ Working |
| Documentation | ✅ Complete |
| Cross-Compilation | ✅ Ready |
| Deployment | ✅ Automated |

---

## 🚀 Next Steps

1. **Install ARM toolchain** from https://developer.arm.com
2. **Read [QUICKSTART_WINDOWS.md](QUICKSTART_WINDOWS.md)** for detailed setup
3. **Build**: `.\build.ps1 -Mode debug`
4. **Deploy**: `.\build.ps1 -Mode release -Install`
5. **Enjoy!** Your Loki system is running! 🎉

---

**Last Updated**: February 2026
**Status**: ✅ Production Ready
**Next Action**: Start with [QUICKSTART_WINDOWS.md](QUICKSTART_WINDOWS.md)
