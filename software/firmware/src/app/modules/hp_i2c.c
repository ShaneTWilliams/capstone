#include "hp_i2c.h"

#include "fatal.h"
#include "generated/bq25703a.h"
#include "generated/lm75b.h"
#include "generated/max17320.h"
#include "generated/mct8329a.h"
#include "generated/tca9539.h"
#include "generated/values.h"
#include "hardware/i2c.h"
#include "hw_config.h"
#include "iox.h"
#include "pico/stdlib.h"
#include "util.h"

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
    tx_buf[1] = val & 0xFF;
    tx_buf[2] = val >> 8;
    i2c_write_blocking(HP_I2C, addr, tx_buf, 1U + sizeof(uint16_t), false);
}

static uint16_t i2c_read_16(uint8_t addr, uint8_t reg) {
    tx_buf[0] = reg;
    i2c_write_blocking(HP_I2C, addr, tx_buf, sizeof(uint8_t), true);
    i2c_read_blocking(HP_I2C, addr, rx_buf, sizeof(uint16_t), false);
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
    tx_buf[0]       = 0x10;
    tx_buf[1]       = reg >> 8;
    tx_buf[2]       = reg & 0xFF;
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

static uint8_t bms_registers[] = {
    MAX17320_DIE_TEMP_ADDRESS, MAX17320_CELL1_ADDRESS, MAX17320_CELL2_ADDRESS,
    MAX17320_CELL3_ADDRESS,    MAX17320_CELL4_ADDRESS, MAX17320_CURRENT_ADDRESS,
    MAX17320_AVG_SOC_ADDRESS,  MAX17320_PCKP_ADDRESS,
};

static void bms_i2c(void) {
    static uint8_t index = 0;
    if (++index >= ARRAY_SIZE(bms_registers)) {
        index = 0;
    }

    reg16 = i2c_read_16(I2C_ADDRESS_BMS_MODELGAUGE, bms_registers[index]);
    switch (bms_registers[index]) {
        case MAX17320_DIE_TEMP_ADDRESS:
            get_max17320_die_temp(reg16, &values.bms_temp);
            break;
        case MAX17320_CELL1_ADDRESS:
            get_max17320_cell1(reg16, &values.cell1_voltage);
            break;
        case MAX17320_CELL2_ADDRESS:
            get_max17320_cell2(reg16, &values.cell2_voltage);
            break;
        case MAX17320_CELL3_ADDRESS:
            get_max17320_cell3(reg16, &values.cell3_voltage);
            break;
        case MAX17320_CELL4_ADDRESS:
            get_max17320_cell4(reg16, &values.cell4_voltage);
            break;
        case MAX17320_CURRENT_ADDRESS:
            float current_ma;
            get_max17320_current(reg16, &current_ma);
            values.bms_current = MA_TO_A(current_ma);
            break;
        case MAX17320_AVG_SOC_ADDRESS:
            get_max17320_avg_soc(reg16, &values.soc);
            break;
        case MAX17320_PCKP_ADDRESS:
            get_max17320_pckp(reg16, &values.battery_voltage);
            break;
        default:
            fatal(FATAL_UNREACHABLE);
            break;
    }
}

static uint8_t cc_registers[] = {
    BQ25703A_ADC_VBUS_PSYS_ADDRESS,
    BQ25703A_ADC_IBAT_ADDRESS,
    BQ25703A_ADC_IIN_CMPIN_ADDRESS,
    BQ25703A_ADC_VSYS_VBAT_ADDRESS,
};

static void cc_i2c(void) {
    static uint8_t index = 0;
    if (++index >= ARRAY_SIZE(cc_registers)) {
        index = 0;
    }

    reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONTROLLER, cc_registers[index]);
    float voltage_mv, current_ma;
    switch (cc_registers[index]) {
        case BQ25703A_ADC_VBUS_PSYS_ADDRESS:
            get_bq25703a_adc_vbus_psys(reg16, NULL, &voltage_mv);
            values.charge_input_voltage = voltage_mv <= 3200.1f ? 0.0f : MV_TO_V(voltage_mv);
            break;
        case BQ25703A_ADC_IBAT_ADDRESS:
            get_bq25703a_adc_ibat(reg16, NULL, &values.charge_output_current);
            break;
        case BQ25703A_ADC_IIN_CMPIN_ADDRESS:
            get_bq25703a_adc_iin_cmpin(reg16, NULL, &current_ma);
            values.charge_input_current = MA_TO_A(current_ma);
            break;
        case BQ25703A_ADC_VSYS_VBAT_ADDRESS:
            get_bq25703a_adc_vsys_vbat(reg16, NULL, &voltage_mv);
            values.charge_output_voltage = MV_TO_V(voltage_mv);
            break;
        default:
            fatal(FATAL_UNREACHABLE);
            break;
    }

    // These watchdog-petting writes don't need to be frequent (> 0.2 Hz as of now).
    if (index == 0) {
        set_bq25703a_iin_host(&reg16, 700.0f);
        i2c_write_16(I2C_ADDRESS_CHG_CONTROLLER, BQ25703A_IIN_HOST_ADDRESS, reg16);
    } else if (index == 1) {
        set_bq25703a_charge_current(&reg16, 256.0f);
        i2c_write_16(I2C_ADDRESS_CHG_CONTROLLER, BQ25703A_CHARGE_CURRENT_ADDRESS, reg16);
    } else if (index == 2) {
        bool low_power_mode = values.drone_state == DRONE_STATE_SLEEP;
        set_bq25703a_charge_option_0(
            &reg16, false, true, true, true, BQ25703A_IBAT_GAIN_8X, false, BQ25703A_PWM_FREQ_800_KHZ,
            false, false, false, BQ25703A_WDTMR_ADJ_ENABLED_5_S, low_power_mode
        );
        i2c_write_16(I2C_ADDRESS_CHG_CONTROLLER, BQ25703A_CHARGE_OPTION_0_ADDRESS, reg16);
    }

    bool acov_fault, batoc_fault, acoc_fault, otg_ovp_fault, otg_ucp_fault;
    reg16 = i2c_read_16(I2C_ADDRESS_CHG_CONTROLLER, BQ25703A_CHARGER_STATUS_ADDRESS);
    get_bq25703a_charger_status(
        reg16, &otg_ucp_fault, &otg_ovp_fault, NULL, NULL, &acoc_fault, &batoc_fault, &acov_fault,
        NULL, NULL, NULL, NULL, NULL, NULL, &values.charge_input_present
    );
    values.charge_fault = acov_fault || batoc_fault || acoc_fault || otg_ovp_fault || otg_ucp_fault;
}

static void temp_i2c(void) {
    static uint8_t index = 0;

    switch (index++) {
        case 0:
            uint16_t temp = i2c_read_16(I2C_ADDRESS_HP_TEMP, LM75B_TEMP_ADDRESS);
            get_lm75b_temp(temp, &values.hp_temp);
            break;
        case 1:
            bool shutdown = values.drone_state == DRONE_STATE_SLEEP;
            set_lm75b_conf(&reg8, shutdown, LM75B_OS_OP_MODE_COMPARATOR, LM75B_OS_POL_ACTIVE_LOW, LM75B_CONS_FAULT_NUM_1);
            i2c_write_8(I2C_ADDRESS_HP_TEMP, LM75B_CONF_ADDRESS, reg8);

            index = 0;
            break;
    }
}

static void init(void) {
    i2c_init(HP_I2C, HP_I2C_FREQ);
    gpio_set_function(PIN_NUM_HP_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_NUM_HP_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_NUM_HP_SDA);
    gpio_pull_up(PIN_NUM_HP_SCL);

    sleep_ms(10U);

    for (int addr = 0x60; addr < 0x64; addr++) {
        mct8329a_write_32(addr, MCT8329A_ISD_CONFIG_ADDRESS, mct8329a_parity_32(0x00000000));
        mct8329a_write_32(addr, MCT8329A_MOTOR_STARTUP1_ADDRESS, mct8329a_parity_32(0x3b3265b7));
        mct8329a_write_32(addr, MCT8329A_MOTOR_STARTUP2_ADDRESS, mct8329a_parity_32(0xec30be83));
        mct8329a_write_32(addr, MCT8329A_CLOSED_LOOP1_ADDRESS, mct8329a_parity_32(0x20a65600));
        mct8329a_write_32(addr, MCT8329A_CLOSED_LOOP2_ADDRESS, mct8329a_parity_32(0x02a1a47C));
        mct8329a_write_32(addr, MCT8329A_CLOSED_LOOP3_ADDRESS, mct8329a_parity_32(0x34c92001));
        mct8329a_write_32(addr, MCT8329A_CLOSED_LOOP4_ADDRESS, mct8329a_parity_32(0x00a1c953));
        mct8329a_write_32(addr, MCT8329A_CONST_SPEED_ADDRESS, mct8329a_parity_32(0x7f040001));
        mct8329a_write_32(addr, MCT8329A_CONST_PWR_ADDRESS, mct8329a_parity_32(0x60000240));
        mct8329a_write_32(addr, MCT8329A_FAULT_CONFIG1_ADDRESS, mct8329a_parity_32(0x71783604));
        mct8329a_write_32(addr, MCT8329A_FAULT_CONFIG2_ADDRESS, mct8329a_parity_32(0x00e20380));
        mct8329a_write_32(addr, MCT8329A_PIN_CONFIG1_ADDRESS, mct8329a_parity_32(0x00000002));
        mct8329a_write_32(
            addr, MCT8329A_PIN_CONFIG2_ADDRESS, mct8329a_parity_32(0x380021e0 | (addr << 20))
        );
        mct8329a_write_32(addr, MCT8329A_DEVICE_CONFIG_ADDRESS, mct8329a_parity_32(0x00000807));
        mct8329a_write_32(addr, MCT8329A_GD_CONFIG1_ADDRESS, mct8329a_parity_32(0x0000003b));
        mct8329a_write_32(addr, MCT8329A_GD_CONFIG2_ADDRESS, mct8329a_parity_32(0x00000000));

        mct8329a_write_32(addr, MCT8329A_ALGO_CTRL1_ADDRESS, 0x8A500000);
    }

    sleep_ms(10U);

    set_bq25703a_adc_option(
        &reg16, true, false, true, false, true, false, true, false, BQ25703A_ADC_FULLSCALE_3_06_V,
        false, BQ25703A_ADC_CONV_TYPE_CONTINUOUS
    );
    i2c_write_16(I2C_ADDRESS_CHG_CONTROLLER, BQ25703A_ADC_OPTION_ADDRESS, reg16);
    set_bq25703a_charge_option_1(
        &reg16, false, false, false, BQ25703A_CMP_DEG_TIME_1_US, BQ25703A_CMP_POL_LOW_BELOW_THRESH,
        BQ25703A_CMP_REF_V_2_3_V, BQ25703A_PSYS_RATIO_1_UA_PER_W, BQ25703A_SENSE_RESISTANCE_MOHM_10,
        BQ25703A_SENSE_RESISTANCE_MOHM_10, false, BQ25703A_PROCHOT_LWPWR_MODE_DISABLE, false
    );
    i2c_write_16(I2C_ADDRESS_CHG_CONTROLLER, BQ25703A_CHARGE_OPTION_1_ADDRESS, reg16);
    set_bq25703a_charge_option_2(
        &reg16, BQ25703A_DISCHARGE_OC_THRESH_PROCHOT_200_PCT, true,
        BQ25703A_CHARGE_OC_THRESH_IDPM_210_PCT, false, BQ25703A_CHARGE_OC_THRESH_ACPACN_150_MV,
        BQ25703A_Q2_OC_THRESH_150_MV, false, false, BQ25703A_PKPWR_OVLD_TIME_10_MS, false, false,
        false, false, BQ25703A_PKPWR_OVLD_TIME_1_MS
    );
    i2c_write_16(I2C_ADDRESS_CHG_CONTROLLER, BQ25703A_CHARGE_OPTION_2_ADDRESS, reg16);
    set_bq25703a_min_sys_voltage(&reg16, 1.024f);
    i2c_write_16(I2C_ADDRESS_CHG_CONTROLLER, BQ25703A_MIN_SYS_VOLTAGE_ADDRESS, reg16);
}

static void run_100ms(uint32_t cycle) {
    temp_i2c();
    bms_i2c();
    cc_i2c();

    // MTR
    // reg32 = mct8329a_read_32(I2C_ADDRESS_MTR_RL, 0xe0);
    // reg32 = mct8329a_read_32(I2C_ADDRESS_MTR_RL, 0xe2);
    // reg32 = mct8329a_read_32(I2C_ADDRESS_MTR_RL, 0xe4);
    // reg32 = mct8329a_read_32(I2C_ADDRESS_MTR_RL, 0xea);

    for (int addr = 0x60; addr < 0x64; addr++) {
        // Auto-clear faults.
        bool bst_uv_fault, ocp_vds_fault, drv_off, mtr_lck, loss_of_sync, lock_ilimit;

        reg32 = mct8329a_read_32(addr, MCT8329A_INPUT_DUTY_ADDRESS);
        reg32 = mct8329a_read_32(addr, MCT8329A_CURR_DUTY_ADDRESS);
        reg32 = mct8329a_read_32(addr, MCT8329A_SET_DUTY_ADDRESS);

        reg32 = mct8329a_read_32(addr, MCT8329A_GATE_DRIVER_FAULT_STATUS_ADDRESS);
        get_mct8329a_gate_driver_fault_status(
            reg32, &drv_off, NULL, &bst_uv_fault, NULL, &ocp_vds_fault, NULL, NULL
        );
        reg32 = mct8329a_read_32(addr, MCT8329A_CONTROLLER_FAULT_STATUS_ADDRESS);
        get_mct8329a_controller_fault_status(
            reg32, NULL, NULL, NULL, NULL, NULL, &lock_ilimit, NULL, &mtr_lck, NULL, &loss_of_sync, NULL,
            NULL, NULL, NULL
        );

        if (drv_off || bst_uv_fault || ocp_vds_fault || (mtr_lck && loss_of_sync)) {
            set_mct8329a_algo_ctrl1(&reg32, false, 0x00, 0x00, true, false, false);
            mct8329a_write_32(addr, MCT8329A_ALGO_CTRL1_ADDRESS, reg32);
        }
    }
    values.debug_32_fl = mct8329a_read_32(I2C_ADDRESS_MTR_FL, 0xe0);
    values.debug_32_fr = mct8329a_read_32(I2C_ADDRESS_MTR_FR, 0xe0);
    values.debug_32_rl = mct8329a_read_32(I2C_ADDRESS_MTR_RL, 0xe0);
    values.debug_32_rr = mct8329a_read_32(I2C_ADDRESS_MTR_RR, 0xe0);

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
}

module_t hp_i2c_module = {
    .name       = "hp_i2c",
    .init       = init,
    .run_1ms    = NULL,
    .run_10ms   = NULL,
    .run_100ms  = run_100ms,
    .run_1000ms = NULL,
};
