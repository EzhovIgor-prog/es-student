#pragma once


#include <stdint.h>
#include "platform/platform.h"

// функция, которую задача вызывает на каждое нажатие
typedef void (*button_task_callback_t)(void);

void button_task_init(platform_button_t id, button_task_callback_t press_callback);

void button_task_handle(void);
bool button_task_is_pressed(platform_button_t id);
uint32_t button_task_get_press_count(platform_button_t id);
