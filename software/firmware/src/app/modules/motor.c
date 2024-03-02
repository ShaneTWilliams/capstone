#include "motor.h"

#include "fatal.h"
#include "generated/values.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hw_config.h"
#include "pico/stdlib.h"
#include "util.h"

#define PWM_CLKDIV  (4.0f)
#define PWM_FREQ    (20000.0f)
#define IDLE_SPEED  (0.15f)
#define MAX_ACC_INC (0.01f)
#define MAX_DEC_INC (0.1f)

static uint8_t slice_num;
static uint8_t chan_num;

static float actual_speed = 0.0f;

void set_speed(float speed_pct) {
    uint32_t sys_freq_hz = clock_get_hz(clk_sys);
    pwm_set_chan_level(
        slice_num, chan_num, (uint16_t)(sys_freq_hz / PWM_FREQ / PWM_CLKDIV * speed_pct)
    );
}

static void init(void) {
    gpio_set_function(PIN_NUM_MTR_RL_SPEED, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(PIN_NUM_MTR_RL_SPEED);
    chan_num  = pwm_gpio_to_channel(PIN_NUM_MTR_RL_SPEED);

    uint32_t sys_freq_hz = clock_get_hz(clk_sys);
    uint32_t count       = sys_freq_hz / PWM_FREQ / PWM_CLKDIV;
    pwm_set_wrap(slice_num, count);

    pwm_set_clkdiv(slice_num, PWM_CLKDIV);
    pwm_set_enabled(slice_num, true);
}

static void run_10ms(uint32_t cycle) {
    if (false) {
        float target_speed =
            MIN(MAX(values.throttle * (1.0f - IDLE_SPEED) + IDLE_SPEED, IDLE_SPEED), 1.0f);
        if (target_speed > actual_speed) {
            actual_speed = MIN(actual_speed + MAX_ACC_INC, target_speed);
        } else {
            actual_speed = MAX(actual_speed - MAX_DEC_INC, target_speed);
        }
        set_speed(actual_speed);
    } else {
        set_speed(0.0f);
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
