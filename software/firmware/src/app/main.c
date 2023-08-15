#include <pico/stdlib.h>

int main(void) {
    gpio_init(0);
    gpio_set_dir(0, GPIO_OUT);
    while (true) {
        gpio_put(0, 1);
        sleep_ms(100);
        gpio_put(0, 0);
        sleep_ms(100);
    }
}
