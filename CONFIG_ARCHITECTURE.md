# Loki Configuration Architecture

> **Added by:** refactor/centralize-config PR  
> **Status:** Active — this document describes the current state after the refactor.

---

## Overview

Loki now has a **single master configuration system**.  All runtime behaviour
is controlled from one file on the SD card (`/loki/loki_config.ini`).
EEPROM is reserved for tiny bootstrap/failsafe metadata only.

```
┌─────────────────────────────────────────────────────────────┐
│                     Boot Priority                           │
│                                                             │
│  Highest  ┌─────────────────────────────────────────────┐  │
│           │  EEPROM safe_mode_flag = 1                  │  │
│           │  → force safe mode, skip SD config          │  │
│           └─────────────────────────────────────────────┘  │
│              ↓                                              │
│           ┌─────────────────────────────────────────────┐  │
│           │  SD card: /loki/loki_config.ini             │  │
│           │  → primary runtime configuration            │  │
│           └─────────────────────────────────────────────┘  │
│              ↓ (fallback if SD missing/corrupt)             │
│           ┌─────────────────────────────────────────────┐  │
│  Lowest   │  Compile-time defaults (config_defaults.h) │  │
│           └─────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## New Files

| File | Purpose |
|------|---------|
| `config_defaults.h` | Compile-time default values for all subsystems |
| `eeprom_layout.h` | EEPROM memory map definition and bootstrap API |
| `config_manager.h` | Runtime config manager public interface |
| `config_manager.c` | Config manager implementation (INI parser + boot sequence) |
| `plugin_registry.h` | Plugin descriptor types and registry API |
| `plugin_registry.c` | Plugin registry implementation |
| `program_registry.h` | Program descriptor types and registry API |
| `program_registry.c` | Program registry implementation |
| `config/loki_config.ini` | Master SD-backed config template (copy to SD card) |
| `CONFIG_ARCHITECTURE.md` | This document |

---

## Master Configuration File

**Location on SD card:** `/loki/loki_config.ini`  
**Template:** `config/loki_config.ini` in this repository

### Format

INI-style key=value pairs within `[section]` headers.

```ini
# This is a comment
[section]
key=value
boolean_key=true
```

- Lines starting with `#` or `;` are comments.
- Inline comments are **not** supported.
- Boolean values: `true` / `false`, `1` / `0`, `yes` / `no`.
- Unknown keys are silently ignored.

### Sections

#### `[system]`
Global system behaviour.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `startup_mode` | string | `normal` | `normal` / `safe` / `demo` |
| `log_level` | string | `info` | `debug` / `info` / `warn` / `error` / `critical` |
| `safe_mode` | bool | `false` | Force safe mode from config (no plugins, no AI) |

#### `[ai]`
AI subsystem.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `enabled` | bool | `false` | Master switch for AI features |
| `model` | string | `none` | Model identifier (application-defined) |
| `max_tokens` | int | `512` | Maximum token budget per request |

#### `[plugins]`
Optional compiled-in subsystems.

Plugins are registered in application code with `plugin_registry_register()`.
This section only controls whether they are enabled and their startup order.

```ini
plugin0=mood_engine
plugin0_enabled=true
plugin0_order=10
plugin1=flipper_bridge
plugin1_enabled=true
plugin1_order=20
```

#### `[programs]`
External programs on the SD card that Loki can launch.

```ini
program0=credits_vending
program0_path=/loki/programs/credits_vending
program0_enabled=true
program0_auto_start=false
```

#### `[storage]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `sd_config_enabled` | bool | `true` | Load this config from SD card |
| `sd_mount_point` | string | `/loki` | Root path on SD card |
| `fallback_to_defaults` | bool | `true` | Use defaults if SD config is missing |

#### `[display]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `brightness` | int | `80` | Display brightness (0–100%) |
| `rotation` | int | `0` | 0=Normal, 1=90°, 2=180°, 3=270° |

#### `[flipper]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `enabled` | bool | `true` | Enable Flipper Zero UART communication |
| `baud_rate` | int | `115200` | UART baud rate |

#### `[mood]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `enabled` | bool | `false` | Enable mood/personality engine |
| `default_mood` | string | `neutral` | Initial mood state |

#### `[safety]`

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `safe_mode_on_boot` | bool | `false` | Always start in safe mode |
| `recovery_enabled` | bool | `true` | Allow recovery from fatal errors |
| `watchdog_timeout_s` | int | `30` | Watchdog timeout (0 = disabled) |

---

## EEPROM Bootstrap Layout

**Device:** FT24C02A — 256 bytes total  
**Bootstrap block:** bytes 0–15 (16 bytes)

```
Offset  Size  Field               Description
------  ----  ------------------  -----------------------------------------
 0       2    magic               0x4C4B ('L','K') — validity marker
 2       1    schema_ver          EEPROM layout version (currently 1)
 3       1    config_ver          SD config schema version last seen
 4       2    boot_count          Cumulative boot counter (saturates at 65535)
 6       1    safe_mode_flag      1 = boot into safe mode on next start
 7       1    sd_config_enabled   1 = load config from SD card
 8       1    brightness          Last-known-good display brightness (0–100)
 9       1    startup_mode        0=normal, 1=safe, 2=demo
10       4    reserved            Zero-filled, reserved for future use
14       2    crc16               CRC-16/CCITT over bytes 0–13
```

Bytes 16–255 are **free** for future application use.

### EEPROM API

```c
#include "eeprom_layout.h"

eeprom_bootstrap_t bs;

/* Read and validate */
if (eeprom_bootstrap_read(&bs) != HAL_OK) {
    /* first boot or corruption — bs is populated with defaults */
}

/* Modify */
bs.safe_mode_flag = 1;

/* Write back (CRC is recomputed automatically) */
eeprom_bootstrap_write(&bs);

/* Or use config_manager_save_bootstrap() to sync from runtime config */
config_manager_save_bootstrap();
```

### EEPROM vs SD Card

| Data | Stored in |
|------|-----------|
| Magic / schema version | EEPROM |
| Boot counter | EEPROM |
| Safe mode flag | EEPROM |
| SD config enabled flag | EEPROM |
| Last-known-good brightness | EEPROM |
| **Everything else** | SD card (`loki_config.ini`) |

---

## Boot Sequence

Managed by `config_manager_init()`, called from `system_init()` after GPIO init.

```
system_init()
  │
  ├─ 1. memory_init()
  ├─ 2. gpio_init()
  │
  └─ config_manager_init()
       │
       ├─ [1/7] Load compile-time defaults (config_defaults.h)
       ├─ [2/7] eeprom_init()
       ├─ [3/7] eeprom_bootstrap_read()  ← increment boot_count, check safe_mode_flag
       ├─ [4/7] sdcard_init()
       ├─ [5/7] load /loki/loki_config.ini  (skipped in safe mode)
       ├─ [6/7] validate + apply config → sync plugin/program registries
       └─ [7/7] plugin_registry_init_all()  (skipped in safe mode)

  ├─ tft_init()   — brightness from config
  ├─ flash_init()
  └─ flipper_uart_init()   — conditional on config flipper.enabled
```

### Fallback behaviour

| Condition | Behaviour | Log message |
|-----------|-----------|-------------|
| EEPROM CRC/magic fail | Write defaults to EEPROM, continue | WARN: `EEPROM bootstrap invalid` |
| EEPROM `safe_mode_flag=1` | Skip SD config, skip plugins | WARN: `SAFE MODE activated` |
| SD card init fail | Use EEPROM + defaults | WARN: `SD card init failed` |
| Config file missing | Use EEPROM + defaults | WARN: `SD config read failed` |
| Config file corrupt | Use EEPROM + defaults | WARN: `SD config does not look like an INI file` |
| Plugin init fail | Log error, return HAL_ERROR | ERROR: `Plugin '...' init failed` |

---

## Plugin Registry

Plugins are compiled-in subsystems managed by `plugin_registry.c`.

### Registering a plugin

```c
#include "plugin_registry.h"

/* In your subsystem init code, before config_manager_init() runs: */
plugin_registry_register("mood_engine", mood_engine_init, mood_engine_deinit);
```

### Config-driven enable/order

The config manager reads `[plugins]` from `loki_config.ini` and calls
`plugin_registry_set_enabled()` and `plugin_registry_set_order()` for each entry.

### Lifecycle

```
plugin_registry_init_all()   ← called by config_manager_init()
  → sorted by .order (lower = earlier)
  → calls .init() for each enabled plugin

plugin_registry_deinit_all() ← called by config_manager_deinit() at shutdown
  → reverse order
  → calls .deinit() for each initialized plugin
```

---

## Program Registry

Programs are external executables on the SD card managed by `program_registry.c`.

```c
/* Programs are registered by the config manager from [programs] in config */
const program_entry_t *p = program_registry_get(0);
if (p && p->enabled && p->auto_start) {
    /* launch p->path */
}
```

`auto_start` programs are identified in the main loop or scheduler — the
registry provides the metadata, not the execution mechanism.

---

## Migration Notes

### Prior configuration locations

| Old location | New location |
|--------------|--------------|
| `board_config.h` — `TFT_BRIGHTNESS` | `loki_config.ini` `[display] brightness` |
| `board_config.h` — `UART1_BAUD_RATE` | `loki_config.ini` `[flipper] baud_rate` |
| Scattered EEPROM read/writes | `eeprom_layout.h` + `eeprom_bootstrap_*()` |
| Direct `sdcard_init()` in `system_init()` | Owned by `config_manager_init()` |
| Direct `eeprom_init()` in `system_init()` | Owned by `config_manager_init()` |

### What stays in `board_config.h`

Hardware constants that are fixed at compile time and never change at runtime:
- SPI/I2C/UART frequencies
- Display dimensions and type
- EEPROM capacity and I2C address
- Flash JEDEC ID and page sizes
- Timing/delay constants

Do **not** move pin numbers or bus addresses to the config file — keep those in
`pinout.h` and `board_config.h`.

### Accessing runtime config

```c
#include "config_manager.h"

const loki_config_t *cfg = config_manager_get();
if (cfg != NULL) {
    if (cfg->ai.enabled) { /* ... */ }
    uint8_t brightness = cfg->display.brightness;
}
```

---

## Deployment

1. **Copy the template to your SD card:**

   ```
   cp config/loki_config.ini <sd_card_mount>/loki/loki_config.ini
   ```

2. **Edit the config as needed** (enable plugins, set brightness, etc.).

3. **Insert SD card and boot** — the config manager will pick it up automatically.

4. **Safe mode boot:** To force safe mode without editing the config file, set
   `safe_mode_flag=1` in EEPROM using the serial console or by temporarily
   editing `[safety] safe_mode_on_boot=true` in `loki_config.ini`.

---

## Limitations and Follow-up Work

- The SD config loader uses raw sector access (sector offset 2048) rather than
  a proper FAT filesystem driver.  When a FAT layer is available, replace
  `load_config_from_sd()` with `fopen(DEFAULT_CONFIG_PATH, "r")`.

- The `[hardware]` section keys are parsed but not yet applied to HAL settings
  at runtime — hardware frequencies remain compile-time constants.

- Plugin `auto_start` via programs is metadata-only; a scheduler/launcher
  component is needed to actually execute program binaries.

- Hot-reload (`config_manager_reload()`) re-parses the config but does not
  restart already-running plugins.  A full plugin lifecycle reload is a
  follow-up task.
