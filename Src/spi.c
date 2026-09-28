#include "spi.h"

#define SPI_TIMEOUT_COUNT   (100000U)
#define SPI_DUMMY_BYTE      (0xFFU)

spi_status_t spi_init(void)
{
    // Enable GPIOA peripheral clock for SPI1 pins
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    // Configure PA5 (SCK) and PA7 (MOSI) as high-speed alternate function push-pull
    LL_GPIO_InitTypeDef gpio_init;
    LL_GPIO_StructInit(&gpio_init);
    gpio_init.Pin        = LL_GPIO_PIN_5 | LL_GPIO_PIN_7;
    gpio_init.Mode       = LL_GPIO_MODE_ALTERNATE;
    gpio_init.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    gpio_init.Pull       = LL_GPIO_PULL_NO;
    gpio_init.Alternate  = LL_GPIO_AF_5;

    if (LL_GPIO_Init(GPIOA, &gpio_init) != SUCCESS) {
        return SPI_ERR_INIT;
    }

    // Configure PA6 (MISO) as alternate function with pull-up to prevent floating states
    gpio_init.Pin        = LL_GPIO_PIN_6;
    gpio_init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    gpio_init.Pull       = LL_GPIO_PULL_UP;
    gpio_init.Alternate  = LL_GPIO_AF_5;

    if (LL_GPIO_Init(GPIOA, &gpio_init) != SUCCESS) {
        return SPI_ERR_INIT;
    }

    // Enable SPI1 peripheral clock on APB2 bus
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

    // Ensure SPI1 is disabled before configuring parameters
    LL_SPI_Disable(SPI1);

    // Configure SPI1 parameters: Master, Mode 0 (CPOL=0, CPHA=0), 8-bit, Software NSS
    LL_SPI_InitTypeDef spi_cfg;
    LL_SPI_StructInit(&spi_cfg);
    spi_cfg.TransferDirection = LL_SPI_FULL_DUPLEX;
    spi_cfg.Mode              = LL_SPI_MODE_MASTER;
    spi_cfg.DataWidth         = LL_SPI_DATAWIDTH_8BIT;
    spi_cfg.ClockPolarity     = LL_SPI_POLARITY_LOW;
    spi_cfg.ClockPhase        = LL_SPI_PHASE_1EDGE;
    spi_cfg.NSS               = LL_SPI_NSS_SOFT;
    spi_cfg.BaudRate          = LL_SPI_BAUDRATEPRESCALER_DIV16;
    spi_cfg.BitOrder          = LL_SPI_MSB_FIRST;
    spi_cfg.CRCCalculation    = LL_SPI_CRCCALCULATION_DISABLE;

    if (LL_SPI_Init(SPI1, &spi_cfg) != SUCCESS) {
        return SPI_ERR_INIT;
    }

    // Set RX FIFO threshold to 8-bit so RXNE flag is triggered on each single byte
    LL_SPI_SetRxFIFOThreshold(SPI1, LL_SPI_RX_FIFO_TH_QUARTER);

    // Enable SPI1 peripheral
    LL_SPI_Enable(SPI1);

    return SPI_OK;
}

uint8_t spi_transfer_byte(uint8_t tx_data)
{
    uint32_t timeout = SPI_TIMEOUT_COUNT;

    // Wait until transmit FIFO buffer has space (TXE flag)
    while (!LL_SPI_IsActiveFlag_TXE(SPI1) && (--timeout > 0U)) {
    }

    if (timeout == 0U) {
        return SPI_DUMMY_BYTE;
    }

    // Transmit 8-bit byte to SPI data register
    LL_SPI_TransmitData8(SPI1, tx_data);

    // Wait until receive FIFO contains received data (RXNE flag)
    timeout = SPI_TIMEOUT_COUNT;
    while (!LL_SPI_IsActiveFlag_RXNE(SPI1) && (--timeout > 0U)) {
    }

    if (timeout == 0U) {
        return SPI_DUMMY_BYTE;
    }

    // Return received byte from data register
    return LL_SPI_ReceiveData8(SPI1);
}

spi_status_t spi_transfer_buffer(const uint8_t *tx_buf, uint8_t *rx_buf, uint16_t length)
{
    if (length == 0U) {
        return SPI_OK;
    }

    // Perform full-duplex transfer byte by byte
    for (uint16_t i = 0; i < length; i++) {
        uint8_t tx = (tx_buf != NULL) ? tx_buf[i] : SPI_DUMMY_BYTE;
        uint8_t rx = spi_transfer_byte(tx);

        if (rx_buf != NULL) {
            rx_buf[i] = rx;
        }
    }

    // Wait until transmission completes and the SPI bus is idle (BSY flag clear)
    uint32_t timeout = SPI_TIMEOUT_COUNT;
    while (LL_SPI_IsActiveFlag_BSY(SPI1) && (--timeout > 0U)) {
    }

    if (timeout == 0U) {
        return SPI_ERR_TIMEOUT;
    }

    return SPI_OK;
}

spi_status_t spi_write_buffer(const uint8_t *tx_buf, uint16_t length)
{
    if (tx_buf == NULL && length > 0U) {
        return SPI_ERR_PARAM;
    }

    // Write data buffer while discarding received bytes
    return spi_transfer_buffer(tx_buf, NULL, length);
}

spi_status_t spi_read_buffer(uint8_t *rx_buf, uint16_t length)
{
    if (rx_buf == NULL && length > 0U) {
        return SPI_ERR_PARAM;
    }

    // Read data buffer by sending dummy 0xFF bytes
    return spi_transfer_buffer(NULL, rx_buf, length);
}
