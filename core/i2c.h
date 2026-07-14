#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include "hal.h"

typedef struct {
    uint32_t frequency;
    uint8_t address_bits;
} i2c_config_t;

#define I2C_BUS_0 0
#define I2C_BUS_1 1

hal_status_t i2c_init(int bus, const i2c_config_t *cfg);
hal_status_t i2c_deinit(int bus);

hal_status_t i2c_read(int bus, uint8_t addr, uint8_t *buf, uint16_t len);
hal_status_t i2c_write(int bus, uint8_t addr, const uint8_t *buf, uint16_t len);

/* write register, then read */
hal_status_t i2c_write_read(int bus, uint8_t addr,
                            const uint8_t *tx, uint16_t tx_len,
                            uint8_t *rx, uint16_t rx_len);

#endif
