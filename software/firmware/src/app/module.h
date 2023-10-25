#pragma once

#include <stddef.h>

typedef struct {
    void (*init)(void);
    void (*run_1ms)(void);
    void (*run_10ms)(void);
    void (*run_100ms)(void);
    void (*run_1000ms)(void);
    char* name;
} module_t;
