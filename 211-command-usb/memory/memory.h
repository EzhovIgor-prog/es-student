#pragma once

#include "pico/stdlib.h"
#include <stdio.h>
#include <stdint.h>


#include "pico/stdlib.h"
#include <string.h>


static void row(const char *name, uintptr_t start, uintptr_t end);
void mem_info(void);
void fw_info(void);

extern uint32_t data_variable;
extern uint32_t bss_variable;


