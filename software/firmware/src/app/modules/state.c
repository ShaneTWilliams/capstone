#include "state.h"

#include "generated/values.h"
#include "indication.h"
#include "iox.h"
#include "hw_config.h"
#include "pico/stdlib.h"

typedef struct {
    void (*execute)(void);
    void (*entry)(void);
    void (*exit)(void);
} state_config_t;

static void _execute_sleep(void) {
    if (!values.sleep_request) {
        values.drone_state = DRONE_STATE_IDLE;
    }
}

static void _enter_sleep(void) {
    set_iox_gpio_output_state(IOX_GPIO_LED_SHDN, false);
    set_iox_gpio_output_state(IOX_GPIO_GPS_RESET, false);
    set_iox_gpio_output_state(IOX_GPIO_MUX_EN, true);
    set_iox_gpio_output_state(IOX_GPIO_50_EN, false);
    set_buzzer(BUZZER_SEQ_ENTER_SLEEP);
}

static void _exit_sleep(void) {
    set_iox_gpio_output_state(IOX_GPIO_LED_SHDN, true);
    set_iox_gpio_output_state(IOX_GPIO_GPS_RESET, true);
    set_iox_gpio_output_state(IOX_GPIO_MUX_EN, false);
    set_iox_gpio_output_state(IOX_GPIO_50_EN, true);
    set_buzzer(BUZZER_SEQ_EXIT_SLEEP);
}

static void _execute_charging(void) {
    if (!values.charge_input_present) {
        values.drone_state = DRONE_STATE_IDLE;
    }
}

static void _enter_charging(void) {
    set_led(LED_SEQ_CHARGING);
    set_buzzer(BUZZER_SEQ_CHARGING_START);
}
static void _exit_charging(void) {
    set_buzzer(BUZZER_SEQ_CHARGING_STOP);
}

static void _execute_idle(void) {
    if (values.ignition) {
        values.drone_state = DRONE_STATE_STARTUP;
    }

    if (values.charge_input_present) {
        values.drone_state = DRONE_STATE_CHARGING;
    }

    if (values.sleep_request) {
        values.drone_state = DRONE_STATE_SLEEP;
    }
}

static void _enter_idle(void) {
    set_led(LED_SEQ_IDLE);
}
static void _exit_idle(void) {}

static void _execute_startup(void) {
    if (!values.ignition) {
        values.drone_state = DRONE_STATE_IDLE;
    }

    if (values.throttle > 0.0f) {
        values.drone_state = DRONE_STATE_FLIGHT;
    }

    if (values.sleep_request) {
        values.drone_state = DRONE_STATE_SLEEP;
    }
}

static void _enter_startup(void) {}
static void _exit_startup(void) {}

static void _execute_flight(void) {
    if (!values.ignition) {
        values.drone_state = DRONE_STATE_IDLE;
    }
}

static void _enter_flight(void) {}
static void _exit_flight(void) {}

static state_config_t _state_cfgs[DRONE_STATE_COUNT] = {
    [DRONE_STATE_SLEEP] =
        {
            .execute = _execute_sleep,
            .entry = _enter_sleep,
            .exit = _exit_sleep,
        },
    [DRONE_STATE_CHARGING] =
        {
            .execute = _execute_charging,
            .entry = _enter_charging,
            .exit = _exit_charging,
        },
    [DRONE_STATE_IDLE] =
        {
            .execute = _execute_idle,
            .entry = _enter_idle,
            .exit = _exit_idle,
        },
    [DRONE_STATE_STARTUP] =
        {
            .execute = _execute_startup,
            .entry = _enter_startup,
            .exit = _exit_startup,
        },
    [DRONE_STATE_FLIGHT] =
        {
            .execute = _execute_flight,
            .entry = _enter_flight,
            .exit = _exit_flight,
        },
};

static void init(void) { values.drone_state = DRONE_STATE_IDLE; }

static void run_10ms(uint32_t cycle) {
    drone_state_t prev_state = values.drone_state;
    _state_cfgs[values.drone_state].execute();
    if (prev_state != values.drone_state) {
        _state_cfgs[prev_state].exit();
        _state_cfgs[values.drone_state].entry();
    }
}

module_t state_module = {
    .name       = "indication",
    .init       = init,
    .run_1ms    = NULL,
    .run_10ms   = run_10ms,
    .run_100ms  = NULL,
    .run_1000ms = NULL,
};
