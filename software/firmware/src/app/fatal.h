typedef enum {
    BAD_PB_DECODE          = 0,
    BAD_PB_ENCODE          = 1,
    BAD_PB_TAG             = 2,
    ZERO_PACKET_SIZE       = 3,
    UNALIGNED_WRITE_OFFSET = 4,
    UNALIGNED_ERASE_OFFSET = 5,
    UNALIGNED_ERASE_LENGTH = 6,
} fatal_reason_t;

void fatal(fatal_reason_t reason);
