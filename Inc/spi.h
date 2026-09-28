#ifndef SPI_H
#define SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_spi.h"

/**
 * @brief Status codes returned by SPI functions.
 */
typedef enum {
    SPI_OK = 0,
    SPI_ERR_INIT,
    SPI_ERR_TIMEOUT,
    SPI_ERR_PARAM
} spi_status_t;

/**
 * @brief  Initializes SPI1 peripheral as Master in Mode 0 (CPOL=0, CPHA=0)
 *         and configures GPIO pins PA5 (SCK), PA6 (MISO), PA7 (MOSI).
 * @return spi_status_t SPI_OK on success, or error code on failure.
 */
spi_status_t spi_init(void);

/**
 * @brief  Transmits and receives a single byte over SPI1 simultaneously.
 * @param  tx_data Byte to transmit.
 * @return Received byte from slave.
 */
uint8_t spi_transfer_byte(uint8_t tx_data);

/**
 * @brief  Transfers a buffer of data over SPI1 (simultaneous full-duplex).
 * @param  tx_buf Pointer to data buffer to transmit (if NULL, 0xFF dummy bytes are sent).
 * @param  rx_buf Pointer to buffer where received data will be stored (if NULL, received data is discarded).
 * @param  length Number of bytes to transfer.
 * @return spi_status_t SPI_OK on success, or error code on failure.
 */
spi_status_t spi_transfer_buffer(const uint8_t *tx_buf, uint8_t *rx_buf, uint16_t length);

/**
 * @brief  Writes a buffer of data over SPI1, discarding received bytes.
 * @param  tx_buf Pointer to data buffer to transmit.
 * @param  length Number of bytes to write.
 * @return spi_status_t SPI_OK on success, or error code on failure.
 */
spi_status_t spi_write_buffer(const uint8_t *tx_buf, uint16_t length);

/**
 * @brief  Reads data into a buffer over SPI1 by transmitting dummy 0xFF bytes.
 * @param  rx_buf Pointer to buffer where received data will be stored.
 * @param  length Number of bytes to read.
 * @return spi_status_t SPI_OK on success, or error code on failure.
 */
spi_status_t spi_read_buffer(uint8_t *rx_buf, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* SPI_H */
