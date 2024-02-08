#include "lp_i2c.h"

#include "generated/is31fl3197.h"
#include "generated/lm75b.h"
#include "generated/tca9539.h"
#include "generated/values.h"
#include "globals.h"
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

static void init(void) {
    i2c_init(LP_I2C, 400 * 1000);
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
    set_is31fl3197_out4_curr(&reg8, 0x01);
    i2c_write_8(I2C_ADDRESS_LED_DRIVER, IS31FL3197_OUT4_CURR_ADDRESS, reg8);
    i2c_write_8(
        I2C_ADDRESS_LED_DRIVER, IS31FL3197_COLOR_UPDATE_ADDRESS, IS31FL3197_MAGIC_NUM_VALUE
    );
    set_is31fl3197_shdn_ctrl(
        &reg8, IS31FL3197_SHDN_MODE_NORMAL, IS31FL3197_SLEEP_MODE_DISABLED, true, true, true, true
    );
    i2c_write_8(I2C_ADDRESS_LED_DRIVER, IS31FL3197_SHDN_CTRL_ADDRESS, reg8);
}

static void run_10ms(void) {
    i2c_read_blocking(LP_I2C, I2C_ADDRESS_QI_TEMP, rx_buf, LM75B_TEMP_SIZE, false);
    get_lm75b_temp(*((uint16_t*)rx_buf), &values.qi_temp);

    set_is31fl3197_out4_pwm(&reg16, get_led_state() ? 1.0f : 0.0f);
    i2c_write_16(I2C_ADDRESS_LED_DRIVER, IS31FL3197_OUT4_PWM_ADDRESS, reg16);
    i2c_write_8(I2C_ADDRESS_LED_DRIVER, IS31FL3197_PWM_UPDATE_ADDRESS, IS31FL3197_MAGIC_NUM_VALUE);

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
