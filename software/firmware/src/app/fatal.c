#include "fatal.h"

#include <hardware/sync.h>
#include <stdbool.h>

void fatal(fatal_reason_t reason) {
    save_and_disable_interrupts();
    while (true) {
        asm volatile("nop");
    }
}
