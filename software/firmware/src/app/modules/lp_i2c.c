#include "lp_i2c.h"

#include "generated/is31fl3197.h"
#include "generated/lm75b.h"
#include "generated/tca9539.h"
#include "generated/values.h"
#include "hardware/i2c.h"
#include "hw_config.h"
#include "indication.h"
#include "iox.h"
#include "pico/stdlib.h"

static uint8_t rx_buf[16];
static uint8_t tx_buf[16];

static uint8_t reg8;
static uint16_t reg16;

static void i2c_write_8(uint8_t addr, uint8_t reg, uint8_t val) {
    tx_buf[0] = reg;
    tx_buf[1] = val;
    i2c_write_blocking(LP_I2C, addr, tx_buf, 1U + sizeof(uint8_t), false);
}

static void i2c_write_16(uint8_t addr, uint8_t reg, uint16_t val) {
    tx_buf[0] = reg;
    tx_buf[1] = val >> 8;
    tx_buf[2] = val & 0xFF;
    i2c_write_blocking(LP_I2C, addr, tx_buf, 1U + sizeof(uint16_t), false);
}

static uint16_t i2c_read_16(uint8_t addr, uint8_t reg) {
    tx_buf[0] = reg;
    i2c_write_blocking(LP_I2C, addr, tx_buf, sizeof(uint8_t), true);
    i2c_read_blocking(LP_I2C, addr, rx_buf, sizeof(uint16_t), false);
    return rx_buf[0] | (rx_buf[1] << 8);
}

static void temp_i2c(void) {
    static uint8_t index = 0;

    switch (index++) {
        case 0:
            uint16_t temp = i2c_read_16(I2C_ADDRESS_QI_TEMP, LM75B_TEMP_ADDRESS);
            get_lm75b_temp(temp, &values.qi_temp);
            break;
        case 1:
            bool shutdown = values.drone_state == DRONE_STATE_SLEEP;
            set_lm75b_conf(&reg8, shutdown, LM75B_OS_OP_MODE_COMPARATOR, LM75B_OS_POL_ACTIVE_LOW, LM75B_CONS_FAULT_NUM_1);
            i2c_write_8(I2C_ADDRESS_QI_TEMP, 0x01, reg8);

            index = 0;
            break;
    }
}

static void led_i2c(void) {
    static uint8_t index = 0;

    if (index++ == 0) {
        set_is31fl3197_out1_pwm(&reg16, (float)get_led_brightness(LED_COMPONENT_RED) / 255.0f);
        i2c_write_16(I2C_ADDRESS_LED_DRIVER, IS31FL3197_OUT1_PWM_ADDRESS, reg16);
        set_is31fl3197_out2_pwm(&reg16, (float)get_led_brightness(LED_COMPONENT_GREEN) / 255.0f);
        i2c_write_16(I2C_ADDRESS_LED_DRIVER, IS31FL3197_OUT2_PWM_ADDRESS, reg16);
        set_is31fl3197_out3_pwm(&reg16, (float)get_led_brightness(LED_COMPONENT_BLUE) / 255.0f);
        i2c_write_16(I2C_ADDRESS_LED_DRIVER, IS31FL3197_OUT3_PWM_ADDRESS, reg16);
        set_is31fl3197_out4_pwm(&reg16, (float)get_led_brightness(LED_COMPONENT_WHITE) / 255.0f);
        i2c_write_16(I2C_ADDRESS_LED_DRIVER, IS31FL3197_OUT4_PWM_ADDRESS, reg16);
        i2c_write_8(I2C_ADDRESS_LED_DRIVER, IS31FL3197_PWM_UPDATE_ADDRESS, IS31FL3197_MAGIC_NUM_VALUE);
    }
    if (index == 2) {
        index = 0;
    }
}

static void init(void) {
    i2c_init(LP_I2C, LP_I2C_FREQ);
    gpio_set_function(PIN_NUM_LP_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_NUM_LP_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_NUM_LP_SDA);
    gpio_pull_up(PIN_NUM_LP_SCL);

    gpio_init(PIN_NUM_MISC_IOX_RST);
    gpio_set_dir(PIN_NUM_MISC_IOX_RST, GPIO_OUT);
    gpio_put(PIN_NUM_MISC_IOX_RST, 1);

    gpio_init(PIN_NUM_CHG_IOX_RST);
    gpio_set_dir(PIN_NUM_CHG_IOX_RST, GPIO_OUT);
    gpio_put(PIN_NUM_CHG_IOX_RST, 1);

    // LED driver initialization.
    static uint8_t current_setting = 0x30;
    set_is31fl3197_out1_curr(&reg8, current_setting);
    i2c_write_8(I2C_ADDRESS_LED_DRIVER, IS31FL3197_OUT1_CURR_ADDRESS, reg8);
    set_is31fl3197_out2_curr(&reg8, current_setting);
    i2c_write_8(I2C_ADDRESS_LED_DRIVER, IS31FL3197_OUT2_CURR_ADDRESS, reg8);
    set_is31fl3197_out3_curr(&reg8, current_setting);
    i2c_write_8(I2C_ADDRESS_LED_DRIVER, IS31FL3197_OUT3_CURR_ADDRESS, reg8);
    set_is31fl3197_out4_curr(&reg8, current_setting);
    i2c_write_8(I2C_ADDRESS_LED_DRIVER, IS31FL3197_OUT4_CURR_ADDRESS, reg8);

    i2c_write_8(
        I2C_ADDRESS_LED_DRIVER, IS31FL3197_COLOR_UPDATE_ADDRESS, IS31FL3197_MAGIC_NUM_VALUE
    );
    set_is31fl3197_shdn_ctrl(
        &reg8, IS31FL3197_SHDN_MODE_NORMAL, IS31FL3197_SLEEP_MODE_DISABLED, true, true, true, true
    );
    i2c_write_8(I2C_ADDRESS_LED_DRIVER, IS31FL3197_SHDN_CTRL_ADDRESS, reg8);
}

static void run_10ms(uint32_t cycle) {
    temp_i2c();
    led_i2c();

    set_tca9539_output_port_0(
        &reg8, get_iox_gpio_output_state(IOX_MISC, IOX_PORT_0, IOX_PIN_0),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_0, IOX_PIN_1),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_0, IOX_PIN_2),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_0, IOX_PIN_3),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_0, IOX_PIN_4),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_0, IOX_PIN_5),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_0, IOX_PIN_6),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_0, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_MISC_IOX, TCA9539_OUTPUT_PORT_0_ADDRESS, reg8);
    set_tca9539_direction_port_0(
        &reg8, get_iox_gpio_direction(IOX_MISC, IOX_PORT_0, IOX_PIN_0),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_0, IOX_PIN_1),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_0, IOX_PIN_2),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_0, IOX_PIN_3),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_0, IOX_PIN_4),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_0, IOX_PIN_5),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_0, IOX_PIN_6),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_0, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_MISC_IOX, TCA9539_DIRECTION_PORT_0_ADDRESS, reg8);

    set_tca9539_output_port_1(
        &reg8, get_iox_gpio_output_state(IOX_MISC, IOX_PORT_1, IOX_PIN_0),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_1, IOX_PIN_1),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_1, IOX_PIN_2),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_1, IOX_PIN_3),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_1, IOX_PIN_4),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_1, IOX_PIN_5),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_1, IOX_PIN_6),
        get_iox_gpio_output_state(IOX_MISC, IOX_PORT_1, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_MISC_IOX, TCA9539_OUTPUT_PORT_1_ADDRESS, reg8);
    set_tca9539_direction_port_1(
        &reg8, get_iox_gpio_direction(IOX_MISC, IOX_PORT_1, IOX_PIN_0),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_1, IOX_PIN_1),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_1, IOX_PIN_2),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_1, IOX_PIN_3),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_1, IOX_PIN_4),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_1, IOX_PIN_5),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_1, IOX_PIN_6),
        get_iox_gpio_direction(IOX_MISC, IOX_PORT_1, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_MISC_IOX, TCA9539_DIRECTION_PORT_1_ADDRESS, reg8);

    set_tca9539_output_port_0(
        &reg8, get_iox_gpio_output_state(IOX_CHG, IOX_PORT_0, IOX_PIN_0),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_0, IOX_PIN_1),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_0, IOX_PIN_2),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_0, IOX_PIN_3),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_0, IOX_PIN_4),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_0, IOX_PIN_5),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_0, IOX_PIN_6),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_0, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_CHG_IOX, TCA9539_OUTPUT_PORT_0_ADDRESS, reg8);
    set_tca9539_direction_port_0(
        &reg8, get_iox_gpio_direction(IOX_CHG, IOX_PORT_0, IOX_PIN_0),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_0, IOX_PIN_1),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_0, IOX_PIN_2),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_0, IOX_PIN_3),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_0, IOX_PIN_4),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_0, IOX_PIN_5),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_0, IOX_PIN_6),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_0, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_CHG_IOX, TCA9539_DIRECTION_PORT_0_ADDRESS, reg8);

    set_tca9539_output_port_1(
        &reg8, get_iox_gpio_output_state(IOX_CHG, IOX_PORT_1, IOX_PIN_0),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_1, IOX_PIN_1),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_1, IOX_PIN_2),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_1, IOX_PIN_3),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_1, IOX_PIN_4),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_1, IOX_PIN_5),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_1, IOX_PIN_6),
        get_iox_gpio_output_state(IOX_CHG, IOX_PORT_1, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_CHG_IOX, TCA9539_OUTPUT_PORT_1_ADDRESS, reg8);
    set_tca9539_direction_port_1(
        &reg8, get_iox_gpio_direction(IOX_CHG, IOX_PORT_1, IOX_PIN_0),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_1, IOX_PIN_1),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_1, IOX_PIN_2),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_1, IOX_PIN_3),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_1, IOX_PIN_4),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_1, IOX_PIN_5),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_1, IOX_PIN_6),
        get_iox_gpio_direction(IOX_CHG, IOX_PORT_1, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_CHG_IOX, TCA9539_DIRECTION_PORT_1_ADDRESS, reg8);
}

module_t lp_i2c_module = {
    .name       = "lp_i2c",
    .init       = init,
    .run_1ms    = NULL,
    .run_10ms   = run_10ms,
    .run_100ms  = NULL,
    .run_1000ms = NULL,
};
