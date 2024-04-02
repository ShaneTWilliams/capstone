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
#define STARTUP_PERIOD_MS (100U)

static float actual_speed = 0.0f;

typedef enum {
    MOTOR_FL = 0,
    MOTOR_FR,
    MOTOR_RL,
    MOTOR_RR,

    MOTOR_COUNT,
    MOTOR_INVALID
} motor_t;

typedef struct {
    pin_num_t pin;
    uint8_t  slice;
    uint8_t  chan;
} motor_config_t;

motor_config_t motor_config[MOTOR_COUNT] = {
    [MOTOR_FL] = {
        .pin = PIN_NUM_MTR_FL_SPEED,
        .slice = (PIN_NUM_MTR_FL_SPEED >> 1U) & 7U,
        .chan = PIN_NUM_MTR_FL_SPEED & 1U,
    },
    [MOTOR_FR] = {
        .pin = PIN_NUM_MTR_FR_SPEED,
        .slice = (PIN_NUM_MTR_FR_SPEED >> 1U) & 7U,
        .chan = PIN_NUM_MTR_FR_SPEED & 1U,
    },
    [MOTOR_RL] = {
        .pin = PIN_NUM_MTR_RL_SPEED,
        .slice = (PIN_NUM_MTR_RL_SPEED >> 1U) & 7U,
        .chan = PIN_NUM_MTR_RL_SPEED & 1U,
    },
    [MOTOR_RR] = {
        .pin = PIN_NUM_MTR_RR_SPEED,
        .slice = (PIN_NUM_MTR_RR_SPEED >> 1U) & 7U,
        .chan = PIN_NUM_MTR_RR_SPEED & 1U,
    },
};

motor_t startup_state = (motor_t)0U;
uint32_t startup_timer = 0U;

void set_speed(float speed_pct, motor_t motor) {
    uint32_t sys_freq_hz = clock_get_hz(clk_sys);
    uint16_t level = sys_freq_hz / PWM_FREQ / PWM_CLKDIV * speed_pct;
    pwm_set_chan_level(motor_config[motor].slice, motor_config[motor].chan, level);
}

static void init(void) {
    for (motor_t motor = 0; motor < MOTOR_COUNT; motor++) {
        gpio_set_function(motor_config[motor].pin, GPIO_FUNC_PWM);

        uint32_t sys_freq_hz = clock_get_hz(clk_sys);
        uint32_t count       = sys_freq_hz / PWM_FREQ / PWM_CLKDIV;

        pwm_set_wrap(motor_config[motor].slice, count);
        pwm_set_clkdiv(motor_config[motor].slice, PWM_CLKDIV);
        pwm_set_enabled(motor_config[motor].slice, true);

        set_speed(0.0f, motor);
    }
}

static void run_10ms(uint32_t cycle) {
    if (values.ignition) {
        float target_speed =
            MIN(MAX(values.throttle * (1.0f - IDLE_SPEED) + IDLE_SPEED, IDLE_SPEED), 1.0f);
        if (target_speed > actual_speed) {
            actual_speed = MIN(actual_speed + MAX_ACC_INC, target_speed);
        } else {
            actual_speed = MAX(actual_speed - MAX_DEC_INC, target_speed);
        }

        if (startup_state < MOTOR_COUNT && startup_timer++ > STARTUP_PERIOD_MS) {
            startup_state++;
            startup_timer = 0U;
        }

        switch (startup_state) {
            case MOTOR_COUNT:
            case 3:
                set_speed(actual_speed, MOTOR_FL);
            case 2:
                set_speed(actual_speed, MOTOR_FR);
            case 1:
                set_speed(actual_speed, MOTOR_RL);
            case 0:
                set_speed(actual_speed, MOTOR_RR);
                break;
            default:
                fatal(FATAL_UNREACHABLE);
        }
    } else {
        actual_speed = 0.0f;
        startup_state = (motor_t)0U;
        startup_timer = 0U;

        for (motor_t motor = 0U; motor < MOTOR_COUNT; motor++) {
            set_speed(0.0f, motor);
        }
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
