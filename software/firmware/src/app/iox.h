#pragma once

#include "generated/tca9539.h"

typedef enum {
    // MISC Port 0
    IOX_GPIO_LED_SHDN,
    IOX_GPIO_PWR_IOX_INT,
    IOX_GPIO_PWR_IOX_RST,
    IOX_GPIO_MTR_IOX_INT,
    IOX_GPIO_MTR_IOX_RST,
    IOX_GPIO_GPS_RESET,
    IOX_GPIO_MUX_A0,

    // MISC Port 1
    IOX_GPIO_MUX_A1,
    IOX_GPIO_MUX_EN,
    IOX_GPIO_SX_RESET,
    IOX_GPIO_SX_DIO1,
    IOX_GPIO_SX_DIO3,
    IOX_GPIO_MCU_BOOT,
    IOX_GPIO_IMU_INT2,
    IOX_GPIO_IMU_INT1,

    // CHG Port 0
    IOX_GPIO_USBPD_FAULT,
    IOX_GPIO_USBPD_INT,
    IOX_GPIO_QI_TEMP_OS,
    IOX_GPIO_QI_EN_2,
    IOX_GPIO_QI_EN_1,
    IOX_GPIO_QI_CHG,
    IOX_GPIO_33_PG,
    IOX_GPIO_50_EN,

    // CHG Port 1
    IOX_GPIO_50_PG,
    IOX_GPIO_TP_1101,
    IOX_GPIO_TP_1102,
    IOX_GPIO_TP_1104,

    // PWR Port 0
    IOX_GPIO_CC_PROCHOT,
    IOX_GPIO_CC_CHRG_OK,
    IOX_GPIO_MTR_TMP_OS,
    IOX_GPIO_BMS_ALRT,
    IOX_GPIO_MTR_RL_nFAULT,
    IOX_GPIO_MTR_RL_BRAKE,

    // PWR Port 1
    IOX_GPIO_MTR_RL_DIR,
    IOX_GPIO_MTR_RL_DRVOFF,

    // MTR Port 0
    IOX_GPIO_MTR_FL_DIR,
    IOX_GPIO_MTR_FL_DRVOFF,
    IOX_GPIO_MTR_FL_BRAKE,
    IOX_GPIO_MTR_FL_nFAULT,
    IOX_GPIO_MTR_FR_DIR,
    IOX_GPIO_MTR_FR_DRVOFF,
    IOX_GPIO_MTR_FR_BRAKE,
    IOX_GPIO_MTR_FR_nFAULT,

    // MTR Port 1
    IOX_GPIO_MTR_RR_DIR,
    IOX_GPIO_MTR_RR_DRVOFF,
    IOX_GPIO_MTR_RR_BRAKE,
    IOX_GPIO_MTR_RR_nFAULT,

    IOX_GPIO_COUNT,
    IOX_GPIO_UNUSED
} iox_gpio_t;

typedef enum {
    IOX_MISC,
    IOX_CHG,
    IOX_PWR,
    IOX_MTR,

    IOX_COUNT,
    IOX_INVALID
} iox_t;

typedef enum {
    IOX_PORT_0,
    IOX_PORT_1,

    IOX_PORT_COUNT,
    IOX_PORT_INVALID
} iox_port_t;

typedef enum {
    IOX_PIN_0,
    IOX_PIN_1,
    IOX_PIN_2,
    IOX_PIN_3,
    IOX_PIN_4,
    IOX_PIN_5,
    IOX_PIN_6,
    IOX_PIN_7,

    IOX_PIN_COUNT,
} iox_pin_t;

// Public interface.
void set_iox_gpio_output_state(iox_gpio_t gpio, bool state);
void set_iox_gpio_direction(iox_gpio_t gpio, tca9539_direction_t direction);
bool get_iox_gpio_input_state(iox_gpio_t gpio);

// For use only by I2C modules.
void set_iox_gpio_input_state(iox_t iox, iox_port_t port, iox_pin_t pin, bool state);
bool get_iox_gpio_output_state(iox_t iox, iox_port_t port, iox_pin_t pin);
tca9539_direction_t get_iox_gpio_direction(iox_t iox, iox_port_t port, iox_pin_t pin);
