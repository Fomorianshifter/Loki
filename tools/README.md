# Config generation tools

## Overview

`config.toml` (repo root) is the **single source of truth** for all board
settings and pin assignments used by the Loki firmware.

The three C headers that the firmware includes are **auto-generated** from
that file and must never be edited by hand:

| Generated file  | Contents |
|-----------------|----------|
| `board_config.h` | Frequencies, capacities, timing, feature flags |
| `pinout.h`       | GPIO / SPI / I2C / UART pin assignments        |
| `config.h`       | Umbrella `#include` — pulls in the two above   |

---

## Editing configuration

1. Open `config.toml` in any text editor.
2. Change the value you need.
3. Regenerate the headers (see below).
4. Rebuild the firmware.

### TOML section guide

| Section       | What it contains |
|---------------|-----------------|
| `[board]`     | Numeric / string settings emitted as C literals |
| `[board.hex]` | Integer values emitted as `0x…` hex in C        |
| `[board.raw]` | Values emitted as raw C identifiers (no quotes)  |
| `[pinout]`    | Pin numbers                                      |
| `[pinout.hex]`| Pin-related hex constants (e.g. I2C addresses)  |
| `[pinout.raw]`| Chip-select / alias macros (`TFT_CS`, etc.)     |

---

## Regenerating headers

```bash
python3 tools/gen_config.py
```

Requires **Python 3.11+** (uses `tomllib` from the standard library).

The script writes `board_config.h`, `pinout.h`, and `config.h` to the repo
root and prints a short confirmation.

---

## Build integration

The `Makefile` regenerates the headers automatically whenever `config.toml`
or `tools/gen_config.py` is newer than the generated files:

```
make          # regenerates headers if needed, then builds
make config   # regenerate headers only, no compilation
```

The `all` target depends on the generated headers, so a fresh checkout or a
change to `config.toml` will always trigger regeneration before the compiler
sees the files.

---

## Adding a new setting

1. Add the key/value pair to the appropriate section in `config.toml`.
2. Run `python3 tools/gen_config.py`.
3. The new `#define` will appear in the relevant generated header with the
   same name (converted to `UPPER_CASE`) and value.
4. No changes to existing C source files are required.
