#include <hardware/flash.h>
#include <hardware/resets.h>
#include <pico/stdlib.h>
#include <stdint.h>
#include <string.h>
#include <tusb.h>
#include <hardware/spi.h>
#include "generated/sx1262.h"
#include "hw_config.h"
#include "pico/time.h"

uint8_t tx_buf[256];
uint8_t rx_buf[256];

#define XTAL_FREQ (32000000.0f)
#define RF_FREQ_TO_REG(freq) ((uint32_t)((freq) * (double)(1 << 25) / XTAL_FREQ))

void spi_xfer(uint8_t len) {
    while (gpio_get(PIN_NUM_SX_BUSY) == true) {
        // Wait for the SX1262 to be ready
    }
    gpio_put(PIN_NUM_SX_CS, false);
    uint8_t len_actual = spi_write_read_blocking(SX_SPI, tx_buf, rx_buf, len);
    gpio_put(PIN_NUM_SX_CS, true);
    if (len_actual != len) {
        while (true) {}
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

    // 1. If not in STDBY_RC mode, then set the circuit in this mode with the command SetStandby()

    // 2. Define the protocol (LoRa® or FSK) with the command SetPacketType(...)
    sx1262_pack_set_packet_type(tx_buf, SX1262_PACKET_TYPE_LORA);
    spi_xfer(SX1262_SET_PACKET_TYPE_XFER_SIZE);

    // 3. Define the RF frequency with the command SetRfFrequency(...)
    sx1262_pack_set_rf_frequency(tx_buf, RF_FREQ_TO_REG(915000000U));
    spi_xfer(SX1262_SET_RF_FREQUENCY_XFER_SIZE);

    // 4. Define where the data will be stored inside the data buffer in Rx with the command SetBufferBaseAddress(...)
    sx1262_pack_set_buffer_base_addr(tx_buf, 0U, 0U);
    spi_xfer(SX1262_SET_BUFFER_BASE_ADDR_XFER_SIZE);

    // 5. Define the modulation parameter according to the chosen protocol with the command SetModulationParams(...)
    sx1262_pack_set_lora_modulation_params(
        tx_buf, SX1262_SF_SF5, SX1262_LORA_BW_31_KHZ, SX1262_CR_45, false
    );
    spi_xfer(SX1262_SET_LORA_MODULATION_PARAMS_XFER_SIZE);

    // 6. Define the frame format to be used with the command SetPacketParams(...)
    sx1262_pack_set_lora_packet_params(
        tx_buf, 4U, SX1262_LORA_HEADER_TYPE_IMPLICIT, 8U, false, false
    );
    spi_xfer(SX1262_SET_LORA_PACKET_PARAMS_XFER_SIZE);

    // 7. Configure DIO and irq: use the command SetDioIrqParams(...) to select the IRQ RxDone and map this IRQ to a DIO (DIO1 or DIO2 or DIO3), set IRQ Timeout as well.
    // sx1262_pack_set_dio2_as_rf_switch_ctrl(tx_buf, false);
    // spi_xfer(SX1262_SET_DIO2_AS_RF_SWITCH_CTRL_XFER_SIZE);

    // 8. Define Sync Word value: use the command WriteReg(...) to write the value of the register via direct register access.
    // FSK only?

    // 9. Set the circuit in reception mode: use the command SetRx(). Set the parameter to enable timeout or continuous mode
    // 10. Wait for IRQ RxDone or Timeout: the chip will stay in Rx and look for a new packet if the continuous mode is selected otherwise it will goes to STDBY_RC mode.
    // 11. In case of the IRQ RxDone, check the status to ensure CRC is correct: use the command GetIrqStatus()
    // 12. Clear IRQ flag RxDone or Timeout : use the command ClearIrqStatus(). In case of a valid packet (CRC Ok), get the packet length and address of the first byte of the received payload by using the command GetRxBufferStatus(...)
    // 13. In case of a valid packet (CRC Ok), start reading the packet
}

int main() {
    stdio_init_all();
    init_radio();

    while (true) {
        sx1262_status_chip_mode_t chip_mode;
        sx1262_status_cmd_t status_cmd;

        sx1262_pack_set_rx(tx_buf, 0U);
        spi_xfer(SX1262_SET_RX_XFER_SIZE);

        while (true) {

            sx1262_pack_get_status(tx_buf);
            spi_xfer(SX1262_GET_STATUS_XFER_SIZE);
            sx1262_unpack_get_status(rx_buf, &status_cmd, &chip_mode);

            if (chip_mode == SX1262_STATUS_CHIP_MODE_STDBY_RC && status_cmd == SX1262_STATUS_CMD_DATA_AVAIL) {
                break;
            }
        }

        tx_buf[0] = 0x14;
        spi_xfer(5);
        int8_t rssi_pkt = -rx_buf[2] / 2;
        int8_t snr_pkt = -rx_buf[3] / 2;
        int8_t signal_rssi_pkt = -rx_buf[4] / 2;

        sx1262_pack_read_buffer(tx_buf, 0U);
        spi_xfer(3 + 8);

        absolute_time_t start_time = get_absolute_time();
        printf("%08ld: %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x rssi=%d, snr=%d, lora-rssi=%d\n",
            to_ms_since_boot(start_time),
            rx_buf[3], rx_buf[4], rx_buf[5], rx_buf[6],
            rx_buf[7], rx_buf[8], rx_buf[9], rx_buf[10],
            rssi_pkt, snr_pkt, signal_rssi_pkt
        );
    }
}
