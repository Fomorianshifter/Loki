#ifndef SPI_H
#define SPI_H

#include <stdint.h>

typedef struct {
    uint32_t frequency;
    uint8_t mode;
    uint8_t bits_per_word;
    uint8_t bit_order;
} spi_config_t;

#define SPI_MODE_0      0
#define SPI_MODE_1      1
#define SPI_MODE_2      2
#define SPI_MODE_3      3

#define SPI_MSB_FIRST   0
#define SPI_LSB_FIRST   1

#define SPI_BUS_2 2
#define SPI2_CS0 0

int spi_init(int bus, spi_config_t *cfg);
int spi_write(int bus, int cs, const uint8_t *data, uint32_t len);
int spi_transfer(int bus, int cs,
                 const uint8_t *tx, uint32_t tx_len,
                 uint8_t *rx, uint32_t rx_len);
int spi_deinit(int bus);

#endif
