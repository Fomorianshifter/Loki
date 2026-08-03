#include "i2c.h"

hal_status_t i2c_init(int bus, const i2c_config_t *cfg) {
    return HAL_OK;
}

hal_status_t i2c_deinit(int bus) {
    return HAL_OK;
}

hal_status_t i2c_read(int bus, uint8_t addr, uint8_t *buf, uint16_t len) {
    return HAL_OK;
}

hal_status_t i2c_write(int bus, uint8_t addr, const uint8_t *buf, uint16_t len) {
    return HAL_OK;
}

hal_status_t i2c_write_read(int bus, uint8_t addr,
                            const uint8_t *tx, uint16_t tx_len,
                            uint8_t *rx, uint16_t rx_len) {
    return HAL_OK;
}
