# Loki Windows Build - Quick Reference Card

## The Problem You Had
```
C:\Users\nlane\Desktop\Loki> make install CROSS_HOST=orange-pi.local
make : The term 'make' is not recognized
```

**Why?** Windows doesn't have `make` - it's a Linux/Mac tool

## The Solution

### Use PowerShell Instead
```powershell
cd C:\Users\nlane\Desktop\Loki
.\build.ps1 -Mode release -Install
```

---

## Quick Commands

### Just Build (Debug)
```powershell
.\build.ps1
```

### Build Release (Smaller)
```powershell
.\build.ps1 -Mode release
```

### Build and Deploy to Orange Pi
```powershell
.\build.ps1 -Mode release -Install -HostName orange-pi.local
```

### Deploy to Specific IP
```powershell
.\build.ps1 -Mode release -Install -HostName 192.168.1.100
```

### With Custom Username
```powershell
.\build.ps1 -Mode release -Install -HostName 192.168.1.100 -User myuser
```

---

## Before You Run

✅ **Check 1: ARM Compiler**
```powershell
arm-linux-gnueabihf-gcc --version
```
If this fails, download from: https://developer.arm.com

✅ **Check 2: SSH Works**
```powershell
ssh pi@orange-pi.local
```
If this fails, check Orange Pi is powered on and SSH is enabled

---

## What It Does

```
.\build.ps1 -Mode release -Install
    ↓
[1] Compiles all .c files → .o files
    ↓
[2] Links .o files → loki_app binary
    ↓
[3] Checks SSH connection to Orange Pi
    ↓
[4] Copies loki_app to Orange Pi
    ↓
[5] Makes it executable (chmod +x)
    ↓
✓ Ready to run: ssh pi@orange-pi.local "sudo /tmp/loki_app"
```

---

## Output Example

Successful build shows:
```
================================================
Loki Build Script for Windows PowerShell
================================================

[>] Checking for ARM cross-compiler...
[+] Compiler found

Configuration:
  Mode: release
  Build dir: build\release
  Optimizer: -O3

[>] Creating build directories...
[+] Directories created

[>] Compiling sources...
[CC] hal/gpio/gpio.c
[CC] hal/spi/spi.c
...
[+] Successfully built loki_app (245.32 KB)

[+] Build successful
```

---

## Common Issues & Fixes

| Problem | Solution |
|---------|----------|
| `arm-linux-gnueabihf-gcc not found` | Download toolchain from https://developer.arm.com |
| `SSH connection failed` | Make sure Orange Pi is on, try: `ping orange-pi.local` |
| `Permission denied` | Use: `ssh pi@orange-pi.local "sudo /tmp/loki_app"` |
| `Cannot overwrite variable` | Use `build.ps1` not PowerShell param named `Host` |

---

## File Locations

After building:
```
C:\Users\nlane\Desktop\Loki\
├── build\debug\loki_app      (Debug binary, 500 KB)
└── build\release\loki_app    (Release binary, 200 KB)
```

---

## Three Ways to Deploy

### Way 1: One Command (Easiest) ⭐
```powershell
.\build.ps1 -Mode release -Install
```

### Way 2: Manual SCP
```powershell
scp build\release\loki_app pi@orange-pi.local:/tmp/
```

### Way 3: Use Batch Script
```batch
build.bat release orange-pi.local pi --install
```

---

## Run After Deploy

```powershell
# Option A: Run remotely
ssh pi@orange-pi.local "sudo /tmp/loki_app"

# Option B: SSH in, then run
ssh pi@orange-pi.local
sudo /tmp/loki_app
```

Output should show:
```
[14:23:45] core/main.c:45 in main(): System initializing
[14:23:46] hal/gpio/gpio.c:32: GPIO HAL initialized
[14:23:47] drivers/tft/tft_driver.c:92: TFT display initialized
```

---

## Debugging

### If build fails:
1. Check error message
2. See [QUICKSTART_WINDOWS.md](QUICKSTART_WINDOWS.md) troubleshooting
3. Check ARM compiler: `arm-linux-gnueabihf-gcc --version`

### If deployment fails:
1. Test SSH: `ssh pi@orange-pi.local`
2. Check Orange Pi is on: `ping orange-pi.local`
3. See [DEPLOYMENT.md](DEPLOYMENT.md) for details

### If run fails:
1. Make it executable: `ssh pi@orange-pi.local "chmod +x /tmp/loki_app"`
2. Run with sudo: `... sudo /tmp/loki_app`
3. Check logs: See [README.md](README.md) for expected output

---

## Learn More

- **[QUICKSTART_WINDOWS.md](QUICKSTART_WINDOWS.md)** ← Start here for Windows setup
- **[README.md](README.md)** ← Complete API reference
- **[DEPLOYMENT.md](DEPLOYMENT.md)** ← Production deployment guide
- **[FIX_GUIDE.md](FIX_GUIDE.md)** ← What was fixed and why

---

## Platform Comparison

| Platform | Command |
|----------|---------|
| Windows | `.\build.ps1 -Mode release -Install` |
| Windows (CMD) | `build.bat release orange-pi.local pi --install` |
| Mac/Linux | `make DEBUG=0 install` |
| WSL | `make DEBUG=0 install` |

---

**Need help?** See [FIX_SUMMARY.md](FIX_SUMMARY.md) for what was fixed.

**Ready?** Run: `.\build.ps1 -Mode debug`
