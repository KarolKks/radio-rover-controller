#include "NRF24L01.h"

// NRF24L01+ SPI Commands
#define NRF24_CMD_R_REGISTER            (0x00U)
#define NRF24_CMD_W_REGISTER            (0x20U)
#define NRF24_CMD_R_RX_PAYLOAD          (0x61U)
#define NRF24_CMD_W_TX_PAYLOAD          (0xA0U)
#define NRF24_CMD_FLUSH_TX              (0xE1U)
#define NRF24_CMD_FLUSH_RX              (0xE2U)
#define NRF24_CMD_REUSE_TX_PL           (0xE3U)
#define NRF24_CMD_NOP                   (0xFFU)

// NRF24L01+ Register Map
#define NRF24_REG_CONFIG                (0x00U)
#define NRF24_REG_EN_AA                 (0x01U)
#define NRF24_REG_EN_RXADDR             (0x02U)
#define NRF24_REG_SETUP_AW              (0x03U)
#define NRF24_REG_SETUP_RETR            (0x04U)
#define NRF24_REG_RF_CH                 (0x05U)
#define NRF24_REG_RF_SETUP              (0x06U)
#define NRF24_REG_STATUS                (0x07U)
#define NRF24_REG_OBSERVE_TX            (0x08U)
#define NRF24_REG_RPD                   (0x09U)
#define NRF24_REG_RX_ADDR_P0            (0x0AU)
#define NRF24_REG_TX_ADDR               (0x10U)
#define NRF24_REG_RX_PW_P0              (0x11U)
#define NRF24_REG_FIFO_STATUS           (0x17U)
#define NRF24_REG_DYNPD                 (0x1CU)
#define NRF24_REG_FEATURE               (0x1DU)

// CONFIG Register Bit Masks
#define NRF24_CONFIG_MASK_RX_DR         (1U << 6)
#define NRF24_CONFIG_MASK_TX_DS         (1U << 5)
#define NRF24_CONFIG_MASK_MAX_RT        (1U << 4)
#define NRF24_CONFIG_EN_CRC             (1U << 3)
#define NRF24_CONFIG_CRCO               (1U << 2)
#define NRF24_CONFIG_PWR_UP             (1U << 1)
#define NRF24_CONFIG_PRIM_RX            (1U << 0)

// STATUS Register Bit Masks
#define NRF24_STATUS_RX_DR              (1U << 6)
#define NRF24_STATUS_TX_DS              (1U << 5)
#define NRF24_STATUS_MAX_RT             (1U << 4)
#define NRF24_STATUS_TX_FULL            (1U << 0)

// Default Radio Configuration
#define NRF24_DEFAULT_CHANNEL           (76U)
#define NRF24_ADDR_WIDTH                (5U)

// Default target rover radio address
static const uint8_t s_default_addr[NRF24_ADDR_WIDTH] = { 'R', 'O', 'V', '0', '1' };

// Control CSN (Chip Select Not) on PA10
static inline void nrf24_csn_high(void)
{
    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_10);
}

static inline void nrf24_csn_low(void)
{
    LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_10);
}

// Control CE (Chip Enable) on PA9
static inline void nrf24_ce_high(void)
{
    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_9);
}

static inline void nrf24_ce_low(void)
{
    LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_9);
}

// Read single 8-bit register
static uint8_t nrf24_read_reg(uint8_t reg)
{
    nrf24_csn_low();
    spi_transfer_byte(NRF24_CMD_R_REGISTER | (reg & 0x1FU));
    uint8_t val = spi_transfer_byte(NRF24_CMD_NOP);
    nrf24_csn_high();
    return val;
}

// Write single 8-bit register
static void nrf24_write_reg(uint8_t reg, uint8_t val)
{
    nrf24_csn_low();
    spi_transfer_byte(NRF24_CMD_W_REGISTER | (reg & 0x1FU));
    spi_transfer_byte(val);
    nrf24_csn_high();
}

// Read multi-byte register buffer (e.g., addresses)
static void nrf24_read_reg_buf(uint8_t reg, uint8_t *buf, uint8_t len)
{
    nrf24_csn_low();
    spi_transfer_byte(NRF24_CMD_R_REGISTER | (reg & 0x1FU));
    spi_transfer_buffer(NULL, buf, len);
    nrf24_csn_high();
}

// Write multi-byte register buffer
static void nrf24_write_reg_buf(uint8_t reg, const uint8_t *buf, uint8_t len)
{
    nrf24_csn_low();
    spi_transfer_byte(NRF24_CMD_W_REGISTER | (reg & 0x1FU));
    spi_write_buffer(buf, len);
    nrf24_csn_high();
}

// Send single command byte to NRF24
static void nrf24_send_cmd(uint8_t cmd)
{
    nrf24_csn_low();
    spi_transfer_byte(cmd);
    nrf24_csn_high();
}

nrf24_status_t nrf24_init(void)
{
    // Enable GPIOA clock for control pins (PA8=IRQ, PA9=CE, PA10=CSN)
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    // Configure PA9 (CE) and PA10 (CSN) as push-pull outputs
    LL_GPIO_InitTypeDef gpio_init;
    LL_GPIO_StructInit(&gpio_init);
    gpio_init.Pin        = LL_GPIO_PIN_9 | LL_GPIO_PIN_10;
    gpio_init.Mode       = LL_GPIO_MODE_OUTPUT;
    gpio_init.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    gpio_init.Pull       = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIOA, &gpio_init);

    // Configure PA8 (IRQ) as input with pull-up
    gpio_init.Pin        = LL_GPIO_PIN_8;
    gpio_init.Mode       = LL_GPIO_MODE_INPUT;
    gpio_init.Pull       = LL_GPIO_PULL_UP;
    LL_GPIO_Init(GPIOA, &gpio_init);

    // Initial pin states: CE low (standby), CSN high (SPI unselected)
    nrf24_ce_low();
    nrf24_csn_high();

    // Initialize underlying SPI1 master bus
    if (spi_init() != SPI_OK) {
        return NRF24_ERR_NOT_FOUND;
    }

    // Wait for radio internal power-on reset stabilization
    LL_mDelay(10);

    // Verify radio presence by writing and reading back address width register
    nrf24_write_reg(NRF24_REG_SETUP_AW, 0x03U);
    if (nrf24_read_reg(NRF24_REG_SETUP_AW) != 0x03U) {
        return NRF24_ERR_NOT_FOUND;
    }

    // Configure 1500us auto-retransmit delay with 15 retries
    nrf24_write_reg(NRF24_REG_SETUP_RETR, 0x5FU);

    // Set default RF channel (2476 MHz)
    nrf24_set_channel(NRF24_DEFAULT_CHANNEL);

    // Set maximum RF output power (0 dBm) and 1 Mbps data rate
    nrf24_set_pa_level(NRF24_PA_MAX);
    nrf24_set_data_rate(NRF24_RATE_1MBPS);

    // Enable auto-acknowledgement on pipe 0
    nrf24_write_reg(NRF24_REG_EN_AA, 0x01U);

    // Enable RX address on pipe 0
    nrf24_write_reg(NRF24_REG_EN_RXADDR, 0x01U);

    // Set default target transmit and pipe 0 receive addresses
    nrf24_set_tx_address(s_default_addr);

    // Clear all pending interrupt flags in status register
    nrf24_write_reg(NRF24_REG_STATUS, NRF24_STATUS_RX_DR | NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT);

    // Flush FIFOs to start with clean state
    nrf24_flush_tx();
    nrf24_flush_rx();

    // Power up in TX mode with 2-byte CRC enabled
    nrf24_write_reg(NRF24_REG_CONFIG, NRF24_CONFIG_EN_CRC | NRF24_CONFIG_CRCO | NRF24_CONFIG_PWR_UP);

    // Wait for oscillator to stabilize in Standby-I mode
    LL_mDelay(2);

    return NRF24_OK;
}

bool nrf24_is_connected(void)
{
    // Test SPI communication via setup address width register
    nrf24_write_reg(NRF24_REG_SETUP_AW, 0x03U);
    return (nrf24_read_reg(NRF24_REG_SETUP_AW) == 0x03U);
}

void nrf24_set_tx_address(const uint8_t *addr)
{
    if (addr == NULL) {
        return;
    }

    // Pipe 0 RX address must match TX address for auto-ACK reception
    nrf24_write_reg_buf(NRF24_REG_TX_ADDR, addr, NRF24_ADDR_WIDTH);
    nrf24_write_reg_buf(NRF24_REG_RX_ADDR_P0, addr, NRF24_ADDR_WIDTH);
}

void nrf24_set_channel(uint8_t channel)
{
    // Clamp channel to maximum valid value (0 - 125)
    if (channel > 125U) {
        channel = 125U;
    }
    nrf24_write_reg(NRF24_REG_RF_CH, channel);
}

void nrf24_set_pa_level(nrf24_pa_level_t level)
{
    uint8_t setup = nrf24_read_reg(NRF24_REG_RF_SETUP) & 0xF9U;

    // Configure RF power output bits
    switch (level) {
        case NRF24_PA_MIN:
            setup |= (0x00U << 1);
            break;
        case NRF24_PA_LOW:
            setup |= (0x01U << 1);
            break;
        case NRF24_PA_HIGH:
            setup |= (0x02U << 1);
            break;
        case NRF24_PA_MAX:
        default:
            setup |= (0x03U << 1);
            break;
    }

    nrf24_write_reg(NRF24_REG_RF_SETUP, setup);
}

void nrf24_set_data_rate(nrf24_data_rate_t rate)
{
    uint8_t setup = nrf24_read_reg(NRF24_REG_RF_SETUP) & ~((1U << 5) | (1U << 3));

    // Configure data rate bits (RF_DR_LOW, RF_DR_HIGH)
    switch (rate) {
        case NRF24_RATE_250KBPS:
            setup |= (1U << 5);
            break;
        case NRF24_RATE_2MBPS:
            setup |= (1U << 3);
            break;
        case NRF24_RATE_1MBPS:
        default:
            break;
    }

    nrf24_write_reg(NRF24_REG_RF_SETUP, setup);
}

nrf24_status_t nrf24_send_packet(const rover_packet_t *packet)
{
    if (packet == NULL) {
        return NRF24_ERR_PARAM;
    }

    // Transmit telemetry struct as raw payload buffer
    return nrf24_send_raw((const uint8_t *)packet, sizeof(rover_packet_t));
}

nrf24_status_t nrf24_send_raw(const uint8_t *data, uint8_t length)
{
    if (data == NULL || length == 0U || length > 32U) {
        return NRF24_ERR_PARAM;
    }

    // Ensure transmitter mode and power up
    uint8_t cfg = nrf24_read_reg(NRF24_REG_CONFIG);
    cfg &= ~NRF24_CONFIG_PRIM_RX;
    cfg |= NRF24_CONFIG_PWR_UP;
    nrf24_write_reg(NRF24_REG_CONFIG, cfg);

    // Load payload into TX FIFO
    nrf24_csn_low();
    spi_transfer_byte(NRF24_CMD_W_TX_PAYLOAD);
    spi_write_buffer(data, length);
    nrf24_csn_high();

    // Pulse CE high for at least 15 microseconds to trigger transmission
    nrf24_ce_high();
    for (volatile uint32_t delay = 0; delay < 200U; delay++) {
    }
    nrf24_ce_low();

    // Poll status register until transmission succeeds, fails, or times out
    uint32_t timeout = 50000U;
    uint8_t status = 0;

    while (--timeout > 0U) {
        status = nrf24_read_reg(NRF24_REG_STATUS);
        if (status & (NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT)) {
            break;
        }
    }

    // Clear interrupt flags in radio
    nrf24_write_reg(NRF24_REG_STATUS, NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT);

    // Handle timeout error
    if (timeout == 0U) {
        nrf24_flush_tx();
        return NRF24_ERR_TIMEOUT;
    }

    // Handle maximum retries reached (no ACK received)
    if (status & NRF24_STATUS_MAX_RT) {
        nrf24_flush_tx();
        return NRF24_ERR_MAX_RT;
    }

    return NRF24_OK;
}

void nrf24_flush_tx(void)
{
    // Flush TX FIFO
    nrf24_send_cmd(NRF24_CMD_FLUSH_TX);
}

void nrf24_flush_rx(void)
{
    // Flush RX FIFO
    nrf24_send_cmd(NRF24_CMD_FLUSH_RX);
}

void nrf24_print_details(void)
{
    // Read configuration registers for diagnostics
    uint8_t cfg    = nrf24_read_reg(NRF24_REG_CONFIG);
    uint8_t status = nrf24_read_reg(NRF24_REG_STATUS);
    uint8_t rf_ch  = nrf24_read_reg(NRF24_REG_RF_CH);
    uint8_t setup  = nrf24_read_reg(NRF24_REG_RF_SETUP);
    uint8_t retr   = nrf24_read_reg(NRF24_REG_SETUP_RETR);

    uint8_t tx_addr[NRF24_ADDR_WIDTH] = {0};
    nrf24_read_reg_buf(NRF24_REG_TX_ADDR, tx_addr, NRF24_ADDR_WIDTH);

    // Print formatted parameters to UART console
    printf("\r\n--- NRF24L01+ Configuration ---\r\n");
    printf("CONFIG:     0x%02X\r\n", cfg);
    printf("STATUS:     0x%02X\r\n", status);
    printf("RF_CH:      %u (Frequency: %u MHz)\r\n", rf_ch, 2400U + rf_ch);
    printf("RF_SETUP:   0x%02X\r\n", setup);
    printf("SETUP_RETR: 0x%02X (Delay: %uus, Retries: %u)\r\n",
           retr, ((retr >> 4) + 1U) * 250U, retr & 0x0FU);
    printf("TX_ADDR:    %c%c%c%c%c (0x%02X 0x%02X 0x%02X 0x%02X 0x%02X)\r\n",
           tx_addr[0], tx_addr[1], tx_addr[2], tx_addr[3], tx_addr[4],
           tx_addr[0], tx_addr[1], tx_addr[2], tx_addr[3], tx_addr[4]);
    printf("-------------------------------\r\n");
}
