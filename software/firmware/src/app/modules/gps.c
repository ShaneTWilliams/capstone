#include <stdlib.h>

#include "fatal.h"
#include "generated/values.h"
#include "hardware/uart.h"
#include "hw_config.h"
#include "indication.h"
#include "lp_i2c.h"
#include "pico/stdlib.h"

/*
(gdb) p rx_buffers
$1 = {"$GNRMC,235119.00,A,4729.98860,N,05247.71168,W,0.036,,060124,,,A,", "$GNGGA,235119.00,4729.98860,N,05247.71168,W,1,07,1.72,117.7,M,10",
  "$GNGSA,A,3,14,17,21,03,22,08,02,,,,,,3.66,1.72,3.23,1*0D\r6\r\000\000\000\000", "$GNGSA,A,3", ',' <repeats 13 times>, "3.66,1.72,3.23,4*01\r", '\000' <repeats 20 times>,
  "$GPGSV,3,1,11,01,70,055,22,02,49,057,28,03,44,130,32,06,03,234,,", "$GPGSV,3,2,11,08,15,101,22,14,69,232,32,17,56,295,25,21,34,058,3",
  "$GPGSV,3,3,11,22,61,287,31,30,10,222,10,32,03,037,22,0*5A\r\000\000\000\000\000", "$BDGSV,1,1,00,0*74\r", '\000' <repeats 44 times>,
  "$GNRMC,235120.00,A,4729.98864,N,05247.71163,W,0.048,,060124,,,A,", "$GNGGA,235120.00,4729.98864,N,05247.71163,W,1,07,1.72,117.8,M,10",
  "$GNGSA,A,3,14,17,21,03,22,08,02,,,,,,,,2.16,1.34,1.69,1*06\r\000\000\000\000", "$GNGSA,A,3", ',' <repeats 13 times>, "2.16,1.34,1.69,4*09\r", '\000' <repeats 20 times>,
  "$GPGSV,3,1,11,01,70,055,21,02,49,057,27,03,44,130,32,06,03,234,,", "$GPGSV,3,2,11,08,15,101,22,14,69,232,31,17,56,295,23,21,34,058,3",
  "$GPGSV,3,3,11,22,61,287,32,30,10,222,12,32,03,037,22,0*5B\r\000\000\000\000\000", "$BDGSV,1,1,00,0*74\r", '\000' <repeats 44 times>}
*/

#define MAX_SENTENCE_LENGTH (100U)
#define MAX_NUM_FIELDS      (20U)
#define MAX_FIELD_SIZE      (20U)

typedef enum {
    SENTENCE_ID_RMC,
    SENTENCE_ID_GGA,
    SENTENCE_ID_GSA,
    SENTENCE_ID_GSV,

    SENTENCE_ID_COUNT,
    SENTENCE_ID_INVALID,
} sentence_id_t;

typedef enum {
    MESSAGE_OFFSET_START       = 0,
    MESSAGE_OFFSET_TALKER_ID   = 1,
    MESSAGE_OFFSET_SENTENCE_ID = 3,

    // GGA
    MESSAGE_OFFSET_GGA_TIME = 7,
} message_offset_t;

typedef enum {
    FIELD_LENGTH_TALKER_ID   = 2,
    FIELD_LENGTH_SENTENCE_ID = 3,
} field_length_t;

typedef enum {
    // GGA
    FIELD_INDEX_GGA_TIME          = 0,
    FIELD_INDEX_GGA_LATITUDE      = 1,
    FIELD_INDEX_GGA_LATITUDE_DIR  = 2,
    FIELD_INDEX_GGA_LONGITUDE     = 3,
    FIELD_INDEX_GGA_LONGITUDE_DIR = 4,
} field_index_t;

char sentence_ids[SENTENCE_ID_COUNT][4] = {
    "RMC",
    "GGA",
    "GSA",
    "GSV",
};

typedef enum {
    PARSE_ERROR_NONE,
    PARSE_ERROR_START,
    PARSE_ERROR_TALKER_ID,
    PARSE_ERROR_SENTENCE_ID,
} parse_error_t;

static uint8_t rx_buf[MAX_SENTENCE_LENGTH];
static uint8_t sentence_fields[MAX_NUM_FIELDS][MAX_FIELD_SIZE];
static parse_error_t parse_error = PARSE_ERROR_NONE;

static void init(void) {
    uart_init(GPS_UART, GPS_UART_BAUD_RATE);
    uart_set_fifo_enabled(GPS_UART, true);

    gpio_set_function(PIN_NUM_GPS_RX, GPIO_FUNC_UART);
    gpio_set_function(PIN_NUM_GPS_TX, GPIO_FUNC_UART);
}

void parse_gga(void) {
    static char field_buf[MAX_FIELD_SIZE + 1];

    // Latitude.
    field_buf[9] = '\0';
    memcpy(field_buf, sentence_fields[FIELD_INDEX_GGA_LATITUDE], 2);
    field_buf[2] = '.';
    memcpy(field_buf + 3, sentence_fields[FIELD_INDEX_GGA_LATITUDE] + 2, 2);
    memcpy(field_buf + 5, sentence_fields[FIELD_INDEX_GGA_LATITUDE] + 5, 5);
    values.drone_latitude = atof(field_buf);

    // Longitude.
    field_buf[10] = '\0';
    memcpy(field_buf, sentence_fields[FIELD_INDEX_GGA_LONGITUDE], 3);
    field_buf[3] = '.';
    memcpy(field_buf + 4, sentence_fields[FIELD_INDEX_GGA_LONGITUDE] + 3, 2);
    memcpy(field_buf + 6, sentence_fields[FIELD_INDEX_GGA_LONGITUDE] + 6, 5);
    values.drone_longitude = atof(field_buf);
}

void parse_sentence(void) {
    if (rx_buf[0] != '$') {
        parse_error = PARSE_ERROR_START;
        return;
    }

    sentence_id_t found_id = SENTENCE_ID_INVALID;
    for (sentence_id_t id = 0; id < SENTENCE_ID_COUNT; id++) {
        if (strncmp(
                sentence_ids[id], (char*)rx_buf + MESSAGE_OFFSET_SENTENCE_ID,
                FIELD_LENGTH_SENTENCE_ID
            ) == 0) {
            found_id = id;
            break;
        }
    }

    if (found_id == SENTENCE_ID_INVALID) {
        parse_error = PARSE_ERROR_SENTENCE_ID;
        return;
    }

    uint8_t field_index = 0;
    uint8_t field_start = 7;
    for (uint8_t i = field_start; i < MAX_SENTENCE_LENGTH; i++) {
        if (rx_buf[i] == ',') {
            memcpy(sentence_fields[field_index], rx_buf + field_start, i - field_start);
            field_start = i + 1;
            field_index++;
            if (field_index >= MAX_NUM_FIELDS) {
                break;
            }
        }
    }

    switch (found_id) {
        case SENTENCE_ID_GGA:
            parse_gga();
            break;
        case SENTENCE_ID_RMC:
        case SENTENCE_ID_GSA:
        case SENTENCE_ID_GSV:
        default:
            break;
    }
}

static void run_1ms(uint32_t cycle) {
    static uint8_t index = 0;

    while (uart_is_readable(GPS_UART) > 0) {
        uint8_t c = uart_getc(GPS_UART);
        if (c == '\n') {
            parse_sentence();
            index = 0;
        } else {
            if (index >= sizeof(rx_buf)) {
                // fatal(UART_BUF_OVERRUN);
                index = 0;
            }
            rx_buf[index++] = c;
        }
    }
}

module_t gps_module = {
    .name       = "gps",
    .init       = init,
    .run_1ms    = run_1ms,
    .run_10ms   = NULL,
    .run_100ms  = NULL,
    .run_1000ms = NULL,
};
