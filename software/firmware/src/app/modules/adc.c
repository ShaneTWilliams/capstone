#include "adc.h"

#include "hardware/adc.h"
#include "values.h"

#define ADC_INPUT_GPIO_26 (0U)
#define ADC_INPUT_GPIO_27 (1U)
#define ADC_INPUT_GPIO_28 (2U)
#define ADC_INPUT_GPIO_29 (3U)
#define ADC_INPUT_TEMP    (4U)

#define ADC_VREF          (3.3f)
#define ADC_RESOLUTION    (1U << 12U)

#define VOLTS_FROM_RAW(raw)    ((float)(raw) * ADC_VREF / ADC_RESOLUTION)

static void init(void) {
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(ADC_INPUT_TEMP);
}

static void run_100ms(void) {
    uint16_t raw_conv = adc_read();
    float conv_voltage = VOLTS_FROM_RAW(raw_conv);
    values.mcu_temp   = 27 - (((float)conv_voltage - 0.706) / 0.001721);
}

module_t adc_module = {
    .name       = "adc",
    .init       = init,
    .run_1ms    = NULL,
    .run_10ms   = NULL,
    .run_100ms  = run_100ms,
    .run_1000ms = NULL,
};
