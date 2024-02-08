#include "hp_i2c.h"

#include "generated/lm75b.h"
#include "generated/tca9539.h"
#include "generated/max17320.h"
#include "generated/mct8329a.h"
#include "generated/bq25703a.h"

#include "generated/values.h"
#include "globals.h"
#include "hardware/i2c.h"
#include "hw_config.h"
#include "iox.h"
#include "pico/stdlib.h"

static uint8_t rx_buf[16];
static uint8_t tx_buf[16];

static uint8_t reg8;
static uint16_t reg16;
static uint32_t reg32;

static void i2c_write_8(uint8_t addr, uint8_t reg, uint8_t val) {
    tx_buf[0] = reg;
    tx_buf[1] = val;
    i2c_write_blocking(HP_I2C, addr, tx_buf, 1U + sizeof(uint8_t), false);
}

static void i2c_write_16(uint8_t addr, uint8_t reg, uint16_t val) {
    tx_buf[0] = reg;
    tx_buf[1] = val >> 8;
    tx_buf[2] = val & 0xFF;
    i2c_write_blocking(HP_I2C, addr, tx_buf, 1U + sizeof(uint16_t), false);
}

static uint16_t i2c_read_16(uint8_t addr, uint8_t reg) {
    tx_buf[0] = reg;
    i2c_write_blocking(HP_I2C, addr, tx_buf, 1U, false);
    i2c_read_blocking(HP_I2C, addr, rx_buf, 2U, false);
    return *(uint16_t*)rx_buf;
}

static uint32_t mct8329a_read_32(uint8_t addr, uint16_t reg) {
    tx_buf[0] = 0x90;
    tx_buf[1] = reg >> 8;
    tx_buf[2] = reg & 0xFF;
    i2c_write_blocking(HP_I2C, addr, tx_buf, 3U, false);
    i2c_read_blocking(HP_I2C, addr, rx_buf, 4U, false);
    return *(uint32_t*)rx_buf;
}

static void mct8329a_write_32(uint8_t addr, uint8_t reg, uint32_t val) {
    tx_buf[0] = 0x10;
    tx_buf[1] = reg >> 8;
    tx_buf[2] = reg & 0xFF;
    uint32_t val_le = htole32(val);
    memcpy(&tx_buf[3], (uint8_t*)&val_le, sizeof(uint32_t));
    i2c_write_blocking(HP_I2C, addr, tx_buf, 7U, false);
}

static uint32_t mct8329a_parity_32(uint32_t val) {
    bool parity = false;
    for (int i = 0; i < 32; i++) {
        parity ^= (val >> i) & 0x1;
    }
    return val | (parity << 31);
}

bool devs[1 << 7] = {0};

static void init(void) {
    i2c_init(HP_I2C, 400 * 1000);
    gpio_set_function(PIN_NUM_HP_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_NUM_HP_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_NUM_HP_SDA);
    gpio_pull_up(PIN_NUM_HP_SCL);

    // mct8329a_write_32(I2C_ADDRESS_MTR_RR, MCT8329A_DEVICE_CONFIG_ADDRESS, 0x00000002);
    // for (int addr = 0x60; addr < 0x64; addr++) {
    //     mct8329a_write_32(addr, MCT8329A_PIN_CONFIG1_ADDRESS, mct8329a_parity_32(0x00000002));
    //     mct8329a_write_32(addr, MCT8329A_GD_CONFIG1_ADDRESS, mct8329a_parity_32(0x00000003));
    //     mct8329a_write_32(addr, MCT8329A_DEVICE_CONFIG_ADDRESS, mct8329a_parity_32(0x0000200F));
    //     mct8329a_write_32(addr, MCT8329A_ALGO_CTRL1_ADDRESS, 0x8A500000);
    // }
    sleep_ms(100U);
    mct8329a_write_32(I2C_ADDRESS_MTR_RL, MCT8329A_ISD_CONFIG_ADDRESS, mct8329a_parity_32(0x00000000));
    mct8329a_write_32(I2C_ADDRESS_MTR_RL, MCT8329A_MOTOR_STARTUP1_ADDRESS, mct8329a_parity_32(0x2b9265b7));
    mct8329a_write_32(I2C_ADDRESS_MTR_RL, MCT8329A_MOTOR_STARTUP2_ADDRESS, mct8329a_parity_32(0xb3431a03));
    mct8329a_write_32(I2C_ADDRESS_MTR_RL, MCT8329A_CLOSED_LOOP1_ADDRESS, mct8329a_parity_32(0x20a5ee00));
    mct8329a_write_32(I2C_ADDRESS_MTR_RL, MCT8329A_CLOSED_LOOP2_ADDRESS, mct8329a_parity_32(0x02a1a428));
    mct8329a_write_32(I2C_ADDRESS_MTR_RL, MCT8329A_FAULT_CONFIG1_ADDRESS, mct8329a_parity_32(0x71783604));
    mct8329a_write_32(I2C_ADDRESS_MTR_RL, MCT8329A_FAULT_CONFIG2_ADDRESS, mct8329a_parity_32(0x00e20000));
    mct8329a_write_32(I2C_ADDRESS_MTR_RL, MCT8329A_GD_CONFIG1_ADDRESS, mct8329a_parity_32(0x0000003b));
    mct8329a_write_32(I2C_ADDRESS_MTR_RL, MCT8329A_DEVICE_CONFIG_ADDRESS, mct8329a_parity_32(0x0000000f));
    mct8329a_write_32(I2C_ADDRESS_MTR_RL, MCT8329A_CONST_PWR_ADDRESS, mct8329a_parity_32(0x0f400640));

    mct8329a_write_32(I2C_ADDRESS_MTR_RL, MCT8329A_ALGO_CTRL1_ADDRESS, 0x8A500000);
    sleep_ms(500U);

    sleep_ms(10);
    i2c_write_16(I2C_ADDRESS_CHG_CONT, BQ25703A_ADC_OPTION_ADDRESS, 0xffa0);
    i2c_write_16(I2C_ADDRESS_CHG_CONT, BQ25703A_CHARGE_OPTION_0_ADDRESS, 0x0e02);
    i2c_write_16(I2C_ADDRESS_CHG_CONT, BQ25703A_CHARGE_OPTION_1_ADDRESS, 0x1082);
    i2c_write_16(I2C_ADDRESS_CHG_CONT, BQ25703A_CHARGE_OPTION_2_ADDRESS, 0xF702);
    i2c_write_16(I2C_ADDRESS_CHG_CONT, BQ25703A_MIN_SYS_VOLTAGE_ADDRESS, 0x0004);


    set_iox_gpio_output_state(IOX_GPIO_MTR_FL_DRVOFF, true);
}

static void run_10ms(void) {
    // Read LM75B temperature.
    reg16 = i2c_read_16(I2C_ADDRESS_HP_TEMP, LM75B_TEMP_ADDRESS);
    get_lm75b_temp(reg16, &values.hp_temp);

    // BMS
    // reg16 = i2c_read_16(I2C_ADDRESS_BMS_MODELGAUGE, MAX17320_AVG_DIE_TEMP_ADDRESS);
    // get_max17320_avg_die_temp(reg16, &values.bms_temp);
    // reg16 = i2c_read_16(I2C_ADDRESS_BMS_MODELGAUGE, MAX17320_AVG_CELL1_ADDRESS);
    // get_max17320_avg_cell1(reg16, &values.cell1_voltage);
    // reg16 = i2c_read_16(I2C_ADDRESS_BMS_MODELGAUGE, MAX17320_AVG_CELL2_ADDRESS);
    // get_max17320_avg_cell2(reg16, &values.cell2_voltage);
    // reg16 = i2c_read_16(I2C_ADDRESS_BMS_MODELGAUGE, MAX17320_AVG_CELL3_ADDRESS);
    // get_max17320_avg_cell3(reg16, &values.cell3_voltage);
    // reg16 = i2c_read_16(I2C_ADDRESS_BMS_MODELGAUGE, MAX17320_AVG_CELL4_ADDRESS);
    // get_max17320_avg_cell4(reg16, &values.cell4_voltage);
    // reg16 = i2c_read_16(I2C_ADDRESS_BMS_MODELGAUGE, MAX17320_AVG_CURRENT_ADDRESS);

    // MTR
    reg32 = mct8329a_read_32(I2C_ADDRESS_MTR_RL, 0xe0);
    reg32 = mct8329a_read_32(I2C_ADDRESS_MTR_RL, 0xe2);
    reg32 = mct8329a_read_32(I2C_ADDRESS_MTR_RL, 0xea);

    reg32 = mct8329a_read_32(I2C_ADDRESS_MTR_RL, 0xe4);
    reg32 = mct8329a_read_32(I2C_ADDRESS_MTR_RL, 0xa6);

    // BQ25703
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_ADC_IBAT_ADDRESS);
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_ADC_IIN_CMPIN_ADDRESS);
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_ADC_VBUS_PSYS_ADDRESS);
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_ADC_VSYS_VBAT_ADDRESS);
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_CHARGER_STATUS_ADDRESS);
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_IIN_DPM_ADDRESS);
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_CHARGE_OPTION_0_ADDRESS);    // CHARGE_OPTION_0 (0x00)    = 0x020e
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_CHARGE_CURRENT_ADDRESS);     // CHARGE_CURRENT (0x02)     = 0x00c0
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_MAX_CHARGE_VOLTAGE_ADDRESS); // MAX_CHARGE_VOLTAGE (0x04) = 0x41a0
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_OTG_VOLTAGE_ADDRESS);        // OTG_VOLTAGE (0x06)        = 0x0000
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_OTG_CURRENT_ADDRESS);        // OTG_CURRENT (0x08)        = 0x0000
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_INPUT_VOLTAGE_ADDRESS);      // INPUT_VOLTAGE (0x0a)      = 0x01c0
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_MIN_SYS_VOLTAGE_ADDRESS);    // MIN_SYS_VOLTAGE (0x0c)    = 0x0400
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_IIN_HOST_ADDRESS);           // IIN_HOST (0x0e)           = 0x0e00
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_CHARGER_STATUS_ADDRESS);     // CHARGER_STATUS (0x20)     = 0xac00
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_PROCHOT_STATUS_ADDRESS);     // PROCHOT_STATUS (0x22)     = 0x0000
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_IIN_DPM_ADDRESS);            // IIN_DPM (0x24)            = 0x0e00
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_ADC_VBUS_PSYS_ADDRESS);      // ADC_VBUS_PSYS (0x26)      = 0x1c00
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_ADC_IBAT_ADDRESS);           // ADC_IBAT (0x28)           = 0x0000
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_ADC_IIN_CMPIN_ADDRESS);      // ADC_IIN_CMPIN (0x2a)      = 0x0d00
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_ADC_VSYS_VBAT_ADDRESS);      // ADC_VSYS_VBAT (0x2c)      = 0xd3d2
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_MANUFACTURE_ID_ADDRESS);     // MANUFACTURE_ID (0x2e)     = 0x40
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_DEVICE_ID_ADDRESS);          // DEVICE_ID (0x2f)          = 0x78
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_CHARGE_OPTION_1_ADDRESS);    // CHARGE_OPTION_1 (0x30)    = 0x8210
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_CHARGE_OPTION_2_ADDRESS);    // CHARGE_OPTION_2 (0x32)    = 0x02f7
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_CHARGE_OPTION_3_ADDRESS);    // CHARGE_OPTION_3 (0x34)    = 0x0000
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_PROCHOT_OPTION_1_ADDRESS);   // PROCHOT_OPTION_1 (0x38)   = 0x8120
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_ADC_OPTION_ADDRESS);         // ADC_OPTION (0x3a)         = 0xa0ff


    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_CHARGE_OPTION_0_ADDRESS);
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_IIN_HOST_ADDRESS);
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_CHARGE_CURRENT_ADDRESS);
    // reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONT, BQ25703A_MIN_SYS_VOLTAGE_ADDRESS);

    // i2c_write_16(I2C_ADDRESS_CHG_CONT, BQ25703A_IIN_HOST_ADDRESS, 0x000e);
    // i2c_write_16(I2C_ADDRESS_CHG_CONT, BQ25703A_CHARGE_CURRENT_ADDRESS, 0xc000);

    if (cycles_10ms == 100) {
        set_iox_gpio_output_state(IOX_GPIO_MTR_FL_DRVOFF, false);
    }

    set_tca9539_output_port_0(
        &reg8, get_iox_gpio_output_state(IOX_PWR, IOX_PORT_0, IOX_PIN_0),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_0, IOX_PIN_1),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_0, IOX_PIN_2),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_0, IOX_PIN_3),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_0, IOX_PIN_4),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_0, IOX_PIN_5),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_0, IOX_PIN_6),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_0, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_PWR_IOX, TCA9539_OUTPUT_PORT_0_ADDRESS, reg8);
    set_tca9539_direction_port_0(
        &reg8, get_iox_gpio_direction(IOX_PWR, IOX_PORT_0, IOX_PIN_0),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_0, IOX_PIN_1),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_0, IOX_PIN_2),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_0, IOX_PIN_3),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_0, IOX_PIN_4),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_0, IOX_PIN_5),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_0, IOX_PIN_6),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_0, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_PWR_IOX, TCA9539_DIRECTION_PORT_0_ADDRESS, reg8);

    set_tca9539_output_port_1(
        &reg8, get_iox_gpio_output_state(IOX_PWR, IOX_PORT_1, IOX_PIN_0),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_1, IOX_PIN_1),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_1, IOX_PIN_2),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_1, IOX_PIN_3),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_1, IOX_PIN_4),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_1, IOX_PIN_5),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_1, IOX_PIN_6),
        get_iox_gpio_output_state(IOX_PWR, IOX_PORT_1, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_PWR_IOX, TCA9539_OUTPUT_PORT_1_ADDRESS, reg8);
    set_tca9539_direction_port_1(
        &reg8, get_iox_gpio_direction(IOX_PWR, IOX_PORT_1, IOX_PIN_0),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_1, IOX_PIN_1),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_1, IOX_PIN_2),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_1, IOX_PIN_3),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_1, IOX_PIN_4),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_1, IOX_PIN_5),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_1, IOX_PIN_6),
        get_iox_gpio_direction(IOX_PWR, IOX_PORT_1, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_PWR_IOX, TCA9539_DIRECTION_PORT_1_ADDRESS, reg8);

    set_tca9539_output_port_0(
        &reg8, get_iox_gpio_output_state(IOX_MTR, IOX_PORT_0, IOX_PIN_0),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_0, IOX_PIN_1),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_0, IOX_PIN_2),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_0, IOX_PIN_3),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_0, IOX_PIN_4),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_0, IOX_PIN_5),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_0, IOX_PIN_6),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_0, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_MTR_IOX, TCA9539_OUTPUT_PORT_0_ADDRESS, reg8);
    set_tca9539_direction_port_0(
        &reg8, get_iox_gpio_direction(IOX_MTR, IOX_PORT_0, IOX_PIN_0),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_0, IOX_PIN_1),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_0, IOX_PIN_2),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_0, IOX_PIN_3),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_0, IOX_PIN_4),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_0, IOX_PIN_5),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_0, IOX_PIN_6),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_0, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_MTR_IOX, TCA9539_DIRECTION_PORT_0_ADDRESS, reg8);

    set_tca9539_output_port_1(
        &reg8, get_iox_gpio_output_state(IOX_MTR, IOX_PORT_1, IOX_PIN_0),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_1, IOX_PIN_1),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_1, IOX_PIN_2),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_1, IOX_PIN_3),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_1, IOX_PIN_4),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_1, IOX_PIN_5),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_1, IOX_PIN_6),
        get_iox_gpio_output_state(IOX_MTR, IOX_PORT_1, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_MTR_IOX, TCA9539_OUTPUT_PORT_1_ADDRESS, reg8);
    set_tca9539_direction_port_1(
        &reg8, get_iox_gpio_direction(IOX_MTR, IOX_PORT_1, IOX_PIN_0),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_1, IOX_PIN_1),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_1, IOX_PIN_2),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_1, IOX_PIN_3),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_1, IOX_PIN_4),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_1, IOX_PIN_5),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_1, IOX_PIN_6),
        get_iox_gpio_direction(IOX_MTR, IOX_PORT_1, IOX_PIN_7)
    );
    i2c_write_8(I2C_ADDRESS_MTR_IOX, TCA9539_DIRECTION_PORT_1_ADDRESS, reg8);

    // tx_buf[0] = 0x00;
    // i2c_write_blocking(HP_I2C, I2C_ADDRESS_MTR_IOX, tx_buf, 1U, false);
    // i2c_read_blocking(HP_I2C, I2C_ADDRESS_MTR_IOX, rx_buf, 1U, false);
}

module_t hp_i2c_module = {
    .name       = "hp_i2c",
    .init       = init,
    .run_1ms    = NULL,
    .run_10ms   = run_10ms,
    .run_100ms  = NULL,
    .run_1000ms = NULL,
};
