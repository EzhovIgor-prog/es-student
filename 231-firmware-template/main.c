#include <stdio.h>
#include "pico/stdlib.h"
#include "led-task/led-task.h"
#include "log.h"
#include "device.h"
#include <string.h>
#include "profiling.h"
#include "api/api.h"
#include "stdio-text-protocol/stdio-text-protocol.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"






//========================================================================
int main()
{
   
    stdio_init_all();
    button_task_init();
    led_task_init();

    stdio_text_protocol_init();
    
  
   while (1)
    {
        button_task_handle();

        const command_t *command = stdio_text_protocol_handle();
        if (command != NULL)
        {
            api_handle(command);
        }

         led_task_handle();
         pi_task_handle();  
    }
}