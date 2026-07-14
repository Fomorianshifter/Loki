#include "loki_core.h"
#include "eeprom_driver.h"

int loki_eeprom_read(uint8_t address, uint8_t *buffer, uint16_t length) {
    return eeprom_read(address, buffer, length);
}

int loki_eeprom_write(uint8_t address, const uint8_t *buffer, uint16_t length) {
    return eeprom_write(address, buffer, length);
}
