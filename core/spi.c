#include "spi.h"
#include <string.h>

int spi_init(int bus, spi_config_t *cfg) {
    return 0;
}

int spi_write(int bus, int cs, const uint8_t *data, uint32_t len) {
    return 0;
}

int spi_transfer(int bus, int cs,
                 const uint8_t *tx, uint32_t tx_len,
                 uint8_t *rx, uint32_t rx_len) {
    if (rx && tx) {
        uint32_t n = tx_len < rx_len ? tx_len : rx_len;
        memcpy(rx, tx, n);  // simple echo stub
    }
    return 0;
}

int spi_deinit(int bus) {
    return 0;
}
