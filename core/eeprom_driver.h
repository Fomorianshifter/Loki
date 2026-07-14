#ifndef EEPROM_DRIVER_H
#define EEPROM_DRIVER_H

#include <stdint.h>
#include "hal.h"
#include "board_config.h"

/* ===== EEPROM INTERFACE ===== */

hal_status_t eeprom_init(void);

hal_status_t eeprom_read(uint8_t address, uint8_t *buffer, uint16_t length);

hal_status_t eeprom_write(uint8_t address, const uint8_t *buffer, uint16_t length);

hal_status_t eeprom_deinit(void);

#endif /* EEPROM_DRIVER_H */
