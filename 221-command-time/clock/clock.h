#pragma once

#include <stdint.h>
#include "hardware/clocks.h"
#include "hardware/timer.h"

void clk_info(void);
void uptime(void);
static void clk_sys_set(uint32_t khz);
void clk_sys_low(void);
void clk_sys_default(void);

