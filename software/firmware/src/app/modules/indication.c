#include "indication.h"

#include <pico/stdlib.h>

#include "values.h"

#define LED_PIN 25

static void init(void) {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
}

static void run_100ms(void) { gpio_put(LED_PIN, values.led_state); }

module_t indication_module = {
    .name       = "indication",
    .init       = init,
    .run_1ms    = NULL,
    .run_10ms   = NULL,
    .run_100ms  = run_100ms,
    .run_1000ms = NULL,
};
