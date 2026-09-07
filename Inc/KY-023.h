#ifndef KY_023_H
#define KY_023_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_adc.h"
#include "stm32l4xx_ll_dma.h"
#include "stm32l4xx_ll_exti.h"
#include "stm32l4xx_ll_system.h"
#include <stddef.h>

#define CONTROLLER_DEADZONE_ADC     (150U)
#define CONTROLLER_ADC_MAX          (4095U)

/**
 * @brief Status codes returned by controller functions.
 */
typedef enum {
    CONTROLLER_OK = 0,
    CONTROLLER_ERR_INIT,
    CONTROLLER_ERR_TIMEOUT
} controller_status_t;

/**
 * @brief Processed and raw controller state.
 */
typedef struct {
    uint16_t raw_x;         /*!< Raw ADC value for X axis (0 - 4095) */
    uint16_t raw_y;         /*!< Raw ADC value for Y axis (0 - 4095) */
    int8_t   steering;      /*!< Normalized steering: -100 (left) to +100 (right) */
    int8_t   throttle;      /*!< Normalized throttle: -100 (reverse) to +100 (forward) */
    bool     is_button_down;/*!< Current physical button state (true if pressed) */
} controller_data_t;

/**
 * @brief  Initializes ADC1 (PA0, PA1) with circular DMA1, PA4 button with EXTI4,
 *         and auto-calibrates center position based on startup resting state.
 * @return controller_status_t CONTROLLER_OK on success, or error code on failure.
 */
controller_status_t controller_init(void);

/**
 * @brief  Reads the latest controller state (raw ADC, normalized values, and button).
 * @param[out] data Pointer to destination structure where results will be written.
 */
void controller_get_data(controller_data_t *data);

/**
 * @brief  Recalibrates the center resting position for X and Y axes.
 */
void controller_calibrate_center(void);

/**
 * @brief  Returns the current physical state of the controller button.
 * @return bool true if button is currently pressed down, false otherwise.
 */
bool controller_is_button_down(void);

/**
 * @brief  Checks and clears the button press event flag set by EXTI interrupt.
 * @return bool true if a new press event occurred since last check, false otherwise.
 */
bool controller_get_button_event(void);

#ifdef __cplusplus
}
#endif

#endif /* KY_023_H */
