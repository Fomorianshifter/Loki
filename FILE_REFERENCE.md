# Loki Project File Reference

Complete inventory of all files in the Loki Orange Pi Zero 2W embedded systems project.

## Configuration Files

### [config/pinout.h](config/pinout.h)
**Orange Pi Zero 2W GPIO Pin Definitions**
- All 40 GPIO pins with hardware pin numbers
- SPI0/1/2 bus pin assignments (SCK, MOSI, MISO, CS)
- I2C0 pin definitions (SDA, SCL)
- UART1 pin definitions (TX, RX)
- PWM pin definitions (backlight control)
- Control pins (TFT DC, RST, SD detect, etc.)
- Interrupt pin definitions

### [config/board_config.h](config/board_config.h)
**Board-level Configuration Parameters**
- Voltage specifications (3.3V, 5V)
- Clock frequencies (SPI speeds, I2C speed, UART baud)
- Timing parameters (debounce delays, timeouts)
- Capacitor/resistor values for circuit design
- Power consumption specs
- Environmental operating conditions
- Device-specific limits

### [includes/types.h](includes/types.h)
**Shared Type Definitions**
- `hal_status_t` enum for return codes
- Color type: `color_t` for RGB565 16-bit colors
- Various integer types: uint8_t, uint16_t, uint32_t, etc.
- Preprocessor magic for cross-platform compatibility
- RGB565 color macro: `RGB565(r, g, b)`
- Color constants (BLACK, WHITE, RED, GREEN, BLUE, etc.)
- Assert macros for DEBUG/RELEASE builds

---

## Hardware Abstraction Layer (HAL)

### [hal/gpio/gpio.h](hal/gpio/gpio.h), [hal/gpio/gpio.c](hal/gpio/gpio.c)
**GPIO Pin Control Interface**
- `gpio_configure()` - Set pin mode (input/output) and pull resistors
- `gpio_set()` - Write high/low to output pins
- `gpio_read()` - Read input pin state
- `gpio_toggle()` - Flip output pin
- `gpio_wait_for_interrupt()` - Block until edge detected
- `gpio_configure_interrupt()` - Set up edge/level triggered interrupts
- Logging integration with LOG_DEBUG macros

### [hal/spi/spi.h](hal/spi/spi.h), [hal/spi/spi.c](hal/spi/spi.c)
**SPI Multi-Bus Interface (SPI0, SPI1, SPI2)**
- `spi_init()` - Initialize SPI bus with clock speed
- `spi_write()` - Send data on bus (MOSI only)
- `spi_read()` - Receive data from bus (MISO only)
- `spi_transfer()` - Full-duplex simultaneous send/receive
- `spi_configure_cs()` - Set chip select pin behavior
- Bus selection for SPI0 (40 MHz TFT), SPI1 (25 MHz SD), SPI2 (20 MHz Flash)

### [hal/i2c/i2c.h](hal/i2c/i2c.h), [hal/i2c/i2c.c](hal/i2c/i2c.c)
**I2C Communication (I2C0)**
- `i2c_init()` - Initialize I2C bus at 100 kHz
- `i2c_write()` - Send data to I2C slave
- `i2c_read()` - Receive data from I2C slave
- `i2c_write_read()` - Send register address then read response (common pattern)
- `i2c_write_register()` - Write single register with value
- `i2c_read_register()` - Read single register
- I2C0 dedicated to EEPROM at 0x50 address

### [hal/uart/uart.h](hal/uart/uart.h), [hal/uart/uart.c](hal/uart/uart.c)
**Serial UART with Ring Buffer (UART1)**
- `uart_init()` - Initialize UART1 at 115200 baud
- `uart_write()` - Send data synchronously
- `uart_read()` - Read available data
- `uart_available()` - Check if data waiting in buffer
- `uart_on_data_received()` - Register callback for incoming data
- Ring buffer for interrupt-driven reception
- Used for Flipper Zero bidirectional communication

### [hal/pwm/pwm.h](hal/pwm/pwm.h), [hal/pwm/pwm.c](hal/pwm/pwm.c)
**PWM Output for Brightness Control**
- `pwm_init()` - Initialize PWM on specified GPIO pin
- `pwm_set_frequency()` - Set PWM frequency (typically 1 kHz)
- `pwm_set_duty()` - Set duty cycle 0-100%
- `pwm_enable()`/`pwm_disable()` - Control PWM output
- Used for TFT display backlight brightness: Pin 7 (PWM)

---

## Device Drivers

### [drivers/tft/tft_driver.h](drivers/tft/tft_driver.h), [drivers/tft/tft_driver.c](drivers/tft/tft_driver.c)
**3.5" TFT Display Driver (ILI9488, 480×320)**
- `tft_init()` - Initialize display controller and SPI0
- `tft_clear()` - Fill entire screen with black
- `tft_fill_rect()` - Draw filled rectangle with color
- `tft_write_pixels()` - Write pixel data to frame buffer region
- `tft_set_brightness()` - Control backlight PWM (0-100%)
- `tft_set_rotation()` - Portrait/landscape orientation
- `tft_sleep()`/`tft_wake()` - Power management
- SPI0 interface (40 MHz), DC/RST control pins
- Color mode: RGB565 16-bit

### [drivers/sdcard/sdcard_driver.h](drivers/sdcard/sdcard_driver.h), [drivers/sdcard/sdcard_driver.c](drivers/sdcard/sdcard_driver.c)
**SD Card Storage Interface (SPI Mode)**
- `sdcard_init()` - Detect and initialize SD card on SPI1
- `sdcard_detect()` - Check if card inserted (GPIO pin)
- `sdcard_read_sector()` - Read 512-byte sector(s) from card
- `sdcard_write_sector()` - Write 512-byte sector(s) to card
- `sdcard_get_info()` - Retrieve card capacity and type
- SPI1 interface (25 MHz), CS on Pin 32
- 6-pin push-pull module with detect pin
- Supports both SDHC and SDXC cards

### [drivers/flash/flash_driver.h](drivers/flash/flash_driver.h), [drivers/flash/flash_driver.c](drivers/flash/flash_driver.c)
**W25Q40 SPI Flash Memory (4 Mbit)**
- `flash_init()` - Initialize flash chip and verify JEDEC ID
- `flash_read()` - Read data from any address (no block boundary)
- `flash_write()` - Write data (requires sector erase first)
- `flash_erase_sector()` - Erase 4 KB sector
- `flash_erase_block()` - Erase 64 KB block (faster bulk erase)
- `flash_get_jedec_id()` - Read manufacturer/device ID
- `flash_write_protection()` - Lock/unlock sectors
- SPI2 interface (20 MHz), CS on Pin 15
- Used for persistent Loki credit storage

### [drivers/eeprom/eeprom_driver.h](drivers/eeprom/eeprom_driver.h), [drivers/eeprom/eeprom_driver.c](drivers/eeprom/eeprom_driver.c)
**FT24C02A I2C EEPROM (256 Bytes)**
- `eeprom_init()` - Initialize I2C and verify device
- `eeprom_read()` - Read configuration bytes
- `eeprom_write()` - Write configuration (page-aligned)
- `eeprom_erase()` - Clear all EEPROM to 0xFF
- I2C0 interface (100 kHz), address 0x50
- Page size: 16 bytes
- 1 million write cycle endurance

### [drivers/flipper_uart/flipper_uart.h](drivers/flipper_uart/flipper_uart.h), [drivers/flipper_uart/flipper_uart.c](drivers/flipper_uart/flipper_uart.c)
**Flipper Zero Bidirectional Communication**
- `flipper_uart_init()` - Initialize UART1 and handshake protocol
- `flipper_send_message()` - Send command with XOR checksum
- `flipper_receive_message()` - Wait for and parse incoming message
- `flipper_available()` - Check if message waiting in buffer
- `flipper_on_data_received()` - Register callback for messages
- Command codes (ACK, NACK, HELLO, STATE_UPDATE, SEND_DATA, CONTROL, etc.)
- UART1 interface (115200 baud), RX on Pin 10, TX on Pin 8
- XOR checksum verification for data integrity

---

## Utility Libraries

### [utils/log.h](utils/log.h), [utils/log.c](utils/log.c)
**Centralized Logging Framework (5 Levels)**
- `LOG_CRITICAL()` - System failures, unrecoverable errors (red)
- `LOG_ERROR()` - Failed operations, recoverable issues (red)
- `LOG_WARN()` - Potential problems, unusual conditions (yellow)
- `LOG_INFO()` - Normal operations, state changes (blue)
- `LOG_DEBUG()` - Detailed diagnostic information (green)
- `log_init()` - Initialize logging system
- `log_set_level()` - Filter logs by minimum severity (runtime configurable)
- Auto-tracking of source file, line number, function name
- Millisecond-precision timestamps
- ANSI color codes for terminal readability

### [utils/memory.h](utils/memory.h), [utils/memory.c](utils/memory.c)
**Safe Memory Management with Leak Detection**
- `malloc_safe()` - Allocate memory with error checking
- `calloc_safe()` - Allocate zeroed memory
- `free_safe()` - Free and nullify pointer (prevents use-after-free)
- `memory_init()` - Initialize tracking table (DEBUG mode only)
- `memory_report()` - Print leak report at shutdown
- Tracks all allocations in DEBUG mode with:
  - Allocation size
  - Allocating function/file/line
  - Time since allocation
  - Total leaked bytes if not freed
- Zero overhead in RELEASE mode

### [utils/retry.h](utils/retry.h), [utils/retry.c](utils/retry.c)
**Automatic Retry Logic with Exponential Backoff**
- `RETRY()` macro - Transparent retry wrapper for function calls
- **RETRY_AGGRESSIVE**: 10 attempts, 1-100 ms backoff
- **RETRY_BALANCED**: 5 attempts, 2-50 ms backoff (recommended)
- **RETRY_CONSERVATIVE**: 3 attempts, 5-20 ms backoff
- **RETRY_NONE**: No retry, fail immediately
- Exponential backoff: delay = base * 2^attempt
- Jitter added to prevent bus flooding
- Retryable error detection (timeout, busy, etc.)
- Non-retryable errors fail immediately (invalid param, not initialized)

---

## Core System

### [core/system.h](core/system.h), [core/system.c](core/system.c)
**Unified System Initialization and Shutdown**
- `system_init()` - Initialize all subsystems in correct order:
  - Logging system
  - Memory tracking
  - GPIO HAL
  - SPI0/1/2 buses
  - I2C0 bus
  - UART1 port
  - TFT display
  - SD card
  - Flash memory
  - EEPROM
  - Flipper UART
- `system_shutdown()` - Graceful shutdown in reverse order
- `system_print_status()` - Display per-component initialization status
- Integrated logging of all initialization steps
- Memory tracking enabled in DEBUG builds

### [core/main.c](core/main.c)
**Application Entry Point and Hardware Tests**
- `main()` - Initialize system and enter event loop
- TFT Display Test:
  - Clear screen
  - Draw colored rectangles
  - Set brightness
- EEPROM Test:
  - Write test data
  - Read and verify
- Flash Memory Test:
  - Read JEDEC ID
- Flipper UART Test:
  - Detect connection
  - Send/receive messages
- Event loop waiting for Flipper commands
- Graceful shutdown with status reporting

---

## Build & Documentation

### [Makefile](Makefile)
**Build System with Multiple Targets**
- `make` (all) - Compile debug build
- `make DEBUG=0` - Compile release/optimized build
- `make DEBUG=1` - Compile debug build with all features
- `make install` - Copy binary to Orange Pi via SCP
- `make run` - Build and execute on target
- `make test` - Compile with test flags enabled
- `make docs` - Generate Doxygen HTML documentation
- `make analyze` - Run cppcheck static analysis
- `make size` - Show binary size breakdown
- `make info` - Display build configuration
- `make clean` - Remove build artifacts (.o files)
- `make clean-all` - Remove all generated files (binary, docs)
- `make help` - Show all available targets

Cross-compilation setup:
- ARM compiler: `arm-linux-gnueabihf-gcc`
- CFLAGS with warning levels, optimization
- DEBUG mode: `-g3 -O0 -DDEBUG=1`
- RELEASE mode: `-O2 -DNDEBUG`

### [Doxyfile](Doxyfile)
**API Documentation Generation Configuration**
- Input directories: config/, hal/, drivers/, utils/, core/, includes/
- HTML output: docs/html/
- Features enabled:
  - Function call graphs
  - Caller graphs
  - Dependency diagrams
  - Full-text search
  - Mobile-responsive layout
- Project name: "Loki Orange Pi Zero 2W"
- Generate navigation tree and search engine

---

## Documentation

### [README.md](README.md)
**Main Project Overview and API Reference**
- Project overview and key features
- Complete project structure diagram
- Hardware connections and wiring reference
- Quick start and build instructions
- Code examples (logging, memory, retry, TFT, Flipper)
- API reference table for all components
- Status codes and color definitions
- Flipper Zero protocol documentation
- Performance specifications
- Troubleshooting guide
- License information

### [BUILD.md](BUILD.md)
**Comprehensive Build and Development Guide**
- Build system overview
- Cross-compilation setup for Orange Pi
- Logging system documentation with examples
- Memory management and leak detection usage
- Retry logic strategies and examples
- Doxygen documentation generation
- Static analysis integration
- Binary size optimization
- Performance profiling with Linux tools
- Debugging techniques
- Troubleshooting common issues

### [CONTRIBUTING.md](CONTRIBUTING.md)
**Code Style and Contribution Guidelines**
- Code naming conventions:
  - Functions: `subsystem_action()`
  - Variables: `snake_case`
  - Constants: `UPPER_CASE`
  - Macros: `UPPER_CASE`
- Formatting standards (indentation, line length, comments)
- Doxygen documentation format for public functions
- Development workflow (branching, testing, code review)
- Driver implementation template with example
- Testing checklist before committing
- Performance and resource considerations
- Git commit message standards

### [IMPROVEMENTS.md](IMPROVEMENTS.md)
**Documentation of All 8 Project Enhancements**
- Enhanced Build System (12 targets, debug/release modes)
- Centralized Logging Framework (5 levels, auto source tracking)
- Safe Memory Management (leak detection)
- Automatic Retry Logic (3 strategies, exponential backoff)
- Doxygen Documentation (auto-generated from source)
- Development Guides (BUILD.md, CONTRIBUTING.md)
- Static Code Analysis (cppcheck integration)
- Binary Size Reporting
- Before/after code comparison
- Implementation timeline
- Future enhancement recommendations
- Code review checklist

### [.github/copilot-instructions.md](.github/copilot-instructions.md)
**Setup Checklist and Project Status**
- Verification checklist of completed components
- HAL driver status (GPIO, SPI, I2C, UART, PWM)
- Device driver status (TFT, SD, Flash, EEPROM, Flipper)
- Core system files status
- Next steps and optional enhancements
- Build and run instructions
- Project status summary

---

## Summary Statistics

### File Inventory
| Category | Count | Purpose |
|----------|-------|---------|
| Configuration | 3 | Pins, board params, common types |
| HAL | 10 | 5 bus drivers (header + implementation) |
| Drivers | 10 | 5 device drivers (header + implementation) |
| Utilities | 6 | 3 utility libraries (header + implementation) |
| Core | 3 | System init, main app |
| Build | 2 | Makefile, Doxygen config |
| Documentation | 5 | README, BUILD, CONTRIBUTING, IMPROVEMENTS, setup |
| **Total** | **39** | **Complete embedded system project** |

### Lines of Code (Estimate)
| Component | LOC | Purpose |
|-----------|-----|---------|
| HAL (5 drivers) | ~1,500 | Hardware abstraction |
| Device drivers (5) | ~2,000 | Device-specific implementations |
| Utilities (3 libs) | ~600 | Logging, memory, retry |
| Core system | ~300 | Initialization, main |
| Documentation | ~2,000 | Guides and references |
| **Total** | **~6,400** | **Production-ready system** |

---

## Architecture Overview

```
User Application
    ↓
Core System (system.h/c)
    ↓
┌─────────────────────────────────────┐
│      Device Drivers (drivers/)      │
│  TFT  SD  Flash  EEPROM  Flipper   │
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│   Hardware Abstraction Layer (hal/) │
│  GPIO  SPI0/1/2  I2C  UART  PWM    │
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│    Orange Pi Zero 2W Hardware       │
│ ARM Cortex-A7 + peripherals         │
└─────────────────────────────────────┘

Utilities (utils/) - Cross-cutting concerns
├── Logging (log.h/c)
├── Memory Management (memory.h/c)
└── Retry Logic (retry.h/c)

Configuration (config/) - Hardware parameters
├── pinout.h
└── board_config.h

Shared Types (includes/types.h)
```

---

## Getting Started

1. **Review Structure**: Read this file to understand layout
2. **Review README.md**: Overview and API reference
3. **Review BUILD.md**: Build instructions and system setup
4. **Compile**: `make` to create debug build
5. **Deploy**: `make install` to copy to Orange Pi
6. **Test**: `make run` to execute on target
7. **Extend**: Follow CONTRIBUTING.md when adding features

---

## File Access Quick Reference

**I want to...**
- Learn about available functions → Look in `hal/{gpio,spi,i2c,uart,pwm}/` headers
- Understand device protocols → Read `drivers/{tft,sdcard,flash,eeprom,flipper_uart}/` headers
- Set log level at runtime → Use `log_set_level()` from `utils/log.h`
- Allocate memory safely → Use `malloc_safe()` from `utils/memory.h`
- Make reliable bus calls → Use `RETRY()` macro from `utils/retry.h`
- See code examples → Check `core/main.c` or README.md
- Build the project → Follow Makefile targets or BUILD.md
- Contribute code → Read CONTRIBUTING.md
- Generate API docs → Run `make docs`, open `docs/html/index.html`
- Check code quality → Run `make analyze`
- Monitor binary size → Run `make size`

---

**Last Updated**: February 2026  
**Project Status**: ✅ Production Ready  
**Total Files**: 39  
**Estimated LOC**: 6,400
