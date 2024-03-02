#include "indication.h"

#include "generated/values.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hw_config.h"
#include "lp_i2c.h"
#include "pico/stdlib.h"

#define PWM_CLKDIV          (8.0f)
#define NOTE_TO_HZ(note)    ((float)note / 100.0f)
#define BUZZER_SLICE        (pwm_gpio_to_slice_num(PIN_NUM_BUZZ_CTRL))
#define BUZZER_CHAN         (pwm_gpio_to_channel(PIN_NUM_BUZZ_CTRL))
#define TOGGLE(value, a, b) ((value) = (((value) == (a)) ? (b) : (a)))

typedef enum {
    NOTE_C0  = 1635,
    NOTE_Db0 = 1732,
    NOTE_D0  = 1835,
    NOTE_Eb0 = 1945,
    NOTE_E0  = 2060,
    NOTE_F0  = 2183,
    NOTE_Gb0 = 2312,
    NOTE_G0  = 2450,
    NOTE_Ab0 = 2596,
    NOTE_A0  = 2750,
    NOTE_Bb0 = 2914,
    NOTE_B0  = 3087,
    NOTE_C1  = 3270,
    NOTE_Db1 = 3465,
    NOTE_D1  = 3671,
    NOTE_Eb1 = 3889,
    NOTE_E1  = 4120,
    NOTE_F1  = 4365,
    NOTE_Gb1 = 4625,
    NOTE_G1  = 4900,
    NOTE_Ab1 = 5191,
    NOTE_A1  = 5500,
    NOTE_Bb1 = 5827,
    NOTE_B1  = 6174,
    NOTE_C2  = 6541,
    NOTE_Db2 = 6930,
    NOTE_D2  = 7342,
    NOTE_Eb2 = 7778,
    NOTE_E2  = 8241,
    NOTE_F2  = 8731,
    NOTE_Gb2 = 9250,
    NOTE_G2  = 9800,
    NOTE_Ab2 = 10383,
    NOTE_A2  = 11000,
    NOTE_Bb2 = 11654,
    NOTE_B2  = 12347,
    NOTE_C3  = 13081,
    NOTE_Db3 = 13859,
    NOTE_D3  = 14683,
    NOTE_Eb3 = 15556,
    NOTE_E3  = 16481,
    NOTE_F3  = 17461,
    NOTE_Gb3 = 18500,
    NOTE_G3  = 19600,
    NOTE_Ab3 = 20765,
    NOTE_A3  = 22000,
    NOTE_Bb3 = 23308,
    NOTE_B3  = 24694,
    NOTE_C4  = 26163,
    NOTE_Db4 = 27718,
    NOTE_D4  = 29366,
    NOTE_Eb4 = 31113,
    NOTE_E4  = 32963,
    NOTE_F4  = 34923,
    NOTE_Gb4 = 36999,
    NOTE_G4  = 39200,
    NOTE_Ab4 = 41530,
    NOTE_A4  = 44000,
    NOTE_Bb4 = 46616,
    NOTE_B4  = 49388,
    NOTE_C5  = 52325,
    NOTE_Db5 = 55437,
    NOTE_D5  = 58733,
    NOTE_Eb5 = 62225,
    NOTE_E5  = 65925,
    NOTE_F5  = 69846,
    NOTE_Gb5 = 73999,
    NOTE_G5  = 78399,
    NOTE_Ab5 = 83061,
    NOTE_A5  = 88000,
    NOTE_Bb5 = 93233,
    NOTE_B5  = 98777,
    NOTE_C6  = 104650,
    NOTE_Db6 = 110873,
    NOTE_D6  = 117466,
    NOTE_Eb6 = 124451,
    NOTE_E6  = 131851,
    NOTE_F6  = 139691,
    NOTE_Gb6 = 147998,
    NOTE_G6  = 156798,
    NOTE_Ab6 = 166122,
    NOTE_A6  = 176000,
    NOTE_Bb6 = 186466,
    NOTE_B6  = 197553,
    NOTE_C7  = 209300,
    NOTE_Db7 = 221746,
    NOTE_D7  = 234932,
    NOTE_Eb7 = 248902,
    NOTE_E7  = 263702,
    NOTE_F7  = 279383,
    NOTE_Gb7 = 295996,
    NOTE_G7  = 313596,
    NOTE_Ab7 = 332244,
    NOTE_A7  = 352000,
    NOTE_Bb7 = 372931,
    NOTE_B7  = 395107,
    NOTE_C8  = 418601,
    NOTE_Db8 = 443492,
    NOTE_D8  = 469863,
    NOTE_Eb8 = 497803,
    NOTE_E8  = 527404,
    NOTE_F8  = 558765,
    NOTE_Gb8 = 591991,
    NOTE_G8  = 627193,
    NOTE_Ab8 = 664488,
    NOTE_A8  = 704000,
    NOTE_Bb8 = 745862,
    NOTE_B8  = 790213,

    NOTE_OFF  = -1,
    NOTE_DONE = -2,
} note_t;

typedef struct {
    note_t (*func)(uint32_t time);
} buzzer_seq_cfg_t;

typedef struct {
    void (*func)(uint32_t time);
} led_seq_cfg_t;

static uint32_t _buzzer_frequency = 1000;
static uint8_t _buzzer_volume     = 100;
static uint32_t _buzzer_start_cycle;
static bool _new_buzzer_seq          = false;
static buzzer_seq_t _curr_buzzer_seq = BUZZER_SEQ_POWERUP;
static led_seq_t _curr_led_seq       = LED_SEQ_IDLE;

static led_state_t _led_state = {
    .red   = 0,
    .green = 0,
    .blue  = 0,
    .white = 0,
};

static void _set_frequency(uint16_t frequency_hz) {
    uint32_t sys_freq_hz = clock_get_hz(clk_sys);
    uint32_t count       = sys_freq_hz / frequency_hz / PWM_CLKDIV;
    pwm_set_wrap(BUZZER_SLICE, count);
    pwm_set_chan_level(BUZZER_SLICE, BUZZER_CHAN, count * _buzzer_volume / 100 / 2);
    _buzzer_frequency = frequency_hz;
}

static void _set_note(note_t note) {
    if (note == NOTE_OFF || note == NOTE_DONE) {
        pwm_set_enabled(BUZZER_SLICE, false);
        return;
    }

    pwm_set_enabled(BUZZER_SLICE, true);
    _set_frequency(NOTE_TO_HZ(note));
}

static note_t _buzzer_seq_off(uint32_t time) { return NOTE_OFF; }

static note_t _buzzer_seq_powerup(uint32_t time) {
    switch (time) {
        case 0 ... 7:
            return NOTE_C5;
        case 8 ... 15:
            return NOTE_E5;
        case 16 ... 23:
            return NOTE_G5;
        case 24 ... 31:
            return NOTE_C6;
        default:
            return NOTE_DONE;
    }
}

static note_t _buzzer_seq_charging_start(uint32_t time) {
    switch (time) {
        case 0 ... 4:
            return NOTE_C7;
        case 5 ... 9:
            return NOTE_OFF;
        case 10 ... 14:
            return NOTE_C7;
        default:
            return NOTE_DONE;
    }
}

static note_t _buzzer_seq_charging_stop(uint32_t time) {
    switch (time) {
        case 0 ... 4:
            return NOTE_G6;
        default:
            return NOTE_DONE;
    }
}

static note_t _buzzer_seq_enter_sleep(uint32_t time) {
    switch (time) {
        case 0 ... 9:
            return NOTE_G5;
        case 10 ... 19:
            return NOTE_E5;
        case 20 ... 29:
            return NOTE_C5;
        default:
            return NOTE_DONE;
    }
}

static note_t _buzzer_seq_exit_sleep(uint32_t time) {
    switch (time) {
        case 0 ... 9:
            return NOTE_C5;
        case 10 ... 19:
            return NOTE_E5;
        case 20 ... 29:
            return NOTE_G5;
        default:
            return NOTE_DONE;
    }
}

static buzzer_seq_cfg_t _buzzer_seq_cfgs[BUZZER_SEQ_COUNT] = {
    [BUZZER_SEQ_OFF] =
        {
            .func     = _buzzer_seq_off,
        },
    [BUZZER_SEQ_POWERUP] =
        {
            .func     = _buzzer_seq_powerup,
        },
    [BUZZER_SEQ_CHARGING_START] =
        {
            .func     = _buzzer_seq_charging_start,
        },
    [BUZZER_SEQ_CHARGING_STOP] =
        {
            .func     = _buzzer_seq_charging_stop,
        },
    [BUZZER_SEQ_ENTER_SLEEP] =
        {
            .func     = _buzzer_seq_enter_sleep,
        },
    [BUZZER_SEQ_EXIT_SLEEP] =
        {
            .func     = _buzzer_seq_exit_sleep,
        },
};

static void _led_seq_idle(uint32_t cycle) {
    if (cycle % 10 == 0) {
        TOGGLE(_led_state.green, 0, 200);
    }
    _led_state.red   = 0;
    _led_state.blue  = 0;
    _led_state.white = 0;
}

static void _led_seq_charging(uint32_t cycle) {
    const uint8_t min_brightness = 10;
    const uint8_t max_brightness = 100;
    const uint8_t step_size = 2;
    static uint8_t brightness = min_brightness;
    static bool counting_up = true;

    if (counting_up) {
        brightness += step_size;
        if (brightness >= max_brightness) {
            counting_up = false;
            brightness = max_brightness;
        }
    } else {
        brightness -= step_size;
        if (brightness <= min_brightness) {
            counting_up = true;
            brightness = min_brightness;
        }
    }
    _led_state.white = 0;
    _led_state.blue  = brightness;
    _led_state.green = 0;
}

static led_seq_cfg_t _led_seq_cfgs[LED_SEQ_COUNT] = {
    [LED_SEQ_IDLE] =
        {
            .func = _led_seq_idle,
        },
    [LED_SEQ_CHARGING] =
        {
            .func = _led_seq_charging,
        },
};

void set_buzzer(buzzer_seq_t sequence) {
    _curr_buzzer_seq = sequence;
    _new_buzzer_seq  = true;
}

void set_led(led_seq_t sequence) { _curr_led_seq = sequence; }

uint8_t get_led_brightness(led_component_t component) {
    switch (component) {
        case LED_COMPONENT_RED:
            return _led_state.red;
        case LED_COMPONENT_GREEN:
            return _led_state.green;
        case LED_COMPONENT_BLUE:
            return _led_state.blue;
        case LED_COMPONENT_WHITE:
            return _led_state.white;
        default:
            return 0;
    }
}

static void init(void) {
    gpio_set_function(PIN_NUM_BUZZ_CTRL, GPIO_FUNC_PWM);
    pwm_set_clkdiv(BUZZER_SLICE, PWM_CLKDIV);
}

static void run_10ms(uint32_t cycle) {
    if (_new_buzzer_seq) {
        _new_buzzer_seq     = false;
        _buzzer_start_cycle = cycle;
    }

    note_t note = _buzzer_seq_cfgs[_curr_buzzer_seq].func(cycle - _buzzer_start_cycle);

    if (note == NOTE_DONE) {
        _curr_buzzer_seq    = BUZZER_SEQ_OFF;
        _buzzer_start_cycle = cycle;
    }
    _set_note(note);

    _led_seq_cfgs[_curr_led_seq].func(cycle);
}

module_t indication_module = {
    .name       = "indication",
    .init       = init,
    .run_1ms    = NULL,
    .run_10ms   = run_10ms,
    .run_100ms  = NULL,
    .run_1000ms = NULL,
};
