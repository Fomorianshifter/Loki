/**
 * Unified Pinout Abstraction
 * Auto-selects platform-specific pinout at compile time
 */

#ifndef PINOUT_UNIFIED_H
#define PINOUT_UNIFIED_H

#ifdef BOARD_RASPBERRY_PI
    #include "pinout_rpi.h"
#elif defined(BOARD_ORANGE_PI_ZERO_2W)
    #include "pinout_opi.h"
#else
    #error "No board defined. Use -DBOARD_RASPBERRY_PI or -DBOARD_ORANGE_PI_ZERO_2W"
#endif

/* ===== COMMON DEVICE ADDRESSES ===== */
#ifndef EEPROM_ADDR
    #define EEPROM_ADDR 0x50
#endif

#ifndef PWM_TFT_BACKLIGHT
    #define PWM_TFT_BACKLIGHT GPIO_TFT_BL
#endif

#ifndef PWM_FREQ_DEFAULT
    #define PWM_FREQ_DEFAULT 1000
#endif

#endif /* PINOUT_UNIFIED_H */
