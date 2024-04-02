#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    void (*init)(void);
    void (*run_1ms)(uint32_t);
    void (*run_10ms)(uint32_t);
    void (*run_100ms)(uint32_t);
    void (*run_1000ms)(uint32_t);
    char* name;
} module_t;
