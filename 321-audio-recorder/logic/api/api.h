#pragma once

#include "stdio-text-protocol/stdio-text-protocol.h"
#include "telemetry-task/telemetry-task.h"

void api_handle(const command_t *command);
void api_print_telemetry(const telemetry_record_t *record);
//void print_dma_cap(const uint16_t *samples, uint32_t count, uint32_t time_us);
