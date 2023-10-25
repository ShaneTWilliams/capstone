#include <hardware/flash.h>
#include <hardware/resets.h>
#include <hardware/structs/scb.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <pico/stdlib.h>
#include <stdint.h>
#include <string.h>
#include <tusb.h>

#include "boot.pb.h"

#define RX_BUFFER_SIZE Request_size
#define TX_BUFFER_SIZE Response_size
#define WRITE_SIZE     128U

#define BOOT_PIN 16U

typedef enum {
    BAD_PB_DECODE          = 0,
    BAD_PB_ENCODE          = 1,
    BAD_PB_TAG             = 2,
    ZERO_PACKET_SIZE       = 3,
    UNALIGNED_WRITE_OFFSET = 4,
    UNALIGNED_ERASE_OFFSET = 5,
    UNALIGNED_ERASE_LENGTH = 6,
} fatal_reason_t;

static Response response;

extern uint32_t __APP_VECTOR_TABLE, __APPLICATION_START, __FLASH_START;

static void fatal(fatal_reason_t reason) {
    save_and_disable_interrupts();
    while (true) {
        asm volatile("nop");
    }
}

static void jump_to_app() {
    reset_block(
        ~(RESETS_RESET_IO_QSPI_BITS | RESETS_RESET_PADS_QSPI_BITS | RESETS_RESET_SYSCFG_BITS |
          RESETS_RESET_PLL_SYS_BITS)
    );

    uint32_t* vtor        = &__APP_VECTOR_TABLE;
    uint32_t reset_vector = *(uint32_t*)((uint8_t*)vtor + 0x04);

    scb_hw->vtor = (volatile uint32_t)(vtor);

    asm volatile("msr msp, %0" ::"g"(*(volatile uint32_t*)vtor));
    asm volatile("bx %0" ::"r"(reset_vector));
}

static void handle_request(Request* request) {
    static uint8_t flash_write_buffer[FLASH_PAGE_SIZE];
    const uint32_t APP_START_FLASH_OFFSET =
        // cppcheck-suppress comparePointers
        (uint32_t)((uint8_t*)&__APPLICATION_START - (uint8_t*)&__FLASH_START);

    uint32_t flags = save_and_disable_interrupts();

    switch (request->which_payload) {
        case Request_erase_tag:
            response.which_payload = Response_erase_tag;
            if (request->payload.erase.offset % FLASH_SECTOR_SIZE != 0) {
                fatal(UNALIGNED_ERASE_OFFSET);
            }
            if (request->payload.erase.length % FLASH_SECTOR_SIZE != 0) {
                fatal(UNALIGNED_ERASE_LENGTH);
            }
            flash_range_erase(
                APP_START_FLASH_OFFSET + request->payload.erase.offset,
                request->payload.erase.length
            );
            break;
        case Request_write_tag:
            response.which_payload = Response_write_tag;
            if (request->payload.write.offset % WRITE_SIZE != 0) {
                fatal(UNALIGNED_WRITE_OFFSET);
            }
            // Need to write 256 B at a time. Put our 128 B at the right offset
            // then fill the rest of the buffer with 0xFF so we don't zero the
            // rest of the page.
            memcpy(
                flash_write_buffer + (request->payload.write.offset % FLASH_PAGE_SIZE),
                request->payload.write.data, WRITE_SIZE
            );
            memset(
                flash_write_buffer + WRITE_SIZE - (request->payload.write.offset % FLASH_PAGE_SIZE),
                0xFF, WRITE_SIZE
            );

            uint32_t page_start_offset =
                ((request->payload.write.offset / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE);
            flash_range_program(
                APP_START_FLASH_OFFSET + page_start_offset, flash_write_buffer, FLASH_PAGE_SIZE
            );
            break;
        case Request_go_tag:
            jump_to_app();
            break;
        default:
            fatal(BAD_PB_TAG);
            break;
    }

    restore_interrupts(flags);
}

static void send_response() {
    static uint8_t tx_buffer[TX_BUFFER_SIZE];

    // Send it.
    pb_ostream_t output_stream = pb_ostream_from_buffer(tx_buffer, sizeof(tx_buffer));
    if (!pb_encode(&output_stream, Response_fields, &response)) {
        fatal(BAD_PB_ENCODE);
        return;
    }
    tud_cdc_write(&output_stream.bytes_written, 1);
    tud_cdc_write(tx_buffer, output_stream.bytes_written);
    tud_cdc_write_flush();
}

int main() {
    gpio_init(BOOT_PIN);
    gpio_set_dir(BOOT_PIN, GPIO_IN);

    if (gpio_get(BOOT_PIN)) {
        jump_to_app();
    }

    tusb_init();

    while (true) {
        tud_task();

        while (tud_cdc_available()) {
            static uint8_t rx_buffer[RX_BUFFER_SIZE];
            static uint8_t remaining_bytes = 0;
            static uint8_t packet_size     = 0;

            if (packet_size == 0) {
                uint32_t count = tud_cdc_read(&remaining_bytes, 1);
                if (count == 0) {
                    fatal(ZERO_PACKET_SIZE);
                }
                packet_size = remaining_bytes;
            }

            uint32_t count =
                tud_cdc_read(rx_buffer + packet_size - remaining_bytes, remaining_bytes);
            remaining_bytes -= count;
            if (remaining_bytes > 0) {
                continue;
            }

            Request request     = Request_init_zero;
            pb_istream_t stream = pb_istream_from_buffer(rx_buffer, packet_size);
            if (!pb_decode(&stream, Request_fields, &request)) {
                fatal(BAD_PB_DECODE);
            }

            packet_size = 0;

            handle_request(&request);
            send_response();
        }
    }
}
