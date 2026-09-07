#include "KY-023.h"

#define CONTROLLER_INVERT_X         (0)
#define CONTROLLER_INVERT_Y         (0)

// DMA buffer filled in background by ADC1 (Index 0: VRX PA0, Index 1: VRY PA1)
static volatile uint16_t s_adc_raw[2] = { 2048U, 2048U };
static volatile bool s_button_event_flag = false;

static uint16_t s_center_x = 2048U;
static uint16_t s_center_y = 2048U;

static int8_t map_adc_to_percent(uint16_t adc_val, uint16_t center)
{
    int32_t diff = (int32_t)adc_val - (int32_t)center;

    if ((diff >= -(int32_t)CONTROLLER_DEADZONE_ADC) && (diff <= (int32_t)CONTROLLER_DEADZONE_ADC)) {
        return 0;
    }

    if (diff > 0) {
        int32_t span = (int32_t)(CONTROLLER_ADC_MAX - center - CONTROLLER_DEADZONE_ADC);
        if (span <= 0) {
            return 100;
        }
        int32_t val = (diff - (int32_t)CONTROLLER_DEADZONE_ADC) * 100 / span;
        if (val > 100) {
            val = 100;
        }
        return (int8_t)val;
    } else {
        int32_t span = (int32_t)(center - CONTROLLER_DEADZONE_ADC);
        if (span <= 0) {
            return -100;
        }
        int32_t val = (diff + (int32_t)CONTROLLER_DEADZONE_ADC) * 100 / span;
        if (val < -100) {
            val = -100;
        }
        return (int8_t)val;
    }
}

void controller_calibrate_center(void)
{
    uint32_t sum_x = 0U;
    uint32_t sum_y = 0U;

    for (uint32_t i = 0; i < 32U; ++i) {
        sum_x += s_adc_raw[0];
        sum_y += s_adc_raw[1];
        for (volatile uint32_t d = 0; d < 2000U; ++d) {
            __NOP();
        }
    }

    s_center_x = (uint16_t)(sum_x / 32U);
    s_center_y = (uint16_t)(sum_y / 32U);
}

controller_status_t controller_init(void)
{
    // Configure ADC clock source to SYSCLK
    LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_SYSCLK);

    // Enable clocks for GPIOA, SYSCFG, DMA1, and ADC
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_ADC);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);

    // Configure PA0 (VRX) and PA1 (VRY) as analog pins
    LL_GPIO_InitTypeDef gpio_analog;
    LL_GPIO_StructInit(&gpio_analog);
    gpio_analog.Pin  = LL_GPIO_PIN_0 | LL_GPIO_PIN_1;
    gpio_analog.Mode = LL_GPIO_MODE_ANALOG;
    gpio_analog.Pull = LL_GPIO_PULL_NO;
    if (LL_GPIO_Init(GPIOA, &gpio_analog) != SUCCESS) {
        return CONTROLLER_ERR_INIT;
    }

    // Connect internal analog switch for PA0 and PA1 to ADC (required on STM32L4)
    LL_GPIO_EnablePinAnalogControl(GPIOA, LL_GPIO_PIN_0 | LL_GPIO_PIN_1);

    // Configure PA4 (SW button) with internal pull-up
    LL_GPIO_InitTypeDef gpio_btn;
    LL_GPIO_StructInit(&gpio_btn);
    gpio_btn.Pin  = LL_GPIO_PIN_4;
    gpio_btn.Mode = LL_GPIO_MODE_INPUT;
    gpio_btn.Pull = LL_GPIO_PULL_UP;
    if (LL_GPIO_Init(GPIOA, &gpio_btn) != SUCCESS) {
        return CONTROLLER_ERR_INIT;
    }

    // Route PA4 to EXTI Line 4 with falling edge trigger (press down)
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE4);
    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_4);
    LL_EXTI_EnableFallingTrig_0_31(LL_EXTI_LINE_4);

    NVIC_SetPriority(EXTI4_IRQn, 6);
    NVIC_EnableIRQ(EXTI4_IRQn);

    // Configure DMA1 Channel 1 for ADC1 circular transfer
    LL_DMA_ConfigTransfer(DMA1, LL_DMA_CHANNEL_1,
                          LL_DMA_DIRECTION_PERIPH_TO_MEMORY |
                          LL_DMA_MODE_CIRCULAR |
                          LL_DMA_PERIPH_NOINCREMENT |
                          LL_DMA_MEMORY_INCREMENT |
                          LL_DMA_PDATAALIGN_HALFWORD |
                          LL_DMA_MDATAALIGN_HALFWORD |
                          LL_DMA_PRIORITY_HIGH);

    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_1, LL_DMA_REQUEST_0);

    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1,
                           LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA),
                           (uint32_t)s_adc_raw,
                           LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, 2);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

    // Select ADC common clock (synchronous PCLK divided by 1)
    LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_CLOCK_SYNC_PCLK_DIV1);

    // Exit ADC deep power down mode and enable voltage regulator
    LL_ADC_DisableDeepPowerDown(ADC1);
    LL_ADC_EnableInternalRegulator(ADC1);

    // Wait for regulator stabilization delay (~20us)
    for (volatile uint32_t i = 0; i < 3000U; ++i) {
        __NOP();
    }

    // Run ADC calibration
    LL_ADC_StartCalibration(ADC1, LL_ADC_SINGLE_ENDED);
    uint32_t timeout = 50000U;
    while (LL_ADC_IsCalibrationOnGoing(ADC1) && (--timeout > 0U)) {
    }
    if (timeout == 0U) {
        return CONTROLLER_ERR_TIMEOUT;
    }

    // Configure scan sequence: Rank 1 -> Channel 5 (PA0), Rank 2 -> Channel 6 (PA1)
    LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_ENABLE_2RANKS);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_5);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, LL_ADC_CHANNEL_6);

    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_5, LL_ADC_SAMPLINGTIME_47CYCLES_5);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_6, LL_ADC_SAMPLINGTIME_47CYCLES_5);

    // Configure continuous conversion, overwrite on overrun, and circular DMA
    LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_CONTINUOUS);
    LL_ADC_REG_SetOverrun(ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
    LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_UNLIMITED);

    // Enable ADC peripheral
    LL_ADC_ClearFlag_ADRDY(ADC1);
    LL_ADC_Enable(ADC1);
    timeout = 50000U;
    while (!LL_ADC_IsActiveFlag_ADRDY(ADC1) && (--timeout > 0U)) {
    }
    if (timeout == 0U) {
        return CONTROLLER_ERR_TIMEOUT;
    }

    // Start background conversions
    LL_ADC_REG_StartConversion(ADC1);

    // Auto-calibrate center resting position at startup
    controller_calibrate_center();

    return CONTROLLER_OK;
}

void controller_get_data(controller_data_t *data)
{
    if (data == NULL) {
        return;
    }

    // Read current values directly from background DMA buffer
    data->raw_x = s_adc_raw[0];
    data->raw_y = s_adc_raw[1];

    int8_t x_scaled = map_adc_to_percent(data->raw_x, s_center_x);
    int8_t y_scaled = map_adc_to_percent(data->raw_y, s_center_y);

#if (CONTROLLER_INVERT_X == 1)
    x_scaled = (int8_t)(-x_scaled);
#endif

#if (CONTROLLER_INVERT_Y == 1)
    y_scaled = (int8_t)(-y_scaled);
#endif

    data->steering = x_scaled;
    data->throttle = y_scaled;
    data->is_button_down = controller_is_button_down();
}

bool controller_is_button_down(void)
{
    // Pin PA4 is pulled up; active state is LOW
    return (LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_4) == 0U);
}

bool controller_get_button_event(void)
{
    if (s_button_event_flag) {
        s_button_event_flag = false;
        return true;
    }
    return false;
}

// Hardware interrupt vector for button SW on PA4
void EXTI4_IRQHandler(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_4)) {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_4);
        s_button_event_flag = true;
    }
}
