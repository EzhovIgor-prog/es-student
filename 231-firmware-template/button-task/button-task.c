#include "button-task/button-task.h"

#include <stddef.h>
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "led-task/led-task.h"

static const uint BUTTON_PIN = 15;

typedef enum
{
    BUTTON_UNPRESSED,
    BUTTON_PRESS_PENDING,
    BUTTON_PRESSED,
    BUTTON_UNPRESS_PENDING,
} button_state_t;

static const uint64_t DEBOUNCE_US = 20000;
static button_state_t button_state;
static uint64_t changed_us;
static uint32_t press_count;

void button_task_init(void)
{
    // инициализируем пин кнопки и настраиваем его на вход с PULLUP
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);


    button_state = BUTTON_UNPRESSED;
    press_count = 0;
    changed_us = 0;
}


bool button_read(void)
{
    return !gpio_get(BUTTON_PIN);
}


void button_task_handle(void)
{
    bool pressed = button_read();
    uint64_t now_us = time_us_64();

    switch (button_state)
    {
    case BUTTON_UNPRESSED:
        if (pressed)
        {
            changed_us = now_us;
            button_state = BUTTON_PRESS_PENDING;
        }
        break;
    case BUTTON_PRESS_PENDING:
        if (!pressed)
        {
            button_state = BUTTON_UNPRESSED;
        }
        else if (now_us - changed_us >= DEBOUNCE_US)
        {
            button_state = BUTTON_PRESSED;
            press_count++;

            led_task_next_state();
         }
        break;
    case BUTTON_PRESSED:
        if (!pressed)
        {
            changed_us = now_us;
            button_state = BUTTON_UNPRESS_PENDING;
        }
        break;
    case BUTTON_UNPRESS_PENDING:
        if (pressed)
        {
            button_state = BUTTON_PRESSED;
        }
        else if (now_us - changed_us >= DEBOUNCE_US)
        {
            button_state = BUTTON_UNPRESSED;
        }
        break;
    }
}

bool button_task_is_pressed(void)
{
    if(button_state == BUTTON_PRESSED || button_state == BUTTON_UNPRESS_PENDING) return true;

    return false;
}

uint32_t button_task_get_press_count(void)
{
    return press_count;
}

