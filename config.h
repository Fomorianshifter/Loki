#ifndef CONFIG_H
#define CONFIG_H

/* AUTO-GENERATED FILE — do not edit by hand.
 * Source:    config.toml
 * Generator: tools/gen_config.py
 * Regenerate with: python3 tools/gen_config.py
 */

/* Single entry point for all board and pin configuration.
 * Include this header instead of board_config.h / pinout.h directly.
 */
/**
 * @file config.h
 * @brief Master configuration entry point for the Loki project.
 *
 * This is the single file that board-level tuners and contributors should
 * edit (or include) when adjusting hardware settings.  It pulls together
 * two subordinate headers:
 *
 *   board_config.h  — device frequencies, capacities, timing constants,
 *                     feature flags, and other non-pin parameters.
 *   pinout.h        — all GPIO / SPI / I2C / UART pin-number assignments.
 *
 * Usage
 * -----
 * Any source file that needs configuration values should include this file:
 *
 *   #include "config.h"
 *
 * To change a setting, locate the relevant #define in board_config.h or
 * pinout.h and update it there.  The change will automatically propagate
 * to every module that includes config.h.
 *
 * Extending the config
 * --------------------
 * Add new board-level constants to board_config.h.
 * Add new pin assignments to pinout.h.
 * Do NOT scatter raw numeric literals across driver source files — always
 * define them here and reference the macro instead.
 */

#include "board_config.h"
#include "pinout.h"

#endif /* CONFIG_H */
