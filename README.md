# Loki

Loki is an embedded C project for a dragon-themed interactive device that runs on a single-board computer and talks to attached hardware such as a TFT display, SD card, flash memory, EEPROM, and a Flipper Zero over UART.

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

If your environment is missing the ARM cross-compiler, builds that depend on `arm-linux-gnueabihf-gcc` will fail until the toolchain is installed.

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

The repository was originally documented around Orange Pi Zero 2W hardware, and much of the current checked-in code and documentation still reflects that. The code also includes Raspberry Pi and Flipper-related intent in various places, so treat board assumptions as something to verify before wiring real hardware.

## Important note

This README now focuses only on the most teachable and durable information. For deeper platform setup, deployment details, troubleshooting, and build workflow notes, use the companion docs already in the repository such as:

- `BUILD.md`
- `BUILD_WINDOWS.md`
- `DEPLOYMENT.md`
- `CONTRIBUTING.md`
- `QUICK_REFERENCE.md`

## License

MIT License. See `LICENSE`.
