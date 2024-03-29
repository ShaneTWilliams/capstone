#include <FreeRTOS.h>
#include <pico/time.h>
#include <task.h>

#include "adc.h"
#include "dma.h"
#include "globals.h"
#include "gps.h"
#include "hw_config.h"
#include "imu.h"
#include "indication.h"
#include "lp_i2c.h"
#include "hp_i2c.h"
#include "qi.h"
#include "radio.h"
#include "usb.h"
#include "motor.h"

static module_t* modules[] = {
    &usb_module, &adc_module, &indication_module, &lp_i2c_module,
    &imu_module, &gps_module, &qi_module,
    &radio_module,
    &hp_i2c_module,
    &motor_module,
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
        for (int i = 0; i < sizeof(modules) / sizeof(module_t*); i++) {
            if (modules[i]->run_1ms != NULL) {
                modules[i]->run_1ms();
            }
        }
        cycles_1ms++;
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1U));
    }
}

static void task_10ms(void*) {
    TickType_t last_wake_time = xTaskGetTickCount();
    while (true) {
        for (int i = 0; i < sizeof(modules) / sizeof(module_t*); i++) {
            if (modules[i]->run_10ms != NULL) {
                modules[i]->run_10ms();
            }
        }
        cycles_10ms++;
        vTaskDelayUntil(&last_wake_time, 10U);
    }
}

static void task_100ms(void*) {
    TickType_t last_wake_time = xTaskGetTickCount();
    while (true) {
        for (int i = 0; i < sizeof(modules) / sizeof(module_t*); i++) {
            if (modules[i]->run_100ms != NULL) {
                modules[i]->run_100ms();
            }
        }
        cycles_100ms++;
        vTaskDelayUntil(&last_wake_time, 100U);
    }
}

static void task_1000ms(void*) {
    TickType_t last_wake_time = xTaskGetTickCount();
    while (true) {
        for (int i = 0; i < sizeof(modules) / sizeof(module_t*); i++) {
            if (modules[i]->run_1000ms != NULL) {
                modules[i]->run_1000ms();
            }
        }
        cycles_1000ms++;
        vTaskDelayUntil(&last_wake_time, 1000U);
    }
}

int main(void) {
    hw_init();
    init();

    xTaskCreate(task_1ms, "1ms-task", 512, NULL, 1, NULL);
    xTaskCreate(task_10ms, "10ms-task", 512, NULL, 2, NULL);
    xTaskCreate(task_100ms, "100ms-task", 512, NULL, 3, NULL);
    xTaskCreate(task_1000ms, "1000ms-task", 512, NULL, 4, NULL);

    vTaskStartScheduler();
}
