#pragma once

#include <stdint.h>


typedef enum
{
    PLATFORM_BUTTON_N1,
    PLATFORM_BUTTON_N2,
    PLATFORM_BUTTON_QTY
} platform_button_t;

// запускает стандартный ввод-вывод и настраивает выводы
void platform_init(void);

void platform_led_set(bool on);

// нажата ли кнопка сейчас, без подавления дребезга
bool platform_button_read(platform_button_t id);