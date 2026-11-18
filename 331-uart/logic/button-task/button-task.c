#include "button-task/button-task.h"

#include <stddef.h>
#include "systime/systime.h"
#include "platform/platform.h"

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


static button_task_callback_t press_callback;


void button_task_init(button_task_callback_t on_press)
{
 
    button_state = BUTTON_UNPRESSED;
    press_count = 0;
    changed_us = 0;
    press_callback = on_press;
}



void button_task_handle(void)
{
    bool pressed = platform_button_read();
    uint64_t now_us = systime_us();

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
            if (press_callback != NULL)
            {
                press_callback();
            }
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

