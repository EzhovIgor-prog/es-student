#include "api/api-commands.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "device/device.h"
#include "profiling/profiling.h"
#include "led-task/led-task.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"







// Число без знака из десятичной записи. false — в записи не только цифры
// или число не помещается в uint32_t.
static bool parse_u32(const char *text, uint32_t *value)
{
    // strtoul приняла бы и знак: из "-1" получилось бы 4294967295
    if (!isdigit((unsigned char)text[0]))
    {
        return false;
    }

    char *end;
    errno = 0;
    unsigned long number = strtoul(text, &end, 10);

    // после числа остались символы или число не поместилось
    if (*end != '\0' || errno == ERANGE || number > UINT32_MAX)
    {
        return false;
    }

    *value = (uint32_t)number;
    return true;
}





void cmd_info(const command_t *command)
{
    // печатаем паспорт устройства
    device_info();
}


void cmd_uptime(const command_t *command)
{
   printf("uptime: %llu ms\n", time_us_64() / 1000);
}



static void cmd_pi_start(const command_t *command)
{
    uint32_t terms = 1000000;
    if (command->argc > 1 ||
        (command->argc == 1 && !parse_u32(command->argv[0], &terms)))
    {
        printf("error: usage pi_start [<terms>]\r\n");
        return;
    }
    if (terms == 0)
    {
        printf("error: terms must be greater than 0\r\n");
        return;
    }
    if (!pi_task_start(terms))
    {
        printf("error: pi is busy\r\n");
        return;
    }
    printf("pi: started, %u terms\r\n", (unsigned)terms);
}

static void cmd_pi(const command_t *command)
{
    
    double result;
    uint64_t elapsed_ms;
    
    if (pi_task_get_result(&result, &elapsed_ms))
    {
        printf("pi: %.9f in %llu ms\r\n", result, (unsigned long long)elapsed_ms);
    }
    else if (pi_task_get_state() == PI_RUNNING)
    {
        printf("pi: running\r\n");
    }
    else
    {
        printf("pi: not started\r\n");
    }
}




void cmd_time_exec(const command_t *command)
{
   time_exec();
}

void cmd_time_reset(const command_t *command)
{
   time_reset();
}


static void prnledState(void)
{
    const char *stateLed = LED_STATE_OFF;
    switch (led_task_get_state())
    {
    case LED_STATE_OFF: 
        stateLed = "off";
        break;

    case LED_STATE_ON: 
        stateLed = "on"; 
        break;

    case LED_STATE_BLINK: 
        stateLed = "blink"; 
        break;
        
    }
    printf("led: %s, period %u ms\r\n", stateLed, (unsigned)led_task_get_period_ms());
}


static void cmd_led_enable(const command_t *command)
{
    //(void)command;
    led_task_set_state(LED_STATE_ON);
    prnledState();
}

static void cmd_led_disable(const command_t *command)
{
    //(void)command;
    led_task_set_state(LED_STATE_OFF);
    prnledState();
}

static void cmd_led_blink(const command_t *command)
{
    //(void)command;
    led_task_set_state(LED_STATE_BLINK);
    prnledState();
}

static void cmd_led_period(const command_t *command)
{
    uint32_t period_ms;
    if (command->argc != 1 || !parse_u32(command->argv[0], &period_ms))
    {
        printf("error: usage led_period <period_ms>\r\n");
        return;
    }
    if (!led_task_set_period_ms(period_ms))
    {
        printf("error: period_ms must be greater than 0\r\n");
        return;
    }
    prnledState();
}


static void cmd_button(const command_t *command)
{
    (void)command;
    printf("button: %s, presses %u\r\n",
           button_task_is_pressed() ? "pressed" : "released",
           (unsigned)button_task_get_press_count());
}


const api_command_t api_commands[] = {
    {"info", "device passport", cmd_info},
    {"uptime", "time since reset", cmd_uptime}, 
    {"pi_start", "start calculation: pi_start [<terms>]", cmd_pi_start},
    {"pi", "calculation state or result", cmd_pi},
    {"main_time_exec", "superloop iteration: average and maximum", cmd_time_exec},
    {"main_time_reset", "reset maximum iteration", cmd_time_reset}, 
    {"led_enable", "LED on", cmd_led_enable},
    {"led_disable", "LED off", cmd_led_disable},
    {"led_blink", "LED blinking", cmd_led_blink},
    {"led_period", "set period: led_period <period_ms>", cmd_led_period},
    {"button", "button state and press count", cmd_button},
};

const uint32_t api_command_count = sizeof(api_commands) / sizeof(api_commands[0]);









