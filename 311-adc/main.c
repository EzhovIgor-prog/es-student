#include <stddef.h>
#include <string.h>
#include "logging/log.h"
#include "led-task/led-task.h"

#include "api/api.h"
#include "stdio-text-protocol/stdio-text-protocol.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"
#include "firmware.h"
#include "platform/platform.h"
#include "telemetry-task/telemetry-task.h"

#if PROFILING_ENABLED
#include "profiling/profiling.h"
#endif


typedef enum
{
    STOPWATCH_LOOP,
    STOPWATCH_COMMAND,
    STOPWATCH_LED,
    STOPWATCH_BUTTON,
    STOPWATCH_PI,
    STOPWATCH_TELEMETRY,
} stopwatch_id_t;



//========================================================================
int main()
{
   
    platform_init();

    button_task_init(PLATFORM_BUTTON_N1, led_task_next_state, led_task_blink);
    button_task_init(PLATFORM_BUTTON_N2, led_task_next_period, NULL);
    telemetry_task_init(api_print_telemetry);
    
    led_task_init();
    stdio_text_protocol_init();

    #if PROFILING_ENABLED
    profiling_stopwatch_init(STOPWATCH_LOOP, "loop");
    profiling_stopwatch_init(STOPWATCH_COMMAND, "command");
    profiling_stopwatch_init(STOPWATCH_LED, "led");
    profiling_stopwatch_init(STOPWATCH_BUTTON, "button");
    profiling_stopwatch_init(STOPWATCH_PI, "pi");
    profiling_stopwatch_init(STOPWATCH_TELEMETRY, "telemetry");
    #endif

    LOG_INF("%s %s\n", FIRMWARE_NAME, FIRMWARE_VERSION);

    
  
   while (1)
    {
        #if PROFILING_ENABLED
        profiling_start(STOPWATCH_LOOP);
        profiling_start(STOPWATCH_BUTTON);
        #endif

       
        button_task_handle();
       
        #if PROFILING_ENABLED
        profiling_stop(STOPWATCH_BUTTON);
        profiling_start(STOPWATCH_COMMAND);
        #endif

        const command_t *command = stdio_text_protocol_handle();
        if (command != NULL)
        {
            api_handle(command);
        }

        #if PROFILING_ENABLED
        profiling_stop(STOPWATCH_COMMAND);
        profiling_start(STOPWATCH_LED);
        #endif

        led_task_handle();

        #if PROFILING_ENABLED
        profiling_stop(STOPWATCH_LED);
        profiling_start(STOPWATCH_PI);
        #endif

        pi_task_handle();

        #if PROFILING_ENABLED
        profiling_stop(STOPWATCH_PI);
        profiling_start(STOPWATCH_TELEMETRY);
        #endif

        telemetry_task_handle();

        #if PROFILING_ENABLED
        profiling_stop(STOPWATCH_TELEMETRY);
        profiling_stop(STOPWATCH_LOOP);
        #endif

    }
}
