# Loki

Loki is an embedded C codebase centered on a dragon-themed hardware project for Linux-capable single-board computers.

This repository is best treated as a **learning and experimentation project**: it demonstrates practical patterns for hardware-oriented C software, while still evolving toward a more complete product.

## Project vision

Loki aims to become a reusable base for interactive hardware applications that combine:

- board-level I/O (GPIO, SPI, I2C, UART, PWM)
- peripheral drivers (display, storage, EEPROM, external device link)
- clear startup/shutdown control flow
- safer logging, memory, and retry practices

## Current state (realistic summary)

What the codebase currently provides:

- a root-level C project with a `Makefile` build flow
- ARM cross-compilation defaults (`arm-linux-gnueabihf-gcc`)
- a runnable app entry (`main.c`) with initialization and teardown paths
- reusable support modules for logging (`log.*`), memory (`memory.*`), and retries (`retry.*`)
- hardware abstraction and device-oriented modules (`gpio.*`, `spi.*`, `i2c.*`, `uart.*`, `pwm.*`, and driver files)

What to verify for your setup:

- actual pin mappings and bus assignments in `pinout.h` and `board_config.h`
- target board capabilities and Linux device availability
- deployment/security model for any long-running use

## What this repository teaches well

- layering HAL-style interfaces and higher-level drivers
- reducing noisy `printf` debugging with leveled logging macros
- safer heap usage with explicit allocation/free helpers
- wrapping transient I/O failures with retry strategies
- structuring embedded-Linux startup and graceful shutdown

## Build and development commands

```bash
make                 # default debug build
make DEBUG=1         # explicit debug build
make DEBUG=0         # release build
make test            # mock-hardware local run target
make install         # copy binary to configured target over SSH
make run             # install + run on configured target
make analyze         # cppcheck static analysis
make docs            # generate Doxygen docs
make clean
make clean-all
```

If `arm-linux-gnueabihf-gcc` is not installed, build targets will fail until the cross toolchain is available.

## Recommended reading order

1. `README.md`
2. `main.c`
3. `system.c` / `system.h`
4. `log.*`, `memory.*`, `retry.*`
5. HAL modules (`gpio.*`, `spi.*`, `i2c.*`, `uart.*`, `pwm.*`)
6. device driver modules (`*_driver.*`, `flipper_uart.*`)
7. `BUILD.md` and `DEPLOYMENT.md`

## Related docs

- `BUILD.md` — concise build and toolchain guide
- `BUILD_WINDOWS.md` — Windows build flow
- `DEPLOYMENT.md` — practical deployment checklist and service setup baseline
- `CONTRIBUTING.md` — contribution and code standards

## License

MIT License. See `LICENSE`.
