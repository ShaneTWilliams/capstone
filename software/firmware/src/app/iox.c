#include "iox.h"

static struct {
    bool output_state;
    bool input_state;
    tca9539_direction_t direction;
} iox_gpio_states[IOX_GPIO_COUNT] = {
    // MISC Port 0
    [IOX_GPIO_LED_SHDN] =
        {.output_state = true, .input_state = false, .direction = TCA9539_DIRECTION_OUTPUT},
    [IOX_GPIO_PWR_IOX_INT] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_PWR_IOX_RST] =
        {.output_state = true, .input_state = false, .direction = TCA9539_DIRECTION_OUTPUT},
    [IOX_GPIO_MTR_IOX_INT] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_MTR_IOX_RST] =
        {.output_state = true, .input_state = false, .direction = TCA9539_DIRECTION_OUTPUT},
    [IOX_GPIO_GPS_RESET] =
        {.output_state = true, .input_state = false, .direction = TCA9539_DIRECTION_OUTPUT},
    [IOX_GPIO_MUX_A0] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_OUTPUT},

    // MISC Port 1
    [IOX_GPIO_MUX_A1] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_OUTPUT},
    [IOX_GPIO_MUX_EN] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_OUTPUT},
    [IOX_GPIO_SX_RESET] =
        {.output_state = true, .input_state = false, .direction = TCA9539_DIRECTION_OUTPUT},
    [IOX_GPIO_SX_DIO1] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_SX_DIO3] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_MCU_BOOT] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_IMU_INT2] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_IMU_INT1] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},

    // CHG Port 0
    [IOX_GPIO_USBPD_FAULT] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_USBPD_INT] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_QI_TEMP_OS] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_QI_EN_2] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_OUTPUT},
    [IOX_GPIO_QI_EN_1] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_OUTPUT},
    [IOX_GPIO_QI_CHG] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_33_PG] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_50_EN] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},

    // CHG Port 1
    [IOX_GPIO_50_PG] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_TP_1101] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_TP_1102] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_TP_1104] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},

    // PWR Port 0
    [IOX_GPIO_CC_PROCHOT] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_CC_CHRG_OK] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_MTR_TMP_OS] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_BMS_ALRT] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_MTR_RL_nFAULT] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_MTR_RL_BRAKE] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},

    // PWR Port 1
    [IOX_GPIO_MTR_RL_DIR] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_MTR_RL_DRVOFF] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},

    // MTR Port 0
    [IOX_GPIO_MTR_FL_DIR] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_OUTPUT},
    [IOX_GPIO_MTR_FL_DRVOFF] =
        {.output_state = true, .input_state = false, .direction = TCA9539_DIRECTION_OUTPUT},
    [IOX_GPIO_MTR_FL_BRAKE] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_OUTPUT},
    [IOX_GPIO_MTR_FL_nFAULT] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_MTR_FR_DIR] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_MTR_FR_DRVOFF] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_MTR_FR_BRAKE] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_MTR_FR_nFAULT] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},

    // MTR Port 1
    [IOX_GPIO_MTR_RR_DIR] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_MTR_RR_DRVOFF] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_MTR_RR_BRAKE] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
    [IOX_GPIO_MTR_RR_nFAULT] =
        {.output_state = false, .input_state = false, .direction = TCA9539_DIRECTION_INPUT},
};

iox_gpio_t iox_config[IOX_COUNT][IOX_PORT_COUNT][IOX_PIN_COUNT] = {
    [IOX_MISC] =
        {
            [IOX_PORT_0] =
                {
                    [IOX_PIN_0] = IOX_GPIO_LED_SHDN,
                    [IOX_PIN_1] = IOX_GPIO_PWR_IOX_INT,
                    [IOX_PIN_2] = IOX_GPIO_PWR_IOX_RST,
                    [IOX_PIN_3] = IOX_GPIO_MTR_IOX_INT,
                    [IOX_PIN_4] = IOX_GPIO_MTR_IOX_RST,
                    [IOX_PIN_5] = IOX_GPIO_UNUSED,
                    [IOX_PIN_6] = IOX_GPIO_GPS_RESET,
                    [IOX_PIN_7] = IOX_GPIO_MUX_A0,
                },
            [IOX_PORT_1] =
                {
                    [IOX_PIN_0] = IOX_GPIO_MUX_A1,
                    [IOX_PIN_1] = IOX_GPIO_MUX_EN,
                    [IOX_PIN_2] = IOX_GPIO_SX_RESET,
                    [IOX_PIN_3] = IOX_GPIO_SX_DIO1,
                    [IOX_PIN_4] = IOX_GPIO_SX_DIO3,
                    [IOX_PIN_5] = IOX_GPIO_MCU_BOOT,
                    [IOX_PIN_6] = IOX_GPIO_IMU_INT2,
                    [IOX_PIN_7] = IOX_GPIO_IMU_INT1,
                },
        },
    [IOX_CHG] =
        {
            [IOX_PORT_0] =
                {
                    [IOX_PIN_0] = IOX_GPIO_USBPD_FAULT,
                    [IOX_PIN_1] = IOX_GPIO_USBPD_INT,
                    [IOX_PIN_2] = IOX_GPIO_QI_TEMP_OS,
                    [IOX_PIN_3] = IOX_GPIO_QI_EN_2,
                    [IOX_PIN_4] = IOX_GPIO_QI_EN_1,
                    [IOX_PIN_5] = IOX_GPIO_QI_CHG,
                    [IOX_PIN_6] = IOX_GPIO_33_PG,
                    [IOX_PIN_7] = IOX_GPIO_50_EN,
                },
            [IOX_PORT_1] =
                {
                    [IOX_PIN_0] = IOX_GPIO_50_PG,
                    [IOX_PIN_1] = IOX_GPIO_TP_1101,
                    [IOX_PIN_2] = IOX_GPIO_TP_1102,
                    [IOX_PIN_3] = IOX_GPIO_TP_1104,
                    [IOX_PIN_4] = IOX_GPIO_UNUSED,
                    [IOX_PIN_5] = IOX_GPIO_UNUSED,
                    [IOX_PIN_6] = IOX_GPIO_UNUSED,
                    [IOX_PIN_7] = IOX_GPIO_UNUSED,
                },
        },
    [IOX_PWR] =
        {
            [IOX_PORT_0] =
                {
                    [IOX_PIN_0] = IOX_GPIO_CC_PROCHOT,
                    [IOX_PIN_1] = IOX_GPIO_CC_CHRG_OK,
                    [IOX_PIN_2] = IOX_GPIO_MTR_TMP_OS,
                    [IOX_PIN_3] = IOX_GPIO_BMS_ALRT,
                    [IOX_PIN_4] = IOX_GPIO_UNUSED,
                    [IOX_PIN_5] = IOX_GPIO_UNUSED,
                    [IOX_PIN_6] = IOX_GPIO_MTR_RL_nFAULT,
                    [IOX_PIN_7] = IOX_GPIO_MTR_RL_BRAKE,
                },
            [IOX_PORT_1] =
                {
                    [IOX_PIN_0] = IOX_GPIO_MTR_RL_DIR,
                    [IOX_PIN_1] = IOX_GPIO_MTR_RL_DRVOFF,
                    [IOX_PIN_2] = IOX_GPIO_UNUSED,
                    [IOX_PIN_3] = IOX_GPIO_UNUSED,
                    [IOX_PIN_4] = IOX_GPIO_UNUSED,
                    [IOX_PIN_5] = IOX_GPIO_UNUSED,
                    [IOX_PIN_6] = IOX_GPIO_UNUSED,
                    [IOX_PIN_7] = IOX_GPIO_UNUSED,
                },
        },
    [IOX_MTR] =
        {
            [IOX_PORT_0] =
                {
                    [IOX_PIN_0] = IOX_GPIO_MTR_FL_DIR,
                    [IOX_PIN_1] = IOX_GPIO_MTR_FL_DRVOFF,
                    [IOX_PIN_2] = IOX_GPIO_MTR_FL_BRAKE,
                    [IOX_PIN_3] = IOX_GPIO_MTR_FL_nFAULT,
                    [IOX_PIN_4] = IOX_GPIO_MTR_FR_DIR,
                    [IOX_PIN_5] = IOX_GPIO_MTR_FR_DRVOFF,
                    [IOX_PIN_6] = IOX_GPIO_MTR_FR_BRAKE,
                    [IOX_PIN_7] = IOX_GPIO_MTR_FR_nFAULT,
                },
            [IOX_PORT_1] =
                {
                    [IOX_PIN_0] = IOX_GPIO_MTR_RR_DIR,
                    [IOX_PIN_1] = IOX_GPIO_MTR_RR_DRVOFF,
                    [IOX_PIN_2] = IOX_GPIO_MTR_RR_BRAKE,
                    [IOX_PIN_3] = IOX_GPIO_MTR_RR_nFAULT,
                    [IOX_PIN_4] = IOX_GPIO_UNUSED,
                    [IOX_PIN_5] = IOX_GPIO_UNUSED,
                    [IOX_PIN_6] = IOX_GPIO_UNUSED,
                    [IOX_PIN_7] = IOX_GPIO_UNUSED,
                },
        }};

void set_iox_gpio_output_state(iox_gpio_t gpio, bool state) {
    iox_gpio_states[gpio].output_state = state;
}

void set_iox_gpio_direction(iox_gpio_t gpio, tca9539_direction_t direction) {
    iox_gpio_states[gpio].direction = direction;
}

bool get_iox_gpio_input_state(iox_gpio_t gpio) { return iox_gpio_states[gpio].input_state; }

void set_iox_gpio_input_state(iox_t iox, iox_port_t port, iox_pin_t pin, bool state) {
    iox_gpio_states[iox_config[iox][port][pin]].input_state = state;
}

bool get_iox_gpio_output_state(iox_t iox, iox_port_t port, iox_pin_t pin) {
    return iox_gpio_states[iox_config[iox][port][pin]].output_state;
}

tca9539_direction_t get_iox_gpio_direction(iox_t iox, iox_port_t port, iox_pin_t pin) {
    return iox_gpio_states[iox_config[iox][port][pin]].direction;
}
