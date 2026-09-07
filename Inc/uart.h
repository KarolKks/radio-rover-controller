#ifndef UART_H
#define UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_usart.h"



#define UART_BAUDRATE   (115200U)

/**
 * @brief Status codes returned by UART functions.
 */
typedef enum {
    UART_OK = 0,
    UART_ERR_INIT,
    UART_ERR_TIMEOUT
} uart_status_t;

/**
 * @brief  Initializes USART2 (PA2 TX) for debug logging at UART_BAUDRATE.
 * @return uart_status_t UART_OK on success, or an error code on failure.
 */
uart_status_t uart_init(void);

/**
 * @brief  Sends a single character over UART.
 * @param[in] c Character to transmit.
 */
void uart_send_char(char c);

/**
 * @brief  Sends a null-terminated string over UART.
 * @param[in] str Pointer to the string to transmit.
 */
void uart_send_string(const char *str);

/**
 * @brief  Sends a signed 32-bit integer as text over UART (e.g. ADC values, stick axis).
 * @param[in] num Integer value to transmit.
 */
void uart_send_int(int32_t num);

/**
 * @brief  Sends a floating-point number as text with specified decimal places.
 * @param[in] num Floating-point value.
 * @param[in] decimals Number of digits after the decimal point.
 */
void uart_send_float(float num, uint8_t decimals);

#ifdef __cplusplus
}
#endif

#endif /* UART_H */
