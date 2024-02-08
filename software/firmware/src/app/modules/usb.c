#include "usb.h"

#include <pb_decode.h>
#include <pb_encode.h>
#include <stddef.h>
#include <tusb.h>

#include "app.pb.h"
#include "fatal.h"
#include "generated/values.h"
#include "indication.h"

#define RX_BUFFER_SIZE Request_size
#define TX_BUFFER_SIZE Response_size
#define WRITE_SIZE     128U

static Response response;

static void handle_request(Request* request) {
    switch (request->which_payload) {
        case Request_get_tag:
            response.which_payload         = Response_get_tag;
            response.payload.get.has_value = true;
            pack_proto_value(&response.payload.get.value, request->payload.get.tag);
            break;
        case Request_set_tag:
            response.which_payload = Response_set_tag;
            unpack_proto_value(&request->payload.set.value, request->payload.set.tag);
            break;
        default:
            fatal(BAD_PB_TAG);
            break;
    }
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

static void init(void) { tusb_init(); }

static void run_1ms(void) {
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

        uint32_t count = tud_cdc_read(rx_buffer + packet_size - remaining_bytes, remaining_bytes);
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

module_t usb_module = {
    .name       = "usb",
    .init       = init,
    .run_1ms    = run_1ms,
    .run_10ms   = NULL,
    .run_100ms  = NULL,
    .run_1000ms = NULL,
};
