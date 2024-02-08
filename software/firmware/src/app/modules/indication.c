#include "indication.h"

#include "generated/values.h"
#include "globals.h"
#include "hardware/pwm.h"
#include "hw_config.h"
#include "lp_i2c.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"

#define PWM_CLKDIV (8.0f)
#define NOTE_TO_HZ(note) ((float)note / 100.0f)

typedef enum {
    NOTE_C0 = 1635,
    NOTE_Db0 = 1732,
    NOTE_D0 = 1835,
    NOTE_Eb0 = 1945,
    NOTE_E0 = 2060,
    NOTE_F0 = 2183,
    NOTE_Gb0 = 2312,
    NOTE_G0 = 2450,
    NOTE_Ab0 = 2596,
    NOTE_A0 = 2750,
    NOTE_Bb0 = 2914,
    NOTE_B0 = 3087,
    NOTE_C1 = 3270,
    NOTE_Db1 = 3465,
    NOTE_D1 = 3671,
    NOTE_Eb1 = 3889,
    NOTE_E1 = 4120,
    NOTE_F1 = 4365,
    NOTE_Gb1 = 4625,
    NOTE_G1 = 4900,
    NOTE_Ab1 = 5191,
    NOTE_A1 = 5500,
    NOTE_Bb1 = 5827,
    NOTE_B1 = 6174,
    NOTE_C2 = 6541,
    NOTE_Db2 = 6930,
    NOTE_D2 = 7342,
    NOTE_Eb2 = 7778,
    NOTE_E2 = 8241,
    NOTE_F2 = 8731,
    NOTE_Gb2 = 9250,
    NOTE_G2 = 9800,
    NOTE_Ab2 = 10383,
    NOTE_A2 = 11000,
    NOTE_Bb2 = 11654,
    NOTE_B2 = 12347,
    NOTE_C3 = 13081,
    NOTE_Db3 = 13859,
    NOTE_D3 = 14683,
    NOTE_Eb3 = 15556,
    NOTE_E3 = 16481,
    NOTE_F3 = 17461,
    NOTE_Gb3 = 18500,
    NOTE_G3 = 19600,
    NOTE_Ab3 = 20765,
    NOTE_A3 = 22000,
    NOTE_Bb3 = 23308,
    NOTE_B3 = 24694,
    NOTE_C4 = 26163,
    NOTE_Db4 = 27718,
    NOTE_D4 = 29366,
    NOTE_Eb4 = 31113,
    NOTE_E4 = 32963,
    NOTE_F4 = 34923,
    NOTE_Gb4 = 36999,
    NOTE_G4 = 39200,
    NOTE_Ab4 = 41530,
    NOTE_A4 = 44000,
    NOTE_Bb4 = 46616,
    NOTE_B4 = 49388,
    NOTE_C5 = 52325,
    NOTE_Db5 = 55437,
    NOTE_D5 = 58733,
    NOTE_Eb5 = 62225,
    NOTE_E5 = 65925,
    NOTE_F5 = 69846,
    NOTE_Gb5 = 73999,
    NOTE_G5 = 78399,
    NOTE_Ab5 = 83061,
    NOTE_A5 = 88000,
    NOTE_Bb5 = 93233,
    NOTE_B5 = 98777,
    NOTE_C6 = 104650,
    NOTE_Db6 = 110873,
    NOTE_D6 = 117466,
    NOTE_Eb6 = 124451,
    NOTE_E6 = 131851,
    NOTE_F6 = 139691,
    NOTE_Gb6 = 147998,
    NOTE_G6 = 156798,
    NOTE_Ab6 = 166122,
    NOTE_A6 = 176000,
    NOTE_Bb6 = 186466,
    NOTE_B6 = 197553,
    NOTE_C7 = 209300,
    NOTE_Db7 = 221746,
    NOTE_D7 = 234932,
    NOTE_Eb7 = 248902,
    NOTE_E7 = 263702,
    NOTE_F7 = 279383,
    NOTE_Gb7 = 295996,
    NOTE_G7 = 313596,
    NOTE_Ab7 = 332244,
    NOTE_A7 = 352000,
    NOTE_Bb7 = 372931,
    NOTE_B7 = 395107,
    NOTE_C8 = 418601,
    NOTE_Db8 = 443492,
    NOTE_D8 = 469863,
    NOTE_Eb8 = 497803,
    NOTE_E8 = 527404,
    NOTE_F8 = 558765,
    NOTE_Gb8 = 591991,
    NOTE_G8 = 627193,
    NOTE_Ab8 = 664488,
    NOTE_A8 = 704000,
    NOTE_Bb8 = 745862,
    NOTE_B8 = 790213,
} note_t;


static uint8_t slice_num;
static uint8_t chan_num;
static uint32_t buzzer_frequency = 1000;
static uint8_t buzzer_volume = 100;
static bool led_state;

bool get_led_state(void) { return led_state; }

void set_frequency(uint16_t frequency_hz) {
    buzzer_frequency = frequency_hz;
    uint32_t sys_freq_hz = clock_get_hz(clk_sys);
    uint32_t count = sys_freq_hz / buzzer_frequency / PWM_CLKDIV;
    pwm_set_wrap(slice_num, count);
    pwm_set_chan_level(slice_num, chan_num, count * buzzer_volume / 100 / 2);
}

void set_note(note_t note) {
    set_frequency(NOTE_TO_HZ(note));
}

void set_volume(uint8_t volume) {
    buzzer_volume = volume;
}

static void init(void) {
    gpio_set_function(PIN_NUM_BUZZ_CTRL, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(PIN_NUM_BUZZ_CTRL);
    chan_num  = pwm_gpio_to_channel(PIN_NUM_BUZZ_CTRL);

    pwm_set_clkdiv(slice_num, PWM_CLKDIV);
    pwm_set_enabled(slice_num, true);
    set_volume(1);
}

static void run_10ms(void) {
    static uint8_t startup_chime_count = 0;
    if (cycles_10ms % 10 == 0) {
        led_state = !led_state;
        startup_chime_count++;
    }

    switch (startup_chime_count) {
        case 1:
            set_note(NOTE_C5);
            break;
        case 2:
            set_note(NOTE_E5);
            break;
        case 3:
            set_note(NOTE_G5);
            break;
        case 4:
            set_note(NOTE_C6);
            break;
        case 5:
            set_note(NOTE_G6);
            break;
        case 6:
            set_note(NOTE_E6);
            break;
        case 7:
            set_note(NOTE_C7);
            break;
        default:
            pwm_set_enabled(slice_num, false);
            break;
    }
}

module_t indication_module = {
    .name       = "indication",
    .init       = init,
    .run_1ms    = NULL,
    .run_10ms   = run_10ms,
    .run_100ms  = NULL,
    .run_1000ms = NULL,
};
