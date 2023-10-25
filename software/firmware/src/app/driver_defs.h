#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BIN_TO_FMT_UNSIGNED(val, sz, lsb, offset) ((double)(val) * (lsb) + offset)

#define BIN_TO_FMT_SIGNED(val, sz, lsb, offset)                      \
    (((double)((int32_t)((val) << ((sizeof(int32_t) * 8) - (sz)))) / \
      (1 << ((sizeof(int32_t) * 8) - (sz))) * (lsb)) +               \
     offset)

#define FMT_TO_BIN(val, lsb, type) ((type)((val) / (lsb)))

typedef enum { REG_DEV_STATUS_OK, REG_DEV_STATUS_ERR } reg_dev_status_t;
typedef enum { CMD_DEV_STATUS_OK, CMD_DEV_STATUS_ERR } cmd_dev_status_t;

typedef struct {
    reg_dev_status_t (*read)(uint8_t address, uint8_t reg, uint8_t *data, uint8_t len);
    reg_dev_status_t (*write)(uint8_t address, uint8_t reg, uint8_t *data, uint8_t len);
} reg_dev_operations_t;

typedef struct {
    cmd_dev_status_t (*execute)(uint8_t *in, uint8_t *out, uint16_t in_len, uint16_t out_len);
} cmd_dev_operations_t;
