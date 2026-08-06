# Loki

Loki is an embedded C project for a dragon-themed interactive device that runs on a single-board computer and talks to attached hardware such as a TFT display, SD card, flash memory, EEPROM, and a Flipper Zero over UART.

## Architecture Overview

Loki is the master embedded systems framework for this ecosystem. It provides reusable HAL modules, device drivers, runtime orchestration, and Python-side integration points that downstream projects can reuse.

The intended model is:

- **Loki (master):** source of truth for embedded framework capabilities
- **FullStack (dependent):** integrates Loki into broader application workflows
- **BlackSmith (dependent):** integrates Loki for project-specific system builds

## Master/Submodule Relationship

Loki stays authoritative while dependent repositories reference Loki as a pinned Git submodule.

- FullStack and BlackSmith include Loki under a dedicated directory (commonly `loki/`)
- each dependent repo pins Loki to a specific commit for reproducible builds
- upgrades happen intentionally by moving the pinned Loki commit forward after validation

See `/SUBMODULE_GUIDE.md` for full setup and day-to-day workflow examples.

## Project Structure

Current Loki repository layout:

```text
Loki/
├── core/                 # framework orchestration and runtime layers
├── drivers/              # hardware/peripheral drivers
├── hal/                  # HAL modules (GPIO, SPI, I2C, UART, PWM)
├── plugins/              # Python plugin extensions
├── config/               # configuration helpers and generated/config assets
├── utils/                # utility helpers and scripts
├── README.md
├── SUBMODULE_GUIDE.md
├── BUILD.md
├── DEPLOYMENT.md
└── Makefile
```

Example dependent-project shape (outside this repo):

```text
FullStack-or-BlackSmith/
├── loki/                 # Git submodule -> Fomorianshifter/Loki
└── <project-specific files>
```

## Dependency Management

Loki dependency behavior is designed for compatibility and predictable integration:

- **Version pinning:** dependent repos pin Loki at a specific commit SHA (or approved tag)
- **Compatibility checks:** update Loki pins only after build/test validation in the dependent repo
- **Coordinated updates:** treat Loki version bumps as explicit changes with release notes and review

Use the compatibility guide for recommended pinning and rollout patterns.

## Quick Links

- **Submodule guide:** [`SUBMODULE_GUIDE.md`](SUBMODULE_GUIDE.md)
- **Integration guide:** [`INTEGRATION_GUIDE.md`](INTEGRATION_GUIDE.md)
- **Version compatibility:** [`VERSION_COMPATIBILITY.md`](VERSION_COMPATIBILITY.md)
- **Build details:** [`BUILD.md`](BUILD.md)
- **Deployment details:** [`DEPLOYMENT.md`](DEPLOYMENT.md)

## What this repository teaches

This repo is most useful if you want to learn how to build a hardware-oriented C project with:

- a small hardware abstraction layer
- separate device drivers for SPI, I2C, UART, GPIO, and PWM devices
- structured logging instead of `printf`
- safer dynamic memory helpers
- retry logic for unreliable bus operations
- a simple top-level startup and shutdown flow

## What is in the codebase

Main source files in the current root-level layout include:

- `main.c` — program entry point, signal handling, and example hardware tests
- `system.c` / `system.h` — system startup, subsystem initialization, and shutdown
- `loki_life.c` / `loki_life.h` — **Loki life-cycle system** (see below)
- `log.c` / `log.h` — leveled logging macros and logger implementation
- `memory.c` / `memory.h` — tracked allocation helpers such as `malloc_safe()` and `free_safe()`
- `retry.c` / `retry.h` — retry strategies for transient hardware failures
- `gpio.c` / `gpio.h` — GPIO abstraction
- `spi.c` / `spi.h` — SPI abstraction
- `i2c.c` / `i2c.h` — I2C abstraction
- `uart.c` / `uart.h` — UART abstraction
- `pwm.c` / `pwm.h` — PWM abstraction
- `tft_driver.c` / `tft_driver.h` — TFT display driver
- `sdcard_driver.c` / `sdcard_driver.h` — SD card driver
- `flash_driver.c` / `flash_driver.h` — flash memory driver
- `eeprom_driver.c` / `eeprom_driver.h` — EEPROM driver
- `flipper_uart.c` / `flipper_uart.h` — Flipper Zero UART protocol support
- `board_config.h` — board-level settings such as frequencies and timing
- `pinout.h` — pin mappings used by the project

## How the program flows

At a high level, `main.c` shows a teachable embedded application structure:

1. print a startup banner
2. initialize logging
3. install signal handlers for graceful shutdown
4. call `system_init()`
5. call `loki_init()` — birth Loki as an egg
6. run sample hardware tests
7. run `demo_loki_lifecycle()` — shows feeding, moods, and stage growth in one pass
8. enter a loop: each iteration calls `loki_tick()` and routes Flipper commands to Loki actions
9. call `system_shutdown()` before exit

That pattern is a good starting point for your own SBC or hardware-control application.

## Loki life-cycle system

`loki_life.c` / `loki_life.h` implement the gameplay foundation for Loki's interactive behaviour.

### Life stages

Loki begins as an egg and advances through four stages based on accumulated **growth points (gp)**:

| Stage      | Threshold |
|------------|-----------|
| Egg        | starts here |
| Hatchling  | 50 gp     |
| Young      | 200 gp    |
| Adult      | 500 gp    |

Growth points are earned by feeding and interacting. The stage check runs automatically after every `loki_feed()`, `loki_interact()`, and `loki_tick()` call.

### Feeding

Three food qualities are available via `loki_feed(state, food_type)`:

| Food            | Hunger reduction | Growth pts | Happiness boost |
|-----------------|-----------------|------------|----------------|
| `LOKI_FOOD_BASIC`   | 20 | 5  | 5  |
| `LOKI_FOOD_TASTY`   | 40 | 10 | 15 |
| `LOKI_FOOD_SPECIAL` | 60 | 20 | 25 |

Feeding when Loki is not hungry halves all effects (overeating penalty).

### Mood

Mood is derived from hunger, happiness, and energy using a priority ladder:

```
HUNGRY  (hunger ≥ 70)
SLEEPY  (energy ≤ 20)
GRUMPY  (happiness ≤ 30 or hunger ≥ 50)
HAPPY   (happiness ≥ 70)
PLAYFUL (happiness ≥ 50 and energy ≥ 60)
NEUTRAL (everything else)
```

When Loki is `SLEEPY` the tick function restores energy instead of draining it, modelling a natural sleep/wake cycle.

### Animation states

`loki_get_animation_state(state)` returns one of:

| State                 | When shown |
|-----------------------|-----------|
| `LOKI_ANIM_EGG_IDLE`       | Default egg state |
| `LOKI_ANIM_EGG_WIGGLE`     | Egg close to hatching |
| `LOKI_ANIM_HATCHLING_IDLE` | Hatchling stage, neutral mood |
| `LOKI_ANIM_EATING`         | Shortly after any feeding |
| `LOKI_ANIM_SLEEPING`       | Mood is SLEEPY |
| `LOKI_ANIM_HAPPY`          | Mood is HAPPY |
| `LOKI_ANIM_GRUMPY`         | Mood is GRUMPY or HUNGRY |
| `LOKI_ANIM_PLAYFUL`        | Mood is PLAYFUL |
| `LOKI_ANIM_IDLE`           | Default for young/adult |

### Core API

```c
void        loki_init(loki_state_t *state);
void        loki_tick(loki_state_t *state, uint32_t delta_seconds);
void        loki_feed(loki_state_t *state, loki_food_t food);
void        loki_interact(loki_state_t *state);
void        loki_update_mood(loki_state_t *state);
void        loki_check_progression(loki_state_t *state);
loki_anim_t loki_get_animation_state(const loki_state_t *state);
void        loki_print_status(const loki_state_t *state);
```

### Extending the system

- **More food types**: add entries to `loki_food_t` and handle them in `loki_feed()`.
- **Persistence**: serialise `loki_state_t` to EEPROM or SD card — every field is a plain integer.
- **Display rendering**: read `loki_get_animation_state()` in the render loop and switch on the returned enum to select sprite/frame sets.
- **Flipper commands**: the main loop already maps Flipper command bytes `0x10`–`0x30` to feed and interact actions.

## Concepts worth learning from Loki

### 1. Layered design

The project separates responsibilities:

- low-level bus access lives in HAL-style modules like GPIO, SPI, I2C, UART, and PWM
- chip or peripheral behavior lives in driver files
- app behavior stays in `main.c` and `system.c`

This makes the code easier to debug and extend.

### 2. Logging discipline

The logging system gives you levels such as:

- `LOG_CRITICAL()`
- `LOG_ERROR()`
- `LOG_WARN()`
- `LOG_INFO()`
- `LOG_DEBUG()`

That is much better than scattering raw prints everywhere because it keeps diagnostics consistent.

### 3. Safer memory usage

The memory helpers encourage patterns like:

- allocate with `malloc_safe()`
- release with `free_safe()`
- report memory usage in debug workflows

That is a practical pattern for learning C without losing track of heap allocations.

### 4. Retry logic for hardware work

Hardware communication can fail temporarily. The retry helpers show how to wrap operations like SPI, I2C, or EEPROM access with a reusable retry policy instead of duplicating error-handling code.

### 5. Graceful shutdown

The signal handling in `main.c` demonstrates a clean exit path. That matters for embedded Linux software that may be stopped through SSH, a service manager, or a terminal.

## Build basics

Common commands documented in this repo are:

```bash name=build-commands.sh
make
make DEBUG=1
make DEBUG=0
make test
make analyze
make docs
make clean
```

The maintained native target is `loki_core.so`, a small shared library used by
`loki.py`. The interactive application is Python-based. Use Python 3.11 or
newer, then install the display dependency before starting the application:

```bash
python3 -m pip install -r requirements.txt
python3 main.py
```

On Debian-based SBC images, `sudo apt install python3-pil` is an equivalent
system-package installation for the display renderer. Without Pillow, Loki
still starts its configuration UI and non-display plugins, but display and
dragon animation rendering fall back safely.

Use `make test` to run the Python test suite.

## Local configuration UI

Loki uses the same USB-network addresses as a typical Pwnagotchi setup:
Loki is `10.0.0.2` and the connected computer is `10.0.0.1`. Install the
included systemd-networkd profile on Loki once, then restart networking:

**Code Review Checklist:**
- ✅ Compiles without warnings
- ✅ Follows naming conventions
- ✅ Includes LOG_* calls for debugging
- ✅ Error codes checked and logged
- ✅ malloc_safe/free_safe for dynamic allocation
- ✅ Doxygen comments for public functions
- ✅ Tested on target hardware

---

## 📜 License

**MIT License** - See [LICENSE](LICENSE) for full text

**Summary:**
- ✅ Free for personal and commercial use
- ✅ Can modify and distribute
- ✅ Must include license notice
- ✅ No warranty provided

---

## 📞 Support & Resources

### Documentation
- **[BUILD.md](BUILD.md)** - Detailed build system guide
- **[BUILD_WINDOWS.md](BUILD_WINDOWS.md)** - Windows platform guide
- **[DEPLOYMENT.md](DEPLOYMENT.md)** - Production deployment procedures
- **[CONTRIBUTING.md](CONTRIBUTING.md)** - Code standards and development
- **[FILE_REFERENCE.md](FILE_REFERENCE.md)** - Complete file inventory

### Datasheets & References
- [Orange Pi Zero 2W](http://orangepi.org)
- [ARM GNU Toolchain](https://developer.arm.com)
- [ILI9488 Display](http://www.displayfuture.com/Display/ili9488_specifications.pdf)
- [W25Q40 Flash](https://www.winbond.com/hq/product/code-storage-flash-memory/)
- [Flipper Zero](https://flipperzero.one)
- [Raspberry Pi](https://www.raspberrypi.com)

### Getting Help
- Check [Troubleshooting](#troubleshooting) section
- Review code examples in this README
- Read BUILD.md and DEPLOYMENT.md
- Check documentation with `make docs`

---

## 📈 Project Statistics

- **Lines of Code**: ~6,400
- **Total Files**: 40+
- **Documentation Pages**: 15+
- **Supported Platforms**: 4
- **Device Drivers**: 5
- **HAL Modules**: 5
- **Build Targets**: 12

---

## 🌐 Web UI

Loki includes a lightweight HTTP server that lets you monitor the device from
any browser on the local network — no extra software needed.

### Endpoints

| Endpoint | Description |
|----------|-------------|
| `GET /` | HTML dashboard — board info, hardware summary, API links |
| `GET /api/status` | JSON — app name, version, board, uptime, build type, mood/state |

### Quick access

```
http://<device-ip>:8080/         # by IP address (always works)
http://loki.local:8080/          # friendly name (requires mDNS — see below)
```

### JSON status example

```json
{
  "app": "Loki",
  "version": "1.0",
  "board": "Orange Pi Zero 2W Loki",
  "model": "OPI_ZERO_2W",
  "uptime_seconds": 142,
  "build": "release",
  "mood": "calm",
  "state": "running"
}
```

### Setting up `loki.local`

1. **Set the device hostname** on the Orange Pi:
   ```bash
   sudo hostnamectl set-hostname loki
   sudo reboot
   ```

2. **Install Avahi** (mDNS daemon) so `loki.local` resolves on the LAN:
   ```bash
   sudo apt-get install -y avahi-daemon
   sudo systemctl enable --now avahi-daemon
   ```

3. Open **`http://loki.local:8080/`** in a browser on any machine on the same LAN.

> **Note**: macOS and Windows 10+ resolve `.local` names automatically via Bonjour/mDNS.
> Linux clients need `avahi-daemon` installed as well (`sudo apt-get install avahi-daemon`).

### Configuration

Override defaults at build time via `-D` flags:

```bash
# Disable Web UI entirely
make DEBUG=0 CFLAGS+="-DWEBUI_ENABLED=0"

# Use a different port
make DEBUG=0 CFLAGS+="-DWEBUI_PORT=9090"
```

See [`DEPLOYMENT.md`](DEPLOYMENT.md#web-ui) for full deployment details.

---

## 🚀 Roadmap

### Planned Features
- [ ] Bluetooth support (ESP32)
- [ ] WiFi integration (ESP32)
- [ ] Multi-threading support
- [ ] Power management modes
- [ ] Watchdog timer integration
- [ ] SD card filesystem support (FatFS)
- [ ] USB HID support
- [ ] Real-time clock (RTC) integration

### Performance Optimization
- [ ] Interrupt-driven I/O
- [ ] DMA for SPI transfers
- [ ] Hardware acceleration where possible
- [ ] Cache optimization
- [ ] Memory pool allocation

---

## 🎉 Acknowledgments

Built with ❤️ for the embedded systems community.

---

**Loki Embedded System** - Making embedded development easier, one HAL at a time.
````
