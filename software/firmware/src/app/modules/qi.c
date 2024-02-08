#include "qi.h"

static void init(void) {}

module_t qi_module = {
    .name       = "qi",
    .init       = init,
    .run_1ms    = NULL,
    .run_10ms   = NULL,
    .run_100ms  = NULL,
    .run_1000ms = NULL,
};
