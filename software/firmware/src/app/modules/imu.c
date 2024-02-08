#include "imu.h"

#include "generated/lsm6dsm.h"
#include "generated/values.h"
#include "hardware/spi.h"
#include "hw_config.h"
#include "pico/stdlib.h"

static uint8_t rx_buf[16];
static uint8_t tx_buf[16];

#define READ_BIT       0x80
#define IMU_FULL_SCALE 8.0f

static void init(void) {
    spi_init(IMU_SPI, IMU_SPI_FREQ);
    gpio_set_function(PIN_NUM_IMU_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_NUM_IMU_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_NUM_IMU_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_NUM_IMU_CS);
    gpio_set_dir(PIN_NUM_IMU_CS, GPIO_OUT);
    gpio_put(PIN_NUM_IMU_CS, true);
}

static void run_100ms(void) {
    tx_buf[0] = 0x10;
    tx_buf[1] = 0x7C;
    gpio_put(PIN_NUM_IMU_CS, false);
    spi_write_read_blocking(IMU_SPI, tx_buf, rx_buf, 1U + 1U);
    gpio_put(PIN_NUM_IMU_CS, true);

    tx_buf[0] = READ_BIT | 0x28;
    tx_buf[1] = 0x00;
    gpio_put(PIN_NUM_IMU_CS, false);
    spi_write_read_blocking(IMU_SPI, tx_buf, rx_buf, 1U + 6U);
    gpio_put(PIN_NUM_IMU_CS, true);

    values.xl_x = (int16_t)(rx_buf[2] << 8 | rx_buf[1]) / (float)(0x8000) * IMU_FULL_SCALE;
    values.xl_y = (int16_t)(rx_buf[4] << 8 | rx_buf[3]) / (float)(0x8000) * IMU_FULL_SCALE;
    values.xl_z = (int16_t)(rx_buf[6] << 8 | rx_buf[5]) / (float)(0x8000) * IMU_FULL_SCALE;
}

module_t imu_module = {
    .name       = "imu",
    .init       = init,
    .run_1ms    = NULL,
    .run_10ms   = NULL,
    .run_100ms  = run_100ms,
    .run_1000ms = NULL,
};
