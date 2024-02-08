#include "motor.h"

#include "fatal.h"
#include "generated/values.h"
#include "hw_config.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "globals.h"

#define PWM_CLKDIV (4.0f)
#define PWM_FREQ (20000.0f)

static uint8_t slice_num;
static uint8_t chan_num;

void set_speed(float speed_pct) {
    uint32_t sys_freq_hz = clock_get_hz(clk_sys);
    pwm_set_chan_level(slice_num, chan_num, (uint16_t)(sys_freq_hz / PWM_FREQ / PWM_CLKDIV * speed_pct));
}

static void init(void) {
    gpio_set_function(PIN_NUM_MTR_FL_SPEED, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(PIN_NUM_MTR_FL_SPEED);
    chan_num  = pwm_gpio_to_channel(PIN_NUM_MTR_FL_SPEED);

    uint32_t sys_freq_hz = clock_get_hz(clk_sys);
    uint32_t count = sys_freq_hz / PWM_FREQ / PWM_CLKDIV;
    pwm_set_wrap(slice_num, count);

    pwm_set_clkdiv(slice_num, PWM_CLKDIV);
    pwm_set_enabled(slice_num, true);
}

static void run_10ms(void) {
    if (cycles_10ms > 200 && cycles_10ms < 1200) {
        set_speed(0.01);
    } else {
        set_speed(0);
    }
}

module_t motor_module = {
    .name       = "motor",
    .init       = init,
    .run_1ms    = NULL,
    .run_10ms   = run_10ms,
    .run_100ms  = NULL,
    .run_1000ms = NULL,
};
