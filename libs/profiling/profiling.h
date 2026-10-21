#pragma once

#include "pico/types.h"

#include <stdint.h>
#include "hardware/clocks.h"
#include "hardware/timer.h"


void profiling_init(void);
void profiling_iteration(void);
float profiling_avg_us(void);
uint32_t profiling_max_us(void);
void profiling_reset_max(void);

void  time_reset(void);
void  time_exec(void);