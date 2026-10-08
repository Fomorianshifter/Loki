# Loki Project File Reference

Complete inventory of the Loki embedded systems project. All source files live in the repository root (flat layout).

## Configuration Files

### [pinout.h](pinout.h)
**GPIO Pin Definitions**
- SPI/I2C/UART bus pin assignments (SCK, MOSI, MISO, CS, SDA, SCL, TX, RX)
- Control pins (TFT DC, TFT RST, TFT backlight, SD detect)
- PWM channel assignments

### [board_config.h](board_config.h)
**Board-level Configuration Parameters**
- Board identification (name, version, model)
- Power and voltage specifications
- Device frequencies (TFT 40 MHz SPI, SD 25 MHz, Flash 20 MHz, EEPROM 100 kHz I2C)
- Memory capacities and page/sector sizes
- UART settings (115200 baud for Flipper)
- Timing constants and error handling flags

### [config.h](config.h)
**Master Configuration Entry Point**
- Aggregates `board_config.h` and `pinout.h`
- Single include for any file needing configuration values

### [types.h](types.h)
**Shared Type Definitions**
- `hal_status_t` return codes (HAL_OK, HAL_ERROR, HAL_TIMEOUT, ...)
- SPI/I2C/UART/GPIO/PWM configuration structs
- `color_t` (RGB565 16-bit) and the `RGB565(r, g, b)` macro
- Standard color constants (COLOR_BLACK, COLOR_WHITE, ...)

---

## Hardware Abstraction Layer (HAL)

### [gpio.h](gpio.h), [gpio.c](gpio.c)
**GPIO Pin Control Interface**
- `gpio_init()` / `gpio_deinit()` - Subsystem lifecycle
- `gpio_configure()` - Set pin mode and pull resistors
- `gpio_set()` / `gpio_read()` / `gpio_toggle()` - Pin I/O

### [spi.h](spi.h), [spi.c](spi.c)
**SPI Multi-Bus Interface**
- `spi_init()` / `spi_deinit()` - Bus lifecycle with `spi_config_t`
- `spi_write()` / `spi_read()` / `spi_transfer()` - Data movement
- Used by TFT (SPI0), SD card (SPI1), and Flash (SPI2)

### [i2c.h](i2c.h), [i2c.c](i2c.c)
**I2C Communication**
- `i2c_init()` / `i2c_deinit()`
- `i2c_write()` / `i2c_read()` / `i2c_write_read()`
- Dedicated to the EEPROM at address 0x50

### [uart.h](uart.h), [uart.c](uart.c)
**Serial UART**
- `uart_init()` / `uart_deinit()` with `uart_config_t`
- `uart_send()` / `uart_receive()` / `uart_receive_byte()`
- `uart_available()` - Bytes waiting in the RX buffer
- Used by the Flipper Zero link (UART1, 115200 baud)

### [pwm.h](pwm.h), [pwm.c](pwm.c)
**PWM Output**
- `pwm_init()` / `pwm_deinit()`
- `pwm_set_duty()` / `pwm_enable()` / `pwm_disable()`
- Drives the TFT backlight brightness

---

## Device Drivers

### [tft_driver.h](tft_driver.h), [tft_driver.c](tft_driver.c)
**3.5" TFT Display Driver (ILI9488, 480×320)**
- `tft_init()` / `tft_deinit()` - Controller init via SPI0
- `tft_write_pixels()` - Stream an RGB565 region
- `tft_fill_rect()` / `tft_clear()` - Solid fills
- `tft_set_brightness()` / `tft_set_rotation()`

### [sdcard_driver.h](sdcard_driver.h), [sdcard_driver.c](sdcard_driver.c)
**SD Card Storage (SPI mode)**
- `sdcard_init()` / `sdcard_deinit()`
- `sdcard_read_sector()` / `sdcard_write_sector()` - 512-byte sectors
- `sdcard_get_info()` - Capacity and card type

### [flash_driver.h](flash_driver.h), [flash_driver.c](flash_driver.c)
**W25Q40 SPI Flash (4 Mbit)**
- `flash_init()` / `flash_deinit()`
- `flash_read()` / `flash_write()`
- `flash_erase_sector()` / `flash_erase_block()`
- `flash_get_jedec_id()` - Chip identification
- Used for persistent Loki credit storage

### [eeprom_driver.h](eeprom_driver.h), [eeprom_driver.c](eeprom_driver.c)
**FT24C02A I2C EEPROM (256 bytes)**
- `eeprom_init()` / `eeprom_deinit()`
- `eeprom_read()` / `eeprom_write()` - Page-aligned writes

### [flipper_uart.h](flipper_uart.h), [flipper_uart.c](flipper_uart.c)
**Flipper Zero Bidirectional Protocol**
- `flipper_uart_init()` / `flipper_uart_deinit()` - Init + HELLO handshake
- `flipper_send_message()` / `flipper_receive_message()` - Packet I/O with XOR checksum
- `flipper_available()` - Incoming data check
- Payloads allocated with `malloc_safe()` (tracked in DEBUG builds)

---

## Graphics & Animation

### [dragon_anim.h](dragon_anim.h), [dragon_anim.c](dragon_anim.c)
**Procedural Dragon Animation Engine**
- Fully procedural: no stored bitmaps or sprite sheets
- Domain-warped aurora plasma sky with twinkling stars
- Dragon built from a capsule-SDF chain on a Lissajous flight path
  - Serpentine body undulation with per-segment phase lag
  - Anatomical features: snout, jaw, twin horns, dorsal spikes, glowing eyes
  - Analytic shading: SDF-gradient normals, key light, cool rim light, fire belly glow
- Additive fire glow with breathing pulse and flicker
- 24 ember particles with deterministic seeds, curl wobble, and life-cycle fade
- Renders in scanline tiles (`DRAGON_TILE_LINES`) to bound RAM use
- `dragon_anim_init()` / `dragon_anim_play()` / `dragon_anim_render_frame()` / `dragon_anim_deinit()`

---

## Utility Libraries

### [log.h](log.h), [log.c](log.c)
**Centralized Logging (5 Levels)**
- `LOG_CRITICAL()` / `LOG_ERROR()` / `LOG_WARN()` / `LOG_INFO()` / `LOG_DEBUG()`
- `log_init()` / `log_deinit()` / `log_set_level()`
- Automatic file, line, and function capture

### [memory.h](memory.h), [memory.c](memory.c)
**Safe Memory Management with Leak Detection**
- `malloc_safe()` / `calloc_safe()` / `free_safe()`
- `memory_get_usage()` / `memory_report()` (DEBUG builds)
- Zero overhead in release builds

### [retry.h](retry.h), [retry.c](retry.c)
**Automatic Retry with Exponential Backoff**
- `RETRY()` macro wrapping any `hal_status_t` call
- Strategies: RETRY_AGGRESSIVE, RETRY_BALANCED, RETRY_CONSERVATIVE, RETRY_NONE

---

## Core System

### [system.h](system.h), [system.c](system.c)
**Unified System Initialization and Shutdown**
- `system_init()` - GPIO, TFT, SD, Flash, EEPROM, Flipper UART in order
- `system_shutdown()` - Reverse-order teardown with memory report
- `system_print_status()` - Per-subsystem status table

### [main.c](main.c)
**Application Entry Point**
- Startup banner and logging setup
- Signal handlers for graceful shutdown (SIGINT/SIGTERM)
- Dragon boot animation via `play_dragon_boot_animation()`
- Hardware self-tests (Flash JEDEC ID, EEPROM read/write verify, Flipper link)
- Main loop receiving Flipper commands with ACK responses

---

## Build & Hardware Design

### [Makefile](Makefile)
**Cross-compilation Build System (Linux/Mac)**
- `make` / `make DEBUG=0` - Debug / release builds
- `make install` / `make run` - Deploy to Orange Pi over SSH
- `make test` / `make analyze` / `make docs` / `make size` / `make info`
- `make clean` / `make clean-all`

### [build.ps1](build.ps1)
**Windows PowerShell Build Script**
- `-Mode debug|release`, `-Install`, `-HostName`, `-User`
- Color-coded output, wraps the ARM cross-compiler

### [Doxyfile](Doxyfile)
**API Documentation Configuration**
- Generate with `make docs` (requires Doxygen)

### [OrangePiZero2W_Loki.kicad_sch](OrangePiZero2W_Loki.kicad_sch), [OrangePiZero2W_Loki.kicad_pcb](OrangePiZero2W_Loki.kicad_pcb)
**KiCad Schematic and PCB Layout**
- Custom carrier board design for the Orange Pi Zero 2W

---

## Documentation

| File | Purpose |
|------|---------|
| [README.md](README.md) | Project overview, architecture, build basics |
| [BUILD.md](BUILD.md) | Build system, logging, memory, retry, profiling guide |
| [BUILD_WINDOWS.md](BUILD_WINDOWS.md) | Windows/Mac/Linux build tool guide |
| [QUICKSTART_WINDOWS.md](QUICKSTART_WINDOWS.md) | Step-by-step Windows setup walkthrough |
| [DEPLOYMENT.md](DEPLOYMENT.md) | Production deployment and troubleshooting |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Code style and contribution guidelines |
| [FILE_REFERENCE.md](FILE_REFERENCE.md) | This file |

---

## Architecture Overview

```
User Application (main.c)
    ↓
Core System (system.c)          Graphics (dragon_anim.c)
    ↓                                  ↓
┌─────────────────────────────────────────────┐
│ Device Drivers                              │
│ TFT  SD Card  Flash  EEPROM  Flipper UART   │
└─────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────┐
│ Hardware Abstraction Layer                  │
│ GPIO  SPI  I2C  UART  PWM                   │
└─────────────────────────────────────────────┘
    ↓
Orange Pi Zero 2W / Raspberry Pi Zero W hardware

Utilities (cross-cutting): log.c, memory.c, retry.c
Configuration: config.h → board_config.h + pinout.h
```
