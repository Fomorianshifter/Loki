#ifndef LOKI_CORE_H
#define LOKI_CORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Minimal hardware API we expose to Python
int loki_eeprom_read(uint8_t address, uint8_t *buffer, uint16_t length);
int loki_eeprom_write(uint8_t address, const uint8_t *buffer, uint16_t length);

// You’ll later add: wifi_scan, wifi_sniff, etc.

#ifdef __cplusplus
}
#endif

#endif
