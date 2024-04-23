#include "FreeRTOS.h"
#include "adc.h"
#include "dma.h"
#include "generated/values.h"
#include "gps.h"
#include "hp_i2c.h"
#include "hw_config.h"
#include "imu.h"
#include "indication.h"
#include "lp_i2c.h"
#include "motor.h"
#include "pico/time.h"
#include "radio.h"
#include "state.h"
#include "task.h"
#include "usb.h"

static uint32_t cycles_1ms    = 0UL;  // 49 days until overflow. Sleep mode not that good.
static uint32_t cycles_10ms   = 0UL;
static uint32_t cycles_100ms  = 0UL;
static uint32_t cycles_1000ms = 0UL;

static module_t* modules[] = {
    &usb_module, &adc_module, &indication_module, &lp_i2c_module, &imu_module, &gps_module,
    &radio_module,
    &hp_i2c_module, &state_module //&motor_module, 
    // &dma_module
};

static void init(void) {
    for (int i = 0; i < sizeof(modules) / sizeof(module_t*); i++) {
        if (modules[i]->init != NULL) {
            modules[i]->init();
        }
    }
}

static void task_1ms(void*) {
    TickType_t last_wake_time = xTaskGetTickCount();
    while (true) {
        absolute_time_t start = get_absolute_time();
        for (int i = 0; i < sizeof(modules) / sizeof(module_t*); i++) {
            if (modules[i]->run_1ms != NULL) {
                modules[i]->run_1ms(cycles_1ms);
            }
        }
        cycles_1ms++;

        absolute_time_t end  = get_absolute_time();
        values.task_1ms_time = (float)absolute_time_diff_us(start, end) / 1000.0f;

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1U));
    }
}

static void task_10ms(void*) {
    TickType_t last_wake_time = xTaskGetTickCount();
    while (true) {
        absolute_time_t start = get_absolute_time();

        for (int i = 0; i < sizeof(modules) / sizeof(module_t*); i++) {
            if (modules[i]->run_10ms != NULL) {
                modules[i]->run_10ms(cycles_10ms);
            }
        }
        cycles_10ms++;

        absolute_time_t end   = get_absolute_time();
        values.task_10ms_time = (float)absolute_time_diff_us(start, end) / 1000.0f;

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(10U));
    }
}

static void task_100ms(void*) {
    TickType_t last_wake_time = xTaskGetTickCount();
    while (true) {
        absolute_time_t start = get_absolute_time();

        for (int i = 0; i < sizeof(modules) / sizeof(module_t*); i++) {
            if (modules[i]->run_100ms != NULL) {
                modules[i]->run_100ms(cycles_100ms);
            }
        }
        cycles_100ms++;

        absolute_time_t end    = get_absolute_time();
        values.task_100ms_time = (float)absolute_time_diff_us(start, end) / 1000.0f;

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100U));
    }
}

static void task_1000ms(void*) {
    TickType_t last_wake_time = xTaskGetTickCount();
    while (true) {
        absolute_time_t start = get_absolute_time();

        for (int i = 0; i < sizeof(modules) / sizeof(module_t*); i++) {
            if (modules[i]->run_1000ms != NULL) {
                modules[i]->run_1000ms(cycles_1000ms);
            }
        }
        cycles_1000ms++;

        absolute_time_t end     = get_absolute_time();
        values.task_1000ms_time = (float)absolute_time_diff_us(start, end) / 1000.0f;

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000U));
    }
}

int main(void) {
    init();

    xTaskCreate(task_1ms, "1ms-task", 512, NULL, 1, NULL);
    xTaskCreate(task_10ms, "10ms-task", 512, NULL, 2, NULL);
    xTaskCreate(task_100ms, "100ms-task", 512, NULL, 3, NULL);
    xTaskCreate(task_1000ms, "1000ms-task", 512, NULL, 4, NULL);

    vTaskStartScheduler();
}
