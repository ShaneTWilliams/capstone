#include "radio.h"

#include "fatal.h"
#include "generated/sx1262.h"
#include "generated/values.h"
#include "hardware/spi.h"
#include "hw_config.h"
#include "lp_i2c.h"
#include "pico/stdlib.h"

static uint8_t tx_buf[256];
static uint8_t rx_buf[256];

#define XTAL_FREQ            (32000000.0f)
#define RF_FREQ_TO_REG(freq) ((uint32_t)((freq) * (double)(1 << 25) / XTAL_FREQ))

void spi_xfer(uint8_t len) {
    while (gpio_get(PIN_NUM_SX_BUSY) == true) {
        // Wait for the SX1262 to be ready
    }
    gpio_put(PIN_NUM_SX_CS, false);
    uint8_t len_actual = spi_write_read_blocking(SX_SPI, tx_buf, rx_buf, len);
    gpio_put(PIN_NUM_SX_CS, true);
    if (len_actual != len) {
        fatal(FATAL_FAILED_SPI_XFER);
    }
    sleep_us(1U);
}

uint8_t get_status_byte(void) {
    tx_buf[0] = SX1262_GET_STATUS_OPCODE;
    spi_xfer(2U);
    return rx_buf[1];
}

static bool tx_done(void) {
    sx1262_status_chip_mode_t chip_mode;
    sx1262_status_cmd_t status_cmd;
    sx1262_pack_get_status(tx_buf);
    spi_xfer(SX1262_GET_STATUS_XFER_SIZE);
    sx1262_unpack_get_status(rx_buf, &status_cmd, &chip_mode);
    return chip_mode == SX1262_STATUS_CHIP_MODE_STDBY_RC &&
           status_cmd == SX1262_STATUS_CMD_TX_DONE;
}

static bool rx_done(void) {
    sx1262_status_chip_mode_t chip_mode;
    sx1262_status_cmd_t status_cmd;
    sx1262_pack_get_status(tx_buf);
    spi_xfer(SX1262_GET_STATUS_XFER_SIZE);
    sx1262_unpack_get_status(rx_buf, &status_cmd, &chip_mode);
    return chip_mode == SX1262_STATUS_CHIP_MODE_STDBY_RC &&
           status_cmd == SX1262_STATUS_CMD_DATA_AVAIL;
}

static void transmit(uint8_t len) {
    tx_buf[0] = SX1262_WRITE_BUFFER_OPCODE;
    tx_buf[1] = 0x00;
    spi_xfer(len + 2U);

    sx1262_pack_set_tx(tx_buf, 0U);
    spi_xfer(SX1262_SET_TX_XFER_SIZE);

    while (!tx_done()) {}
}

static void set_receive(void) {
    sx1262_pack_set_rx(tx_buf, 0U);
    spi_xfer(SX1262_SET_RX_XFER_SIZE);
}

void init(void) {
    spi_init(SX_SPI, SX_SPI_FREQ);
    gpio_set_function(PIN_NUM_SX_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_NUM_SX_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_NUM_SX_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_NUM_SX_CS);
    gpio_set_dir(PIN_NUM_SX_CS, GPIO_OUT);
    gpio_put(PIN_NUM_SX_CS, true);

    gpio_init(PIN_NUM_SX_BUSY);
    gpio_set_dir(PIN_NUM_SX_BUSY, GPIO_IN);

    tx_buf[0] = SX1262_GET_DEVICE_ERRORS_OPCODE;
    spi_xfer(4U);

    sx1262_pack_set_packet_type(tx_buf, SX1262_PACKET_TYPE_LORA);
    spi_xfer(SX1262_SET_PACKET_TYPE_XFER_SIZE);

    uint32_t reg = RF_FREQ_TO_REG(915000000U);
    sx1262_pack_set_rf_frequency(tx_buf, reg);
    spi_xfer(SX1262_SET_RF_FREQUENCY_XFER_SIZE);

    sx1262_pack_set_pa_config(tx_buf, SX1262_PA_CONFIG_SX1262_22_DBM);
    spi_xfer(SX1262_SET_PA_CONFIG_XFER_SIZE);

    sx1262_pack_set_tx_params(tx_buf, 22U, SX1262_RAMP_TIME_US_80);
    spi_xfer(SX1262_SET_TX_PARAMS_XFER_SIZE);

    sx1262_pack_set_buffer_base_addr(tx_buf, 0U, 128U);
    spi_xfer(SX1262_SET_BUFFER_BASE_ADDR_XFER_SIZE);

    sx1262_pack_set_lora_modulation_params(
        tx_buf, SX1262_SF_SF7, SX1262_LORA_BW_500_KHZ, SX1262_CR_47, false
    );
    spi_xfer(SX1262_SET_LORA_MODULATION_PARAMS_XFER_SIZE);

    sx1262_pack_set_lora_packet_params(
        tx_buf, 4U, SX1262_LORA_HEADER_TYPE_EXPLICIT, 9U, true, false
    );
    spi_xfer(SX1262_SET_LORA_PACKET_PARAMS_XFER_SIZE);

    sx1262_pack_set_dio2_as_rf_switch_ctrl(tx_buf, true);
    spi_xfer(SX1262_SET_DIO2_AS_RF_SWITCH_CTRL_XFER_SIZE);

    tx_buf[0] = SX1262_SET_DIO_IRQ_PARAMS_OPCODE;
    tx_buf[1] = 0xff;
    tx_buf[2] = 0xff;
    tx_buf[3] = 0x00;
    tx_buf[4] = 0x00;
    tx_buf[5] = 0x00;
    tx_buf[6] = 0x00;
    spi_xfer(7U);

    set_receive();
}

static downlink_packet_t downlink_packet = DOWNLINK_PACKET_FIRST;
uint32_t rx_time = 0U;
uint8_t wakeup_count = 0U;

void run_1ms(uint32_t cycle) {
    if (rx_done()) {

        tx_buf[0] = SX1262_GET_IRQ_STATUS_OPCODE;
        spi_xfer(4U);
        uint16_t irq_status = (rx_buf[2] << 8) | rx_buf[3];
        if ((irq_status & 0x0060) != 0x00) {
            tx_buf[0] = SX1262_CLEAR_IRQ_STATUS_OPCODE;
            tx_buf[1] = 0x00;
            tx_buf[2] = 0x60;
            spi_xfer(3U);
            return;
        }

        sx1262_pack_read_buffer(tx_buf, 128U);
        spi_xfer(3 + 9);

        unpack_uplink_packet_0(&rx_buf[3]);

        tx_buf[2] = downlink_packet;
        pack_downlink_packet(downlink_packet, tx_buf + 3U);
        transmit(downlink_packet_lengths[downlink_packet] + 1U);

        set_receive();
        if (++downlink_packet == DOWNLINK_PACKET_COUNT) {
            downlink_packet = DOWNLINK_PACKET_FIRST;
        }
    }
}

module_t radio_module = {
    .name       = "radio",
    .init       = init,
    .run_1ms    = run_1ms,
    .run_10ms   = NULL,
    .run_100ms  = NULL,
    .run_1000ms = NULL,
};
