
#pragma once

#include <stdint.h>

#define DEVICE_NAME "es-led-module"
#define FIRMWARE_VERSION "1.2.0"

#define DEVICE_PROJECT "231_firmware_template"
#define DEVICE_REPO "https://github.com/EzhovIgor-prog/es-student"

#ifndef DEVICE_BOARD
#define DEVICE_BOARD "unknown"
#endif


typedef struct
{
    const char *board;
    char serial[17];
    uint32_t manufacturer;
    uint32_t part;
    uint32_t revision;
    const char *sdk_version;
} device_info_t;

void device_get_info(device_info_t *info);


  