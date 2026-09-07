#include "main.h"
#include "stm32l4xx_ll_utils.h"

int main(void)
{
    // Initialize 1ms SysTick for LL_mDelay based on core clock
    LL_Init1msTick(SystemCoreClock);

    // Initialize debug UART (PA2, 115200 baud)
    if (uart_init() != UART_OK) {
        while (1) {
        }
    }

    // Initialize analog controller (KY-023: PA0, PA1 via ADC1+DMA1, PA4 via EXTI4)
    if (controller_init() != CONTROLLER_OK) {
        uart_send_string("ERROR: Controller initialization failed!\r\n");
        while (1) {
        }
    }

    uart_send_string("\r\n--- Radio Rover Controller - KY-023 Test ---\r\n");

    controller_data_t ctrl;

    while (1)
    {
        // Read latest continuous DMA values and button state
        controller_get_data(&ctrl);

        printf("X: %4u [Steer: %4d%%] | Y: %4u [Throttle: %4d%%] | BTN: %s\r\n",
               ctrl.raw_x, (int)ctrl.steering,
               ctrl.raw_y, (int)ctrl.throttle,
               ctrl.is_button_down ? "PRESSED" : "RELEASED");

        // Check if a hardware interrupt button event was detected
        if (controller_get_button_event()) {
            uart_send_string(">>> EVENT: Button clicked! <<<\r\n");
        }

        LL_mDelay(100);
    }
}
