#include "main.h"
#include "stm32l4xx_ll_utils.h"

int main(void)
{
    // Initialize 1ms SysTick for timing functions
    LL_Init1msTick(SystemCoreClock);

    // Initialize debug UART console (PA2, 115200 baud)
    if (uart_init() != UART_OK) {
        while (1) {
        }
    }

    uart_send_string("       Radio Rover Controller\r\n");

    // Initialize analog stick KY-023 (PA0 VRX, PA1 VRY, PA4 SW)
    if (controller_init() != CONTROLLER_OK) {
        uart_send_string("ERROR: Controller initialization failed!\r\n");
        while (1) {
        }
    }
    uart_send_string("OK: KY-023 Controller calibrated and ready.\r\n");

    // Initialize NRF24L01+ radio (SPI1: PA5/6/7, CSN: PA10, CE: PA9, IRQ: PA8)
    if (nrf24_init() != NRF24_OK) {
        uart_send_string("WARNING: NRF24L01+ not responding on SPI bus!\r\n");
        uart_send_string("Verify pin connections:\r\n");
        uart_send_string("  PA5  -> SCK\r\n");
        uart_send_string("  PA6  -> MISO\r\n");
        uart_send_string("  PA7  -> MOSI\r\n");
        uart_send_string("  PA10 -> CSN\r\n");
        uart_send_string("  PA9  -> CE\r\n");
        uart_send_string("  PA8  -> IRQ\r\n");
        uart_send_string("  3V3  -> VCC\r\n");
        uart_send_string("  GND  -> GND\r\n");
    } else {
        uart_send_string("OK: NRF24L01+ detected and configured.\r\n");
        nrf24_print_details();
    }

    controller_data_t ctrl;
    rover_packet_t packet;
    uint8_t seq = 0;

    while (1)
    {
        // Read latest analog readings and button state
        controller_get_data(&ctrl);

        // Build telemetry packet
        packet.steering = ctrl.steering;
        packet.throttle = ctrl.throttle;
        packet.button   = ctrl.is_button_down ? 1U : 0U;
        packet.sequence = seq++;

        // Send wireless packet if radio is responding
        nrf24_status_t tx_status = nrf24_send_packet(&packet);
        const char *tx_str;
        if (tx_status == NRF24_OK) {
            tx_str = "ACK_OK";
        } else if (tx_status == NRF24_ERR_MAX_RT) {
            tx_str = "NO_ACK";
        } else {
            tx_str = "TX_ERR";
        }

        // Print combined telemetry to serial console
        printf("Seq: %3u | X:%4d%% | Y:%4d%% | BTN:%s | Radio: %s\r\n",
               packet.sequence,
               (int)packet.steering,
               (int)packet.throttle,
               packet.button ? "PRESSED" : "RELEASED",
               tx_str);

        // Notify on button click event
        if (controller_get_button_event()) {
            uart_send_string(">>> EVENT: Stick button clicked! <<<\r\n");
        }

        LL_mDelay(100);
    }
}
