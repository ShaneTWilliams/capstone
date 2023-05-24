#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    I2C_STATUS_OK,
    I2C_STATUS_ERR
} i2c_status_t;

typedef struct {
    i2c_status_t (*read)(uint8_t address, uint16_t reg, uint8_t *data, uint8_t len);
    i2c_status_t (*write)(uint8_t address, uint16_t reg, uint8_t *data, uint8_t len);
} i2c_operations_t;
