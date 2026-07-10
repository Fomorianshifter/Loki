#!/usr/bin/env python3
"""
gen_config.py — Generate C config headers from config.toml.

Usage:
    python3 tools/gen_config.py

Generates (at repo root):
    board_config.h   — board-level settings (frequencies, sizes, timing)
    pinout.h         — GPIO/SPI/I2C/UART pin assignments
    config.h         — umbrella header that includes the two above

All generated files carry an AUTO-GENERATED banner and must not be edited
by hand.  Edit config.toml and re-run this script instead.

Requires Python 3.11+ (tomllib is in the standard library from 3.11 onward).
"""
from __future__ import annotations

import pathlib
import sys

# tomllib is stdlib in Python 3.11+
try:
    import tomllib
except ModuleNotFoundError:
    print(
        "ERROR: Python 3.11+ is required (tomllib not found). "
        "On older Python you can install 'tomli' and import it instead.",
        file=sys.stderr,
    )
    sys.exit(1)

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
ROOT = pathlib.Path(__file__).resolve().parents[1]
TOML_PATH = ROOT / "config.toml"
BOARD_H   = ROOT / "board_config.h"
PINOUT_H  = ROOT / "pinout.h"
CONFIG_H  = ROOT / "config.h"

# ---------------------------------------------------------------------------
# Banner written into every generated file
# ---------------------------------------------------------------------------
BANNER = """\
/* AUTO-GENERATED FILE — do not edit by hand.
 * Source:    config.toml
 * Generator: tools/gen_config.py
 * Regenerate with: python3 tools/gen_config.py
 */"""


# ---------------------------------------------------------------------------
# Value formatting helpers
# ---------------------------------------------------------------------------

def fmt_value(v: object) -> str:
    """Return the C literal representation for a TOML scalar value."""
    if isinstance(v, bool):
        # bool is a subclass of int — check it first
        return "1" if v else "0"
    if isinstance(v, int):
        return str(v)
    if isinstance(v, float):
        # Preserve at least one decimal place so it looks like a float in C
        s = repr(v)
        if "." not in s and "e" not in s.lower():
            s += ".0"
        return s
    if isinstance(v, str):
        # Strings are emitted as quoted C string literals
        escaped = v.replace("\\", "\\\\").replace('"', '\\"')
        return f'"{escaped}"'
    raise TypeError(f"Unsupported TOML value type: {type(v).__name__!r} for value {v!r}")


def fmt_hex(v: int) -> str:
    """Return a 0x-prefixed uppercase hex literal."""
    if not isinstance(v, int) or isinstance(v, bool):
        raise TypeError(f"Expected int for hex section, got {type(v).__name__!r}")
    # Use enough digits to represent the value cleanly
    if v <= 0xFF:
        return f"0x{v:02X}"
    if v <= 0xFFFF:
        return f"0x{v:04X}"
    return f"0x{v:06X}"


# ---------------------------------------------------------------------------
# Header emitters
# ---------------------------------------------------------------------------

def _write_defines(lines: list[str], section: dict) -> None:
    """Append #define lines for every scalar key in *section* (skip sub-tables)."""
    for name, value in section.items():
        if isinstance(value, dict):
            continue  # sub-table — handled separately
        lines.append(f"#define {name:<24} {fmt_value(value)}")


def _write_hex_defines(lines: list[str], hex_section: dict) -> None:
    """Append #define lines with 0x… literals for every key in *hex_section*."""
    for name, value in hex_section.items():
        lines.append(f"#define {name:<24} {fmt_hex(value)}")


def _write_raw_defines(lines: list[str], raw_section: dict) -> None:
    """Append #define lines where values are raw C identifiers (no quotes)."""
    for name, raw_expr in raw_section.items():
        if not isinstance(raw_expr, str):
            raise TypeError(
                f"[*.raw] entries must be TOML strings, got {type(raw_expr).__name__!r}"
                f" for key {name!r}"
            )
        lines.append(f"#define {name:<24} {raw_expr}")


def emit_board_config_h(board: dict, path: pathlib.Path) -> None:
    guard = "BOARD_CONFIG_H"
    lines: list[str] = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        BANNER,
        "",
        "/* ===== BOARD CONFIGURATION ===== */",
    ]

    _write_defines(lines, board)

    if "hex" in board:
        lines.append("")
        lines.append("/* ===== HEX CONSTANTS ===== */")
        _write_hex_defines(lines, board["hex"])

    if "raw" in board:
        lines.append("")
        lines.append("/* ===== C IDENTIFIER ALIASES ===== */")
        _write_raw_defines(lines, board["raw"])

    lines += ["", f"#endif /* {guard} */", ""]
    path.write_text("\n".join(lines), encoding="utf-8")
    print(f"  wrote {path.relative_to(ROOT)}")


def emit_pinout_h(pinout: dict, path: pathlib.Path) -> None:
    guard = "PINOUT_H"
    lines: list[str] = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        BANNER,
        "",
        "/* ===== PIN ASSIGNMENTS ===== */",
    ]

    _write_defines(lines, pinout)

    if "hex" in pinout:
        lines.append("")
        lines.append("/* ===== HEX CONSTANTS ===== */")
        _write_hex_defines(lines, pinout["hex"])

    if "raw" in pinout:
        lines.append("")
        lines.append("/* ===== DEVICE ALIASES ===== */")
        _write_raw_defines(lines, pinout["raw"])

    lines += ["", f"#endif /* {guard} */", ""]
    path.write_text("\n".join(lines), encoding="utf-8")
    print(f"  wrote {path.relative_to(ROOT)}")


def emit_config_h(path: pathlib.Path) -> None:
    guard = "CONFIG_H"
    lines: list[str] = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        BANNER,
        "",
        "/* Single entry point for all board and pin configuration.",
        " * Include this header instead of board_config.h / pinout.h directly.",
        " */",
        '#include "board_config.h"',
        '#include "pinout.h"',
        "",
        f"#endif /* {guard} */",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")
    print(f"  wrote {path.relative_to(ROOT)}")


# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------

def validate(data: dict) -> None:
    errors: list[str] = []
    if "board" not in data or not isinstance(data["board"], dict):
        errors.append("Missing [board] table")
    if "pinout" not in data or not isinstance(data["pinout"], dict):
        errors.append("Missing [pinout] table")
    if errors:
        raise ValueError("config.toml validation failed:\n  " + "\n  ".join(errors))


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> int:
    if not TOML_PATH.exists():
        print(f"ERROR: {TOML_PATH} not found", file=sys.stderr)
        return 1

    with TOML_PATH.open("rb") as fh:
        data = tomllib.load(fh)

    validate(data)

    print(f"Reading {TOML_PATH.relative_to(ROOT)}")
    print("Generating headers:")
    emit_board_config_h(data["board"], BOARD_H)
    emit_pinout_h(data["pinout"], PINOUT_H)
    emit_config_h(CONFIG_H)
    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
