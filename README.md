# Loki

Loki is an embedded C project for a dragon-themed interactive device that runs on a single-board computer and talks to attached hardware such as a TFT display, SD card, flash memory, EEPROM, and a Flipper Zero over UART.

## Architecture Overview

Loki is the master embedded systems framework for this ecosystem. It provides reusable HAL modules, device drivers, runtime orchestration, and Python-side integration points that downstream projects consume rather than re-implement.

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

By default the configuration UI serves on loopback (`http://127.0.0.1:8082`),
which works on a fresh install with no extra network setup. To reach it from
another machine over USB, Loki uses the same USB-network addresses as a typical
Pwnagotchi setup: Loki is `10.0.0.2` and the connected computer is `10.0.0.1`.
Set `address = "10.0.0.2"` under `[ui.web]` in `config.toml`, install the
included systemd-networkd profile on Loki once, then restart networking:

```bash
sudo install -D -m 644 network/loki-usb0.network /etc/systemd/network/10-loki-usb0.network
sudo systemctl enable --now systemd-networkd
sudo systemctl restart systemd-networkd
```

Set the computer's USB Ethernet interface to the static address
`10.0.0.1/24`, connect to Loki over USB, and open `http://10.0.0.2:8080`.
On Linux, this can be configured with:

```bash
sudo ip address replace 10.0.0.1/24 dev <usb-interface>
sudo ip link set <usb-interface> up
```

The UI accepts connections only from that USB subnet. Edit values, save them,
then restart Loki for the updated configuration to take effect. If the
configured USB address is not assigned to a local interface yet (for example
`usb0` is down), Loki logs a warning and serves the UI on loopback
(`127.0.0.1`) instead of failing to start. Configure the
WPA-SEC plugin key outside the repository:

```bash
export LOKI_WPA_SEC_API_KEY="your-key"
python3 main.py
```

For a systemd-managed Loki process, persist the key in an override instead of
adding it to `config.toml`:

```bash
sudo systemctl edit loki
# Add: [Service]
# Add: Environment=LOKI_WPA_SEC_API_KEY=your-key
```

Set `[web_ui].enabled = false` in `config.toml` to disable the editor. For a
strictly local-only UI instead, set `[web_ui].host = "127.0.0.1"`.

## A2C AI brain (Pwnagotchi-style adaptive loop)

Loki now includes a local actor-critic plugin at `plugins/ai_brain.py` that
runs a lightweight A2C loop without external ML dependencies.

1. Enable Bettercap telemetry and the AI brain in `config.toml`:
   - `[plugins.bettercap].enabled = true`
   - `[plugins.ai_brain].enabled = true`
   - `[plugins.ai_brain].learning = true` (online updates)
2. Start Loki normally with `python3 main.py`.
3. Let it run for a while; policy/value weights are persisted to:
   - `~/.local/share/loki/a2c_state.json`
4. For inference-only behavior, set:
   - `[plugins.ai_brain].learning = false`
5. To make inference deterministic (no action sampling), set:
   - `[plugins.ai_brain].deterministic = true`

The AI brain consumes shared telemetry (AP/client counts and API health) from
the Bettercap plugin and publishes its current action/probabilities through
plugin state for other modules to consume.

## Master configuration (`config.toml`)

Loki now uses a single master `config.toml` with a runtime-first structure inspired by Pwnagotchi:

- `[main]`, `[main.auth]`, `[main.network]`
- `[main.plugins.*]` including plugin loader settings
- `[ui]`, `[ui.web]`, `[ui.display]`

Build-time C macros are generated from `[build.board]` and `[build.pinout]` by:

```bash
python3 tools/gen_config.py
```

The generated files are `board_config.h`, `pinout.h`, and `config.h`.

## Good ways to study this project

If you are learning from this repo, a strong reading order is:

1. `README.md`
2. `main.c`
3. `system.c` and `system.h`
4. `loki_life.h` and `loki_life.c` — the life-cycle system
5. `log.*`, `memory.*`, and `retry.*`
6. `spi.*`, `i2c.*`, `uart.*`, `gpio.*`, and `pwm.*`
7. the device drivers
8. `BUILD.md` and `DEPLOYMENT.md`

## Hardware focus

The repository was originally documented around Raspberry Pi hardware, and much of the current checked-in code and documentation still reflects that. The code also includes Raspberry Pi and Flipper-related intent in various places, so treat board assumptions as something to verify before wiring real hardware.

## Important note

This README now focuses only on the most teachable and durable information. For deeper platform setup, deployment details, troubleshooting, and build workflow notes, use the companion docs already in the repository such as:

- `BUILD.md`
- `BUILD_WINDOWS.md`
- [`DEPLOYMENT.md`](DEPLOYMENT.md) — cross-compile and deploy to Orange Pi / Raspberry Pi, including [setting up Loki as a persistent systemd service](DEPLOYMENT.md#using-systemd-service)
- `CONTRIBUTING.md`
- `QUICK_REFERENCE.md`

## License

MIT License. See `LICENSE`.

