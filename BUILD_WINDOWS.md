# Building Loki on Windows, Mac, and Linux

This guide explains which build tool to use based on your platform.

## Platform-Specific Build Tools

### Windows Users
Choose one of the following methods:

#### Option 1: PowerShell Script (Recommended)
```powershell
# Allow script execution (first time only)
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser

# Debug build
.\build.ps1 -Mode debug

# Release build
.\build.ps1 -Mode release

# Build and install to Orange Pi
.\build.ps1 -Mode release -Install -HostName orange-pi.local -User pi
```

**Advantages**: Color-coded output, easier to read, modern PowerShell features

#### Option 2: Batch Script (CMD.exe)
```batch
REM Debug build
build.bat debug

REM Release build
build.bat release

REM Build and install
build.bat release orange-pi.local pi --install
```

**Advantages**: Works in standard CMD even if PowerShell is unavailable

### Mac/Linux Users
Use the standard Makefile:

```bash
# Debug build
make

# Release build
make DEBUG=0

# Build and install
make DEBUG=0 install CROSS_HOST=orange-pi.local CROSS_USER=pi

# Run on target
make run

# Generate documentation
make docs

# Static analysis
make analyze

# All available targets
make help
```

---

## Prerequisites

All platforms require the ARM cross-compiler:

### Windows
1. Download from: https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads
2. Extract to `C:\Program Files\arm`
3. Add to PATH: `C:\Program Files\arm\bin`
4. Verify: Open PowerShell/CMD and run:
   ```
   arm-linux-gnueabihf-gcc --version
   ```

### Mac
```bash
# Using Homebrew
brew tap ArmMbed/homebrew-formulae
brew install arm-none-eabi-gcc

# Or download from ARM website as above
```

### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install gcc-arm-linux-gnueabihf
```

---

## Build Configuration

Both Windows scripts and the Makefile support the same build modes:

### Debug Mode (Default)
```
Compilation: -g -O0 (Debugging symbols, no optimization)
Logging: Full (LOG_LEVEL=4)
Features: All enabled (logging, memory tracking, retry)
Size: Larger (~500 KB)
```

Use for: Development, debugging, testing

### Release Mode
```
Compilation: -O3 (Fully optimized)
Logging: Minimal (LOG_LEVEL=2)
Features: Only critical features
Size: Smaller (~200 KB)
```

Use for: Production deployment, performance-critical applications

---

## Deployment to Orange Pi

### From Windows PowerShell
```powershell
# Build release and deploy
.\build.ps1 -Mode release -Install -HostName orange-pi.local -User pi

# With custom IP
.\build.ps1 -Mode release -Install -HostName 192.168.1.100 -User pi
```

### From Windows CMD
```batch
build.bat release orange-pi.local pi --install
```

### From Mac/Linux
```bash
make DEBUG=0 install
make run  # Build and execute
```

---

## Troubleshooting

### "arm-linux-gnueabihf-gcc not found"
The ARM cross-compiler is not installed or not in PATH.

**Windows**:
1. Download from https://developer.arm.com
2. Add `bin` folder to Windows PATH environment variable
3. Restart terminal

**Mac/Linux**:
```bash
# Ubuntu/Debian
sudo apt-get install gcc-arm-linux-gnueabihf

# Mac
brew install arm-none-eabi-gcc
```

### SSH connection fails during install
```
[ERROR] SSH connection failed to orange-pi.local
```

**Solutions**:
1. Check Orange Pi is powered on: `ping orange-pi.local`
2. Use IP address instead: `.\build.ps1 -Host 192.168.1.100`
3. Verify SSH is enabled on Orange Pi
4. Check username (default: `pi`)

### PowerShell script execution blocked
```
cannot be loaded because running scripts is disabled on this system
```

**Solution**:
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Batch script doesn't recognize arm-linux-gnueabihf-gcc
Restart CMD after modifying PATH environment variable.

---

## Build Output

Successful build creates:

**Windows/PowerShell/Batch**:
- `build/debug/loki_app` (Debug binary)
- `build/release/loki_app` (Release binary)

**Mac/Linux**:
- `build/debug/loki_app` (Debug binary)
- `build/release/loki_app` (Release binary)

---

## Next Steps

1. **Review [BUILD.md](BUILD.md)** for detailed development guide
2. **See [DEPLOYMENT.md](DEPLOYMENT.md)** for deployment procedures
3. **Check [README.md](README.md)** for API reference
4. **Read [CONTRIBUTING.md](CONTRIBUTING.md)** for code style

---

## Build System Comparison

| Feature | Makefile (Linux/Mac) | PowerShell (Windows) | Batch (Windows) |
|---------|---|---|---|
| Platform | Linux/Mac | Windows | Windows (CMD) |
| Ease | Easy | Very Easy | Easy |
| Output | Colored | Colored | Plain |
| Install | Yes (scp) | Yes (scp) | Yes (scp) |
| Debug | Yes | Yes | Yes |
| Release | Yes | Yes | Yes |
| Docs | Yes (make docs) | No | No |

For full feature set, use Linux/Mac with Makefile or use WSL on Windows.

---

**Last Updated**: February 2026  
**Status**: All platforms supported
