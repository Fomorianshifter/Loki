#!/usr/bin/env python3
"""
Generate board_config.h, pinout.h, and config.h from config.toml.
Runtime settings are kept in config.toml for application/plugin use.
Build-time macro generation reads [build.board] and [build.pinout].
"""

from __future__ import annotations

import pathlib
import sys

try:
    import tomllib
except ModuleNotFoundError:
    print("Python 3.11+ is required (tomllib not found).", file=sys.stderr)
    raise SystemExit(1)

ROOT = pathlib.Path(__file__).resolve().parents[1]
CONFIG_TOML = ROOT / "config.toml"
BOARD_H = ROOT / "board_config.h"
PINOUT_H = ROOT / "pinout.h"
CONFIG_H = ROOT / "config.h"

BANNER = """/* AUTO-GENERATED FILE - do not edit directly.
 * Source: config.toml
 * Generator: tools/gen_config.py
 */"""


def format_value(value: object) -> str:
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        formatted = repr(value)
        if "." not in formatted and "e" not in formatted.lower():
            formatted += ".0"
        return formatted
    if isinstance(value, str):
        escaped = value.replace("\\", "\\\\").replace('"', '\\"')
        return f'"{escaped}"'
    raise TypeError(f"Unsupported TOML value type: {type(value)!r}")


def format_hex(value: object) -> str:
    if not isinstance(value, int) or isinstance(value, bool):
        raise TypeError(f"Hex value must be int, got {type(value)!r}")
    return f"0x{value:X}"


def write_defines(lines: list[str], section: dict[str, object]) -> None:
    for key, value in section.items():
        if isinstance(value, dict):
            continue
        lines.append(f"#define {key:<28} {format_value(value)}")


def write_hex_defines(lines: list[str], section: dict[str, object]) -> None:
    for key, value in section.items():
        lines.append(f"#define {key:<28} {format_hex(value)}")


def write_raw_defines(lines: list[str], section: dict[str, object]) -> None:
    for key, value in section.items():
        if not isinstance(value, str):
            raise TypeError(f"Raw value for {key} must be string")
        lines.append(f"#define {key:<28} {value}")


def emit_board(board: dict[str, object]) -> None:
    lines: list[str] = [
        "#ifndef BOARD_CONFIG_H",
        "#define BOARD_CONFIG_H",
        "",
        BANNER,
        "",
        "/* ===== BOARD CONFIGURATION ===== */",
    ]
    write_defines(lines, board)

    if "hex" in board:
        lines.extend(["", "/* ===== HEX CONSTANTS ===== */"])
        write_hex_defines(lines, board["hex"])
    if "raw" in board:
        lines.extend(["", "/* ===== RAW CONSTANTS ===== */"])
        write_raw_defines(lines, board["raw"])

    lines.extend(["", "#endif /* BOARD_CONFIG_H */", ""])
    BOARD_H.write_text("\n".join(lines), encoding="utf-8")


def emit_pinout(pinout: dict[str, object]) -> None:
    lines: list[str] = [
        "#ifndef PINOUT_H",
        "#define PINOUT_H",
        "",
        BANNER,
        "",
        "/* ===== PINOUT CONFIGURATION ===== */",
    ]
    write_defines(lines, pinout)

    if "hex" in pinout:
        lines.extend(["", "/* ===== HEX CONSTANTS ===== */"])
        write_hex_defines(lines, pinout["hex"])
    if "raw" in pinout:
        lines.extend(["", "/* ===== RAW CONSTANTS ===== */"])
        write_raw_defines(lines, pinout["raw"])

    lines.extend(["", "#endif /* PINOUT_H */", ""])
    PINOUT_H.write_text("\n".join(lines), encoding="utf-8")


def emit_master_config() -> None:
    lines = [
        "#ifndef CONFIG_H",
        "#define CONFIG_H",
        "",
        BANNER,
        "",
        '#include "board_config.h"',
        '#include "pinout.h"',
        "",
        "#endif /* CONFIG_H */",
        "",
    ]
    CONFIG_H.write_text("\n".join(lines), encoding="utf-8")


def resolve_build_sections(data: dict[str, object]) -> tuple[dict[str, object], dict[str, object]]:
    """Return board and pinout sections for macro generation.

    Preferred layout:
      [build.board], [build.pinout]
    Backward-compatible layout:
      [board], [pinout]
    """
    build = data.get("build")
    if isinstance(build, dict) and isinstance(build.get("board"), dict) and isinstance(build.get("pinout"), dict):
        return build["board"], build["pinout"]

    board = data.get("board")
    pinout = data.get("pinout")
    if isinstance(board, dict) and isinstance(pinout, dict):
        return board, pinout

    raise ValueError(
        "config.toml must define [build.board] and [build.pinout] "
        "(or legacy [board]/[pinout])."
    )


def main() -> int:
    if not CONFIG_TOML.exists():
        print(f"Missing {CONFIG_TOML}", file=sys.stderr)
        return 1

    with CONFIG_TOML.open("rb") as file_obj:
        data = tomllib.load(file_obj)

    try:
        board, pinout = resolve_build_sections(data)
    except ValueError as error:
        print(str(error), file=sys.stderr)
        return 1

    emit_board(board)
    emit_pinout(pinout)
    emit_master_config()
    print("Generated board_config.h, pinout.h, config.h")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
