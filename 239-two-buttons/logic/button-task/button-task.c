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

typedef struct
{
    button_state_t button_state;
    uint64_t changed_us;
    uint32_t press_count;
    button_task_callback_t press_callback;
} button_t;

static button_t buttons[PLATFORM_BUTTON_QTY];


static const uint64_t DEBOUNCE_US = 20000;


void button_task_init(platform_button_t id, button_task_callback_t press_callback)
{
    if ((unsigned)id < PLATFORM_BUTTON_QTY)
    {
        buttons[id] = (button_t){.button_state = BUTTON_UNPRESSED, 
                                .changed_us = 0,
                                .press_count = 0,                                      
                                .press_callback = press_callback};
    }
}



static void button_handle(platform_button_t id)
{
    bool pressed = platform_button_read(id);
    uint64_t now_us = systime_us();

    switch (buttons[id].button_state)
    {
    case BUTTON_UNPRESSED:
        if (pressed)
        {
            buttons[id].changed_us = now_us;
            buttons[id].button_state = BUTTON_PRESS_PENDING;
        }
        break;
    case BUTTON_PRESS_PENDING:
        if (!pressed)
        {
            buttons[id].button_state = BUTTON_UNPRESSED;
        }
        else if (now_us - buttons[id].changed_us >= DEBOUNCE_US)
        {
            buttons[id].button_state = BUTTON_PRESSED;
            buttons[id].press_count++;
            if (buttons[id].press_callback != NULL)
            {
                buttons[id].press_callback();
            }
         }
        break;
    case BUTTON_PRESSED:
        if (!pressed)
        {
            buttons[id].changed_us = now_us;
            buttons[id].button_state = BUTTON_UNPRESS_PENDING;
        }
        break;
    case BUTTON_UNPRESS_PENDING:
        if (pressed)
        {
            buttons[id].button_state = BUTTON_PRESSED;
        }
        else if (now_us - buttons[id].changed_us >= DEBOUNCE_US)
        {
            buttons[id].button_state = BUTTON_UNPRESSED;
        }
        break;
    }
}

void button_task_handle(void)
{
    for (platform_button_t j = 0; j < PLATFORM_BUTTON_QTY; j++)
    {
        button_handle(j);
    }
}


bool button_task_is_pressed(platform_button_t id)
{
    if(((unsigned)id < PLATFORM_BUTTON_QTY) &&
    ((buttons[id].button_state == BUTTON_PRESSED) ||
    (buttons[id].button_state == BUTTON_UNPRESS_PENDING))) return true;

    return false;
}

uint32_t button_task_get_press_count(platform_button_t id)
{
    return buttons[id].press_count;
}

