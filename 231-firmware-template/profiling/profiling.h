#pragma once

#include "pico/types.h"

#include <stdint.h>
#include "hardware/clocks.h"
#include "hardware/timer.h"




typedef struct
{
    const char *name;
    uint32_t count;
    double mean_us;
    uint32_t max_us;
} profiling_result_t;

#define PROFILING_STOPWATCH_COUNT 5


// Сумма хранится в uint32_t: переполнение после 4294967295 мкс 
// накопленного времени (около 71 минуты) До этого нужно сбросить результаты.
void profiling_stopwatch_init(uint32_t id, const char *name);
void profiling_start(uint32_t id);
void profiling_stop(uint32_t id);
bool profiling_get(uint32_t id, profiling_result_t *result);
void profiling_reset(void);

