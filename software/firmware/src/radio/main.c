#include <hardware/flash.h>
#include <hardware/resets.h>
#include <hardware/spi.h>
#include <pico/stdlib.h>
#include <stdint.h>
#include <string.h>

#include "generated/sx1262.h"
#include "hw_config.h"
#include "pico/time.h"

uint8_t tx_buf[256];
uint8_t rx_buf[256];

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
        while (true) {
        }
    }
    sleep_us(1U);
}

void init_radio(void) {
    spi_init(SX_SPI, SX_SPI_FREQ);
    gpio_set_function(PIN_NUM_SX_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_NUM_SX_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_NUM_SX_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_NUM_SX_CS);
    gpio_set_dir(PIN_NUM_SX_CS, GPIO_OUT);
    gpio_put(PIN_NUM_SX_CS, true);

    gpio_init(PIN_NUM_SX_BUSY);
    gpio_set_dir(PIN_NUM_SX_BUSY, GPIO_IN);

    sx1262_pack_set_packet_type(tx_buf, SX1262_PACKET_TYPE_LORA);
    spi_xfer(SX1262_SET_PACKET_TYPE_XFER_SIZE);

    sx1262_pack_set_rf_frequency(tx_buf, RF_FREQ_TO_REG(915000000U));
    spi_xfer(SX1262_SET_RF_FREQUENCY_XFER_SIZE);

    sx1262_pack_set_pa_config(tx_buf, SX1262_PA_CONFIG_SX1262_22_DBM);
    spi_xfer(SX1262_SET_PA_CONFIG_XFER_SIZE);

    sx1262_pack_set_tx_params(tx_buf, 22U, SX1262_RAMP_TIME_US_80);
    spi_xfer(SX1262_SET_TX_PARAMS_XFER_SIZE);

    sx1262_pack_set_buffer_base_addr(tx_buf, 0U, 128U);
    spi_xfer(SX1262_SET_BUFFER_BASE_ADDR_XFER_SIZE);

    sx1262_pack_set_lora_modulation_params(
        tx_buf, SX1262_SF_SF5, SX1262_LORA_BW_500_KHZ, SX1262_CR_45, false
    );
    spi_xfer(SX1262_SET_LORA_MODULATION_PARAMS_XFER_SIZE);

    sx1262_pack_set_lora_packet_params(
        tx_buf, 4U, SX1262_LORA_HEADER_TYPE_IMPLICIT, 9U, false, false
    );
    spi_xfer(SX1262_SET_LORA_PACKET_PARAMS_XFER_SIZE);

    sx1262_pack_set_dio2_as_rf_switch_ctrl(tx_buf, true);
    spi_xfer(SX1262_SET_DIO2_AS_RF_SWITCH_CTRL_XFER_SIZE);
}

static void init_uart(void) {
    uart_init(uart0, 115200);
    uart_set_fifo_enabled(uart0, true);

    gpio_set_function(28, GPIO_FUNC_UART);
    gpio_set_function(29, GPIO_FUNC_UART);
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

int main() {
    sleep_ms(10U);
    init_radio();
    init_uart();

    uint8_t count = 0;
    while (true) {
        while (uart_is_readable(uart0)) {
            uint8_t c = uart_getc(uart0);
            tx_buf[2 + count++] = c;
            if (count >= 9) {
                transmit(count);
                count = 0;
                set_receive();
            }
        }

        if (rx_done()) {
            sx1262_pack_read_buffer(tx_buf, 128U);
            spi_xfer(3 + 9);
            for (int i = 0; i < 9; i++) {
                uart_putc(uart0, rx_buf[3 + i]);
            }
            set_receive();
        }
    }

    /*
    while (true) {
        sx1262_status_chip_mode_t chip_mode;
        sx1262_status_cmd_t status_cmd;

        sx1262_pack_set_rx(tx_buf, 0U);
        spi_xfer(SX1262_SET_RX_XFER_SIZE);

        while (true) {
            sx1262_pack_get_status(tx_buf);
            spi_xfer(SX1262_GET_STATUS_XFER_SIZE);
            sx1262_unpack_get_status(rx_buf, &status_cmd, &chip_mode);

            if (chip_mode == SX1262_STATUS_CHIP_MODE_STDBY_RC &&
                status_cmd == SX1262_STATUS_CMD_DATA_AVAIL) {
                break;
            }
        }

        tx_buf[0] = 0x14;
        spi_xfer(5);
        int8_t rssi_pkt        = -rx_buf[2] / 2;
        int8_t snr_pkt         = -rx_buf[3] / 2;
        int8_t signal_rssi_pkt = -rx_buf[4] / 2;

        sx1262_pack_read_buffer(tx_buf, 0U);
        spi_xfer(3 + 8);

        absolute_time_t start_time = get_absolute_time();
        printf(
            "%08ld: %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x rssi=%d, snr=%d, lora-rssi=%d\n",
            to_ms_since_boot(start_time), rx_buf[3], rx_buf[4], rx_buf[5], rx_buf[6], rx_buf[7],
            rx_buf[8], rx_buf[9], rx_buf[10], rssi_pkt, snr_pkt, signal_rssi_pkt
        );
    }
    */
}
