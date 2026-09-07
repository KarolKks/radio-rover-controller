#include "uart.h"

uart_status_t uart_init(void)
{
    // Enable GPIOA clock and configure PA2 (TX)
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    LL_GPIO_InitTypeDef gpio_init;
    LL_GPIO_StructInit(&gpio_init);
    gpio_init.Pin        = LL_GPIO_PIN_2;
    gpio_init.Mode       = LL_GPIO_MODE_ALTERNATE;
    gpio_init.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    gpio_init.Pull       = LL_GPIO_PULL_NO;
    gpio_init.Alternate  = LL_GPIO_AF_7;

    if (LL_GPIO_Init(GPIOA, &gpio_init) != SUCCESS) {
        return UART_ERR_INIT;
    }

    // Enable USART2 clock
    LL_RCC_SetUSARTClockSource(LL_RCC_USART2_CLKSOURCE_PCLK1);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);

    LL_USART_Disable(USART2);

    LL_USART_InitTypeDef usart_init;
    LL_USART_StructInit(&usart_init);
    usart_init.BaudRate          = UART_BAUDRATE;
    usart_init.TransferDirection = LL_USART_DIRECTION_TX;

    if (LL_USART_Init(USART2, &usart_init) != SUCCESS) {
        return UART_ERR_INIT;
    }

    LL_USART_Enable(USART2);

    // Wait until transmitter is ready
    uint32_t timeout = 10000U;
    while (!LL_USART_IsActiveFlag_TEACK(USART2) && (--timeout > 0U)) {
    }

    if (timeout == 0U) {
        return UART_ERR_TIMEOUT;
    }

    return UART_OK;
}

void uart_send_char(char c)
{
    while (!LL_USART_IsActiveFlag_TXE(USART2)) {
    }

    LL_USART_TransmitData8(USART2, (uint8_t)c);
}

void uart_send_string(const char *str)
{
    if (str == NULL) {
        return;
    }

    while (*str != '\0') {
        uart_send_char(*str);
        str++;
    }
}

void uart_send_int(int32_t num)
{
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%ld", (long)num);
    uart_send_string(buffer);
}

void uart_send_float(float num, uint8_t decimals)
{
    if (num < 0.0f) {
        uart_send_char('-');
        num = -num;
    }

    uint32_t int_part = (uint32_t)num;
    uart_send_int((int32_t)int_part);

    if (decimals > 0) {
        uart_send_char('.');
        float frac = num - (float)int_part;

        for (uint8_t i = 0; i < decimals; ++i) {
            frac *= 10.0f;
            uint8_t digit = (uint8_t)frac;
            uart_send_char((char)('0' + digit));
            frac -= (float)digit;
        }
    }
}

// Low-level hook allowing standard printf() to output to UART
int _write(int file, char *ptr, int len)
{
    (void)file;
    if ((ptr == NULL) || (len <= 0)) {
        return 0;
    }

    for (int i = 0; i < len; ++i) {
        uart_send_char(ptr[i]);
    }

    return len;
}
