#include "adc.h"

#include "fatal.h"
#include "generated/values.h"
#include "hardware/adc.h"
#include "hw_config.h"
#include "iox.h"
#include "pico/stdlib.h"

#define ADC_VREF       (3.3f)
#define ADC_RESOLUTION (1U << 12U)

#define VOLTS_FROM_RAW(raw) ((float)(raw)*ADC_VREF / (float)ADC_RESOLUTION)

typedef enum {
    MCU_SELECT_MOTOR_CS = 0,
    MCU_SELECT_LV_CS    = 1,
    MCU_SELECT_TEMP     = 4,
} mcu_select_t;

typedef enum {
    EXTERNAL_SELECT_3V3_CS_AND_MOTOR_FR_CS = 0,
    EXTERNAL_SELECT_MOTOR_FL_CS            = 1,
    EXTERNAL_SELECT_5V_CS_AND_MOTOR_RL_CS  = 2,
    EXTERNAL_SELECT_MOTOR_RR_CS            = 3,
} external_select_t;
static external_select_t current_sel = EXTERNAL_SELECT_3V3_CS_AND_MOTOR_FR_CS;

static void write_sels(external_select_t sel_val) {
    set_iox_gpio_output_state(IOX_GPIO_MUX_A0, sel_val & 0x01);
    set_iox_gpio_output_state(IOX_GPIO_MUX_A1, sel_val & 0x02);
    current_sel = sel_val;  // Safe assumption - IOX updates in 10ms task but we update in 100ms.
}

static void init(void) {
    adc_init();
    adc_gpio_init(PIN_NUM_LV_CS);
    adc_gpio_init(MCU_SELECT_MOTOR_CS);
    adc_set_temp_sensor_enabled(true);

    write_sels(EXTERNAL_SELECT_3V3_CS_AND_MOTOR_FR_CS);
}

static void run_100ms(void) {
    switch (current_sel) {
        case EXTERNAL_SELECT_3V3_CS_AND_MOTOR_FR_CS:
            adc_select_input(MCU_SELECT_LV_CS);
            values.lv_3v3_current = VOLTS_FROM_RAW(adc_read());
            adc_select_input(MCU_SELECT_MOTOR_CS);
            values.motor_fr_current = VOLTS_FROM_RAW(adc_read());

            write_sels(EXTERNAL_SELECT_MOTOR_FL_CS);
            break;

        case EXTERNAL_SELECT_MOTOR_FL_CS:
            adc_select_input(MCU_SELECT_MOTOR_CS);
            values.motor_fl_current = VOLTS_FROM_RAW(adc_read());

            write_sels(EXTERNAL_SELECT_5V_CS_AND_MOTOR_RL_CS);
            break;

        case EXTERNAL_SELECT_5V_CS_AND_MOTOR_RL_CS:
            adc_select_input(MCU_SELECT_LV_CS);
            values.lv_5v0_current = VOLTS_FROM_RAW(adc_read());
            adc_select_input(MCU_SELECT_MOTOR_CS);
            values.motor_rl_current = VOLTS_FROM_RAW(adc_read());

            write_sels(EXTERNAL_SELECT_MOTOR_RR_CS);
            break;

        case EXTERNAL_SELECT_MOTOR_RR_CS:
            adc_select_input(MCU_SELECT_MOTOR_CS);
            values.motor_rr_current = VOLTS_FROM_RAW(adc_read());

            write_sels(EXTERNAL_SELECT_3V3_CS_AND_MOTOR_FR_CS);
            break;

        default:
            fatal(UNREACHABLE);
    }

    adc_select_input(MCU_SELECT_TEMP);
    float conv_voltage = VOLTS_FROM_RAW(adc_read());
    values.mcu_temp    = 27 - (((float)conv_voltage - 0.706) / 0.001721);
}

module_t adc_module = {
    .name       = "adc",
    .init       = init,
    .run_1ms    = NULL,
    .run_10ms   = NULL,
    .run_100ms  = run_100ms,
    .run_1000ms = NULL,
};
