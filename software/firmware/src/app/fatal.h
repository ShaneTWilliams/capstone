typedef enum {
    FATAL_BAD_PB_DECODE          = 0,
    FATAL_BAD_PB_ENCODE          = 1,
    FATAL_BAD_PB_TAG             = 2,
    FATAL_ZERO_PACKET_SIZE       = 3,
    FATAL_UNALIGNED_WRITE_OFFSET = 4,
    FATAL_UNALIGNED_ERASE_OFFSET = 5,
    FATAL_UNALIGNED_ERASE_LENGTH = 6,
    FATAL_FAILED_SPI_XFER        = 7,
    FATAL_UNREACHABLE            = 8,
    FATAL_UART_BUF_OVERRUN       = 9,
} fatal_reason_t;

void fatal(fatal_reason_t reason);
