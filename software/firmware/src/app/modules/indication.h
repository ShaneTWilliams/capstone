#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "module.h"

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t white;
} led_state_t;

typedef enum {
    BUZZER_SEQ_OFF = 0,
    BUZZER_SEQ_POWERUP,
    BUZZER_SEQ_CHARGING_START,
    BUZZER_SEQ_CHARGING_STOP,
    BUZZER_SEQ_ENTER_SLEEP,
    BUZZER_SEQ_EXIT_SLEEP,

    BUZZER_SEQ_COUNT,
} buzzer_seq_t;

typedef enum {
    LED_SEQ_CHARGING,
    LED_SEQ_ERROR,
    LED_SEQ_CONNECTED,
    LED_SEQ_IDLE,

    LED_SEQ_COUNT,
} led_seq_t;

typedef enum {
    LED_COMPONENT_RED = 0,
    LED_COMPONENT_GREEN,
    LED_COMPONENT_BLUE,
    LED_COMPONENT_WHITE,

    LED_COMPONENT_COUNT,
} led_component_t;

void set_buzzer(buzzer_seq_t sequence);
void set_led(led_seq_t sequence);
uint8_t get_led_brightness(led_component_t component);

extern module_t indication_module;
