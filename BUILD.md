# Build Guide

This guide covers the current `Makefile` workflow for Loki.

## What the build expects

- GNU Make
- ARM cross-compiler: `arm-linux-gnueabihf-gcc`
- optional tools:
  - `cppcheck` for `make analyze`
  - `doxygen` for `make docs`

Ubuntu/Debian example:

```bash
sudo apt-get update
sudo apt-get install make gcc-arm-linux-gnueabihf cppcheck doxygen
```

## Build modes

```bash
make         # default debug build (same as DEBUG=1)
make DEBUG=1 # debug build
make DEBUG=0 # release build
```

Outputs:

- debug binary: `build/debug/loki_app`
- release binary: `build/release/loki_app`

## Useful Make targets

| Target | Purpose |
| --- | --- |
| `make` / `make all` | Build with current mode |
| `make DEBUG=0` | Build optimized release binary |
| `make install` | Copy built binary to remote target via SCP and set executable bit |
| `make run` | `install` then execute remotely with `sudo` |
| `make test` | Rebuild with `-DMOCK_HARDWARE` and run local debug binary |
| `make analyze` | Run `cppcheck` |
| `make docs` | Generate Doxygen docs |
| `make size` | Print binary size and top symbols |
| `make info` | Show current build configuration |
| `make clean` | Remove active build directory |
| `make clean-all` | Remove build/docs artifacts and object/dependency leftovers |

## Remote install/run configuration

`make install` and `make run` use these variables:

```bash
CROSS_USER=pi
CROSS_HOST=orange-pi.local
CROSS_PATH=/tmp
```

Override per command when needed:

```bash
make DEBUG=0 install CROSS_HOST=192.168.1.100 CROSS_USER=pi CROSS_PATH=/tmp
make run CROSS_HOST=orange-pi.local
```

## Typical workflow

```bash
# 1) build
make DEBUG=1

# 2) optional static checks
make analyze

# 3) optional docs
make docs

# 4) deploy/run on target board
make DEBUG=0 install
# or
make DEBUG=0 run
```

## Troubleshooting

### `arm-linux-gnueabihf-gcc: No such file or directory`

Install the ARM toolchain and ensure it is in `PATH`.

### `cppcheck: No such file or directory`

Install `cppcheck` or skip `make analyze`.

### SSH/SCP errors during `make install`

Verify network access and credentials, then override `CROSS_HOST`/`CROSS_USER` if needed.

## Notes

- Source files are compiled from repository root (`*.c` with `-I.`).
- The build system currently assumes an ARMv7 target (`-march=armv7-a -mtune=cortex-a7`).
- For deployment details, see `DEPLOYMENT.md`.

## License

MIT License. See `LICENSE`.
