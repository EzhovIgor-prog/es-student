#include "api/api-commands.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "device/device.h"
#include "profiling/profiling.h"
#include "led-task/led-task.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"
#include "systime/systime.h"
#include "firmware.h"
#include "platform/platform.h"
#include "telemetry-task/telemetry-task.h"
#include "block-task/block-task.h"
#include "stream-task/stream-task.h"


#define TELEMETRY_MIN_PERIOD_MS 10u
#define STREAM_BLINK_PERIOD_MS 250  // период индикатора записи во время потока






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



static void cmd_uptime(const command_t *command)
{
   printf("uptime: %llu ms\n", systime_ms());
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
    (void)command;
    led_task_set_state(LED_STATE_ON);
    prnledState();
}

static void cmd_led_disable(const command_t *command)
{
    (void)command;
    led_task_set_state(LED_STATE_OFF);
    prnledState();
}

static void cmd_led_blink(const command_t *command)
{
    (void)command;
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
    for (platform_button_t id = 0; id < PLATFORM_BUTTON_QTY; id++)
    {
        printf("button %u: %s, presses %u\r\n", 
                (unsigned)id + 1,
                button_task_is_pressed(id) ? "pressed" : "released",
                (unsigned)button_task_get_press_count(id));
    }
}

static void cmd_info(const command_t *command)
{
    (void)command;
    device_info_t info;
    device_get_info(&info);
    printf("name: %s\r\n", FIRMWARE_NAME);
    printf("version: %s\r\n", FIRMWARE_VERSION);
    printf("project: %s\r\n", FIRMWARE_PROJECT);
    printf("repo: %s\r\n", FIRMWARE_REPO);
    printf("board: %s\r\n", info.board);
    printf("serial: %s\r\n", info.serial);
    printf("chip: manufacturer 0x%03x, part 0x%04x, revision %u\r\n",
           (unsigned)info.manufacturer, (unsigned)info.part, (unsigned)info.revision);
    printf("pico-sdk: %s\r\n", info.sdk_version);
}


static void cmd_profiling(const command_t *command)
{
    (void)command;
#if PROFILING_ENABLED
    for (uint32_t id = 0; id < PROFILING_STOPWATCH_COUNT; id++)
    {
        profiling_result_t result;
        if (profiling_get(id, &result))
        {
            printf("%-10s count %-10u mean %.2f us  max %u us\r\n",
                   result.name, (unsigned)result.count, result.mean_us,
                   (unsigned)result.max_us);
        }
    }
#else
    printf("profiling is disabled in this build\r\n");
#endif
}

static void cmd_profiling_reset(const command_t *command)
{
    (void)command;
    #if PROFILING_ENABLED
    profiling_reset();
    printf("profiling reset\r\n");
    #else
    printf("profiling is disabled in this build\r\n");
    #endif
}

static void cmd_get_voltage(const command_t *command)
{
    (void)command;

    if(platform_adc_busy() == true)
    {
        printf("error: adc busy\r\n");
        return;
    }

    printf("voltage: %.3f V\r\n", platform_voltage_read());
}

static void cmd_get_temp(const command_t *command)
{
    (void)command;

    if(platform_adc_busy() == true)
    {
        printf("error: adc busy\r\n");
        return;
    }

    printf("temp: %.1f C\r\n", platform_temp_read());
}



static void cmd_tm_start(const command_t *command)
{
    if(platform_adc_busy() == true)
    {
        printf("error: adc busy\r\n");
        return;
    }

    uint32_t period_ms = TELEMETRY_DEFAULT_PERIOD_MS;
    if (command->argc > 1 ||
        (command->argc == 1 && !parse_u32(command->argv[0], &period_ms)))
    {
        printf("error: usage tm_start [<period_ms>]\r\n");
        return;
    }
    if (period_ms < TELEMETRY_MIN_PERIOD_MS)
    {
        printf("error: period_ms must be at least %lu\r\n",
               (unsigned long)TELEMETRY_MIN_PERIOD_MS);
        return;
    }
    if (!telemetry_task_start(period_ms))
    {
        printf("error: failed to start telemetry\r\n");
        return;
    }
    printf("telemetry: started, period %lu ms\r\n", (unsigned long)period_ms);
}

static void cmd_tm_stop(const command_t *command)
{
    (void)command;
    telemetry_task_stop();
    printf("telemetry: stopped\r\n");
}

static void cmd_charge_on(const command_t *command)
{
    (void)command;
    platform_charge_set(true);
    printf("charge: on\r\n");
}

static void cmd_charge_off(const command_t *command)
{
    (void)command;
    platform_charge_set(false);
    printf("charge: off\r\n");
}

void api_print_telemetry(const telemetry_record_t *record)
{
    
    if (record != NULL)
    {
        printf("tm: %" PRIu64 " %.3f %.1f\r\n",
               record->time_us,
               (double)record->voltage_v,
               (double)record->temp_c);
    }
}


static void cmd_timer_blink_start(const command_t *command)
{
    uint32_t period_ms = 100;



    if (command->argc < 1  || command->argc > 1 || (command->argc == 1 && !parse_u32(command->argv[0], &period_ms)))
    {
        printf("error: usage timer_blink_start <period_ms>\r\n");
        return;
    }


    if(platform_blink_start(period_ms) == false)
    {
        printf("error: period_ms must be 10..60000 \r\n");
        return;
    }
    
    led_task_set_state(LED_STATE_OFF);

    printf("timer_blink: started, period %lu ms\r\n", (unsigned long)period_ms);

}

static void cmd_timer_blink_stop(const command_t *command)
{
    (void)command;
   
    printf("timer_blink: stopped, %lu toggles\r\n", (unsigned long)platform_blink_stop());
    
}





// cmd_cap_start(): аргументов не два или не числа — error: usage cap_start <N> <period_us>
// N = 0, N > PLATFORM_CAPTURE_MAX или период меньше 10 мкс —
//     error: N must be 1..2000, period_us at least 10
// идёт телеметрия или platform не начал захват — error: adc busy
// ответ capture: started, <N> samples, period <период> us

// cmd_cap_dump(): захвата не было — error: no capture
// строка capture: <N> samples, period <период> us, interval <мин>..<макс> us, handler <доля> %
// коды строками cap: по 16

// get_voltage, get_temp, tm_start: АЦП занят — error: adc busy

static void cmd_cap_start(const command_t *command)
{
    if(telemetry_task_get_state() == TELEMETRY_STATE_RUN)
    {
        printf("error: adc busy\r\n");
        return;
    }

    uint32_t capt_count = 0;
    uint32_t capt_period_us = 0;

     if (command->argc != 2  || (command->argc == 2 && !parse_u32(command->argv[0], &capt_count))|| (command->argc == 2 && !parse_u32(command->argv[1], &capt_period_us)))
    //if (command->argc != 2  || (parse_u32(command->argv[0], &capt_count) == false)|| (parse_u32(command->argv[1], &capt_period_us) == false))
    
    {
        printf("error: usage cap_start <N> <period_us>\r\n");
        return;
    }

     //printf("capture: started, %lu samples, period %lu us\r\n", (unsigned long)capt_count, (unsigned long)capt_period_us);


    if(platform_capture_start(capt_count, capt_period_us) == false)
    {
        printf("error: N must be 1..2000, period_us at least 10\r\n");
        return;
    }

    printf("capture: started, %lu samples, period %lu us\r\n", (unsigned long)capt_count, (unsigned long)capt_period_us);
    
}


static void cmd_cap_dump(const command_t *command)
{

    platform_capture_t capture_data;
    platform_capture_get(&capture_data);


    if(platform_capture_done() == false)
    {
        printf("error: no capture\r\n");
        return;
    }

    printf("capture: %lu samples, period %lu us, interval %lu..%lu us, handler %3.1f %%\r\n", 
                                    (unsigned long)capture_data.count, 
                                    (unsigned long)capture_data.period_us,
                                    (unsigned long)capture_data.min_interval_us,
                                    (unsigned long)capture_data.max_interval_us,
                                    (float)capture_data.handler_percent);
   
        
        printf("cap: ");

        uint8_t j=0;

        for(uint16_t i=0; i< capture_data.count; i++)
        {
            printf("%lu ", (unsigned long)(*(capture_data.samples + i)));    

            if(++j >= 16)
            {
                j = 0;
                printf(" ");
            }
        }
        printf("\r\n");

}

void print_dma_cap(const uint16_t *samples, uint32_t count, uint32_t time_us)
{
    const uint16_t *pData = samples;
    uint16_t data_min = *pData;
    uint16_t data_max = *pData;
    uint32_t  data_summ = 0;  
    uint64_t time_expected = 0;

    for(uint16_t i = 0; i< count; i++)
    {
        data_summ += *pData;
        if(*pData < data_min)  data_min = *pData;
        if(*pData > data_max)  data_max = *pData;

        pData++;
    }

    float data_mean = (float) data_summ / (float) count;

    // расчёт времени — count * 1000000 / PLATFORM_SAMPLE_RATE_HZ

    time_expected = (count * 500000);
    time_expected += (count * 500000);
    time_expected =  time_expected / PLATFORM_SAMPLE_RATE_HZ;


    printf("dma_cap: %lu samples in %lu us, expected %lu us, min %lu max %lu mean %5.1f\r\n", 
                                    (unsigned long) count,
                                    (unsigned long) time_us,
                                    (unsigned long) time_expected,
                                    (unsigned long) data_min,
                                    (unsigned long) data_max,
                                    (float) data_mean);

 
    
}


static void cmd_dma_cap(const command_t *command)
{
    
    uint32_t dma_capt_count = 0;

    if (command->argc != 1 || (command->argc == 1 && !parse_u32(command->argv[0], &dma_capt_count)))
    {
        printf("error: usage dma_cap <N>\r\n");
        return;
    }

    if((dma_capt_count < 1) || (dma_capt_count > PLATFORM_BLOCK_MAX))
    {

        printf("error: N must be 1..8192\r\n");
        return;

    } 
    
    if((telemetry_task_get_state() == TELEMETRY_STATE_RUN) || (!block_task_start(dma_capt_count,  print_dma_cap)))
    {
        printf("error: adc busy\r\n");
        return;
    }


}


static bool input_reserved(void)
{
    return platform_adc_busy() || (telemetry_task_get_state() == TELEMETRY_STATE_RUN);
}

static void print_busy(void) { printf("error: adc busy\r\n"); }


void api_send_frame(const stream_header_t *header, const uint8_t *data, uint32_t bytes)
{
    // байты заголовка и отсчётов как они лежат в памяти
    platform_write(header, sizeof(*header));
    platform_write(data, bytes);
}



static void cmd_stream_start(const command_t *command)
{
    uint32_t bits = 16;
    if (command->argc > 1 ||
        (command->argc == 1 && !parse_u32(command->argv[0], &bits)) ||
        (bits != 8 && bits != 16))
    {
        printf("error: usage stream_start [8|16]\r\n");
        return;
    }
    if (input_reserved() || !stream_task_start((uint8_t)bits)) { print_busy(); return; }
    if (!platform_blink_start(STREAM_BLINK_PERIOD_MS))
    {
        stream_task_stop();
        printf("error: timer unavailable\r\n");
        return;
    }
    led_task_set_state(LED_STATE_OFF);
    printf("stream: started, 44100 Hz, %u bit\r\n", (unsigned)bits);
}

static void cmd_stream_stop(const command_t *command)
{
    (void)command;
    if (stream_task_get_state() == STREAM_STATE_RUN)
    {
        stream_task_stop();
        platform_blink_stop();
    }
    printf("stream: stopped, %u frames, %u samples, %u overruns\r\n",
           (unsigned)stream_task_get_frames(), (unsigned)stream_task_get_samples(),
           (unsigned)stream_task_get_overruns());
}




const api_command_t api_commands[] = {
    {"info",                "device passport", cmd_info},
    {"uptime",              "time since reset", cmd_uptime}, 
    {"pi_start",            "start calculation: pi_start [<terms>]", cmd_pi_start},
    {"pi",                  "calculation state or result", cmd_pi},
    {"profiling",           "stopwatch results", cmd_profiling},
    {"profiling_reset",     "reset stopwatches", cmd_profiling_reset},
    {"led_enable",          "LED on", cmd_led_enable},
    {"led_disable",         "LED off", cmd_led_disable},
    {"led_blink",           "LED blinking", cmd_led_blink},
    {"led_period",          "set period: led_period <period_ms>", cmd_led_period},
    {"button",              "button state and press count", cmd_button},
    {"get_voltage",         "voltage on GP26 (ADC input 0)", cmd_get_voltage},
    {"get_temp",            "chip temperature in Celsius", cmd_get_temp},
    {"tm_start",            "start telemetry", cmd_tm_start},
    {"tm_stop",             "stop telemetry", cmd_tm_stop},
    {"charge_on",           "charge RC circuit (GP22)", cmd_charge_on},
    {"charge_off",          "discharge RC circuit (GP22)", cmd_charge_off},
    {"timer_blink_start",   "blink led by timer: timer_blink_start <period_ms>", cmd_timer_blink_start},
    {"timer_blink_stop",    "stop timer blink, number of toggles", cmd_timer_blink_stop},
    {"cap_start",           "capture start <N>, <period_us>", cmd_cap_start},
    {"cap_dump",            "capture resultate", cmd_cap_dump},
    {"dma_cap",             "dma capture start <N>", cmd_dma_cap},
    {"stream_start",        "start stream: stream_start", cmd_stream_start},
    {"stream_stop",         "stop stream, report counters", cmd_stream_stop},
    
};

const uint32_t api_command_count = sizeof(api_commands) / sizeof(api_commands[0]);









