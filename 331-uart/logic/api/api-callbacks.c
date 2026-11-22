#include "api/api-commands.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "device/device.h"
#include "profiling/profiling.h"
#include "led-task/led-task.h"
#include "button-task/button-task.h"
#include "pi-task/pi-task.h"
#include "systime/systime.h"
#include "firmware.h"
#include "block-task/block-task.h"
#include "platform/platform.h"
#include "edge-task/edge-task.h"
#include "uart-task/uart-task.h"

 // строка команды text: ровно 32 символа, столько же мест в очереди приёма
static const char text_line[] = "0123456789ABCDEFGHIJKLMNOPQRSTUV";


#define SCOPE_SAMPLES PLATFORM_BLOCK_MAX
#define SCOPE_BAUD_MIN 9600
#define SCOPE_TEXT_MAX 4
#define SCOPE_PRETRIGGER_US 24u



// блок команды block — 64 байта, вдвое длиннее очереди приёма
#define BLOCK_SIZE 64
static uint8_t blockTrBuf[BLOCK_SIZE];

static uint8_t string_len = 0; 

static uint8_t* p_blockTrBuf;

static uint8_t blockIrqmode = 0;


volatile uint32_t scope_received_errors;
static platform_parity_t configured_parities[2];
static bool internal_loop;

static struct {
    bool active, tx_started, rx_done;
    uint32_t baud, errors;
    uint64_t trigger_us;
    uint8_t text[SCOPE_TEXT_MAX], received[SCOPE_TEXT_MAX];
    size_t text_size, received_size;
    const uint16_t *samples;
    uint32_t sample_count;
} scope;


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
    printf("button: %s, presses %u\r\n",
           button_task_is_pressed() ? "pressed" : "released",
           (unsigned)button_task_get_press_count());
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


void print_edge(platform_line_mode_t mode, bool ok, uint32_t rise_us, uint32_t fall_us)
{
    
    float frise_us = (float)rise_us;
    float ffall_us = (float)fall_us;

    if(ok == false)
    {
      printf("error: edge %s timeout\r\n", (mode == PLATFORM_LINE_PUSH_PULL) ? "pp":"od");
    }

    printf("edge %s: rise %.1f ms, fall %.1f ms\r\n", (mode == PLATFORM_LINE_PUSH_PULL) ? "pp":"od", frise_us/1000, ffall_us/1000);


}


static void cmd_edge(const command_t *command)
{

    if (command->argc != 1)
    {
        printf("error: usage edge pp|od\r\n");
        return;
    }
  
    const char str1[] = "pp";
    
    if(strstr(command->argv[0], (const char *) str1))
    {
        if(edge_task_start(PLATFORM_LINE_PUSH_PULL, print_edge) == false)
        {
            printf("error: usage edge pp|od\r\n");
        }
        return;
    }

    const char str2[] = "od";
    if(strstr(command->argv[0], (const char *) str2))
    {
        if(edge_task_start(PLATFORM_LINE_OPEN_DRAIN, print_edge) == false)
        {
            printf("error: usage edge pp|od\r\n");
        }
        return;
    } 
    
        printf("error: usage edge pp|od\r\n");

}





static void cmd_loop(const command_t *command)
{

    if (command->argc != 1)
    {
        printf("error: usage loop internal|pins\r\n");
        return;
    }
  
    const char str1[] = "internal";
    
    if(strstr(command->argv[0], (const char *) str1))
    {
        platform_uart_loop_set(true);
        
        printf("loop: %s\r\n", str1);
        
        return;
    }

    const char str2[] = "pins";
    
    if(strstr(command->argv[0], (const char *) str2))
    {
        platform_uart_loop_set(false);
        
        printf("loop: %s\r\n", str2);
        
        return;
    }   
    
    printf("error: usage loop internal|pins\r\n");


}

static void print_errors(uint32_t errors)
{
    printf(" err: ");
    if (!errors) printf("none");
    if (errors & PLATFORM_UART_FE) printf("FE");
    if (errors & PLATFORM_UART_PE) printf("PE");
    if (errors & PLATFORM_UART_BE) printf("BE");
    if (errors & PLATFORM_UART_OE) printf("OE");
    printf("\r\n");
}


static void print_echo(const uint8_t *data, size_t size, uint32_t errors)
{
    
    char  buf[size+1];
    uint8_t i = 0;
    
    if(!size)
    {
        printf("error: pins loop\r\n");
        return;
    }


    for(i = 0; i < size; i++)
    {
        buf[i] = (char) *(data + i);
    }
    
    buf[i] = '\0';
    
       printf("rx: %s ", buf);
       print_errors(errors);

}



static void cmd_echo(const command_t *command)
{

    if (command->argc != 1)
    {
        printf("error: usage echo <text>\r\n");
        return;
    }

    uint8_t *ptr;
    ptr = (uint8_t *) command->argv[0];
    size_t cnt = 0;
    
    while(1)
    {
        if(isprint(*ptr++) == 0)  break;
 
        if(++cnt > 32)
        {
            printf("error: text longer than 32\r\n");
            return;
        }
    }

        ptr = (uint8_t *) command->argv[0];

     if(!uart_task_start(PLATFORM_UART0, PLATFORM_UART0,ptr,cnt, print_echo))
     {

            printf("error: uart is busy\r\n");
            return;
     }
}


static void print_link_back(const uint8_t *data, size_t size, uint32_t errors)
{

    char  buf[size+1];
    uint8_t i = 0;
    
    if(!size)
    {
        printf("error: pins loop\r\n");
        return;
    }


    for(i = 0; i < size; i++)
    {
        buf[i] = (char) *(data + i);
    }
    
    buf[i] = '\0';

    
    printf("1->0: %s", buf);

    print_errors(errors);

}



static void print_link_forth(const uint8_t *data, size_t size, uint32_t errors)
{
    uint8_t  bufHex[size];
    char  buf[size+1];
    uint8_t i = 0;
    
    if(!size)
    {
        printf("error: pins loop\r\n");
        return;
    }


    for(i = 0; i < size; i++)
    {
        buf[i] = (char) *(data + i);
        bufHex[i] = *(data + i);
    }
        buf[i] = '\0';

       printf("0->1: %s", buf);

       print_errors(errors);

     if(!uart_task_start(PLATFORM_UART1, PLATFORM_UART0, bufHex, size, print_link_back))
     {
            printf("error: uart is busy\r\n");
            return;
     }

}




static void cmd_link(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage link <text>\r\n");
        return;
    }

    uint8_t *ptr;
    ptr = (uint8_t *) command->argv[0];
    size_t cnt = 0;
    
    while(1)
    {
        if(isprint(*ptr++) == 0)  break;
 
        if(++cnt > 32)
        {
            printf("error: text longer than 32\r\n");
            return;
        }
    }

        ptr = (uint8_t *) command->argv[0];

     if(!uart_task_start(PLATFORM_UART0, PLATFORM_UART1, ptr, cnt, print_link_forth))
     {
            printf("error: uart is busy\r\n");
            return;
     }

}



static void cmd_baud_shift(const command_t *command)
{
    if (command->argc != 1)
    {
        printf("error: usage baud_shift <percent>\r\n");
        return;
    }

    uint8_t  sign = 0;
    char *ptr;

    ptr = (char *) command->argv[0];
    if(*ptr == '-')
    {
        ptr++;
        sign = 1;   //minus
    }

    uint32_t shift;
    if (command->argc != 1 || !parse_u32(ptr, &shift))
    {
        printf("error: usage baud_shift <percent>\r\n");
        return;
    }

    if(shift > 50)
    {
        printf("error: percent must be -50..50\r\n");
        return;
    }
  
    uint32_t baud;  //скорость UART1 = 115200 * (100 + процент) / 100

    if(sign) 
    {
        baud = 115200 * (100 - shift) / 100;    
    }
    else
    {
        baud = 115200 * (100 + shift) / 100;
    }

    baud = platform_uart_baud_set(PLATFORM_UART1, baud);

    printf("baud_shift: uart1 %u baud\r\n", (unsigned)baud);
}


static void print_text(const uint8_t *data, size_t size, uint32_t errors)
{
     char  buf[size+1];
    uint8_t i = 0;
    
    if(!size)
    {
        printf("error: pins loop\r\n");
        return;
    }


    for(i = 0; i < size; i++)
    {
        buf[i] = (char) *(data + i);
    }
    
    buf[i] = '\0';

   
    printf("text: %u chars %s ", (unsigned)size, buf);

    print_errors(errors);                                                         
}


static void cmd_text(const command_t *command)
{
    uint8_t *ptr;
    ptr = (uint8_t *) text_line;
    size_t cnt = 0;

    while(1)
    {
        if(isprint(*ptr++) == 0)  break;
 
        if(++cnt > 32)
        {
            printf("error: text longer than 32\r\n");
            return;
        }
    }
        ptr = (uint8_t *) text_line;
        string_len = cnt;

     if(!uart_task_start(PLATFORM_UART1, PLATFORM_UART0,ptr,cnt, print_text))
     {
            printf("error: uart is busy\r\n");
            return;
     }
}



static void cmd_parity(const command_t *command)
{
    if (command->argc != 2)
    {
        printf("error: usage parity none|even|odd none|even|odd\r\n");
        return;
    }

    platform_parity_t u0_mode = PLATFORM_PARITY_ERR;
    platform_parity_t u1_mode = PLATFORM_PARITY_ERR;

    const char str1[] = "none";
    const char str2[] = "even";
    const char str3[] = "odd";
    
    if(strstr(command->argv[0], (const char *) str1)) 
    {
        u0_mode = PLATFORM_PARITY_NONE;
    }
    else if(strstr(command->argv[0], (const char *) str2)) 
    {
        u0_mode = PLATFORM_PARITY_EVEN;
    }
     else if(strstr(command->argv[0], (const char *) str3)) 
    {
        u0_mode = PLATFORM_PARITY_ODD;
    } 
 

    if(strstr(command->argv[1], (const char *) str1)) 
    {
        u1_mode = PLATFORM_PARITY_NONE;
    }
    else if(strstr(command->argv[1], (const char *) str2)) 
    {
        u1_mode = PLATFORM_PARITY_EVEN;
    }
     else if(strstr(command->argv[1], (const char *) str3)) 
    {
        u1_mode = PLATFORM_PARITY_ODD;
    } 
    

    if((u0_mode == PLATFORM_PARITY_ERR) || (u1_mode == PLATFORM_PARITY_ERR))
    {
        printf("error: usage parity none|even|odd none|even|odd\r\n");
        return;
    }

        platform_uart_parity_set(PLATFORM_UART0, u0_mode);
        platform_uart_parity_set(PLATFORM_UART1, u1_mode);

        printf("parity uart0 %s, uart1 %s\r\n", command->argv[0], command->argv[1]);

}


static void print_block(const uint8_t *data, size_t size, uint32_t errors)
{
        platform_uart_rx_irq_set(false);

        int cmp = memcmp(p_blockTrBuf, data, 1);


        printf("block %s: %u/64 %s", (blockIrqmode)? "irq":"poll", 
                                     (unsigned)size, 
                                     (!cmp)? "match":"mismatch");

        print_errors(errors);                                        
}


static void cmd_block(const command_t *command)
{

    if (command->argc != 1)
    {
        printf("error: usage block irq|poll\r\n");
        return;
    }
  
    

    for(uint8_t i = 0; i < BLOCK_SIZE; i++)
    {
        blockTrBuf[i] = i;
    }
    
    p_blockTrBuf = (uint8_t *)blockTrBuf; 

    const char str1[] = "irq";
    const char str2[] = "poll";

    if(strstr(command->argv[0], (const char *) str1))
    {
        platform_uart_rx_irq_set(true);
        blockIrqmode = 1;
    }
    else if((strstr(command->argv[0], (const char *) str2)))
    {
        platform_uart_rx_irq_set(false);
        blockIrqmode = 0;
    }   
    else
    {
        printf("error: usage block irq|poll\r\n");
        return;
    }

    
        if(!uart_task_start(PLATFORM_UART1, PLATFORM_UART0, p_blockTrBuf, BLOCK_SIZE, print_block))
        {
                platform_uart_rx_irq_set(false);
                printf("error: uart is busy\r\n");
                return;
        }

}

static bool check_uart_idle(void)
{
    if (!scope.active && !uart_task_busy()) return true;
    printf("error: uart is busy\r\n");
    return false;
}



static void print_bytes(const uint8_t *data, size_t size)
{
    if (!size)
    {
        printf("(empty)");
        return;
    }
    char printable[UART_TASK_MAX + 1];
    for (size_t i = 0; i < size; ++i)
        printable[i] = isprint(data[i]) ? (char)data[i] : '.';
    printable[size] = '\0';
    printf("%s", printable);
}

static void print_uart(const char *prefix, const uint8_t *data, size_t size, uint32_t errors)
{
    printf("%s", prefix);
    print_bytes(data, size);
    printf(" ");
    print_errors(errors);
}


static void print_scope_rx(const uint8_t *data, size_t size, uint32_t errors)
{
    scope.received_size = size < sizeof scope.received ? size : sizeof scope.received;
    memcpy(scope.received, data, scope.received_size);
    scope.errors = errors;
    scope.rx_done = true;
}

static void save_scope_block(const uint16_t *samples, uint32_t count, uint32_t elapsed_us)
{
    (void)elapsed_us;
    scope.samples = samples;
    scope.sample_count = count;
}

static void restore_uart_after_scope(void)
{
    platform_uart_baud_set(PLATFORM_UART0, PLATFORM_UART_BAUD);
    platform_uart_baud_set(PLATFORM_UART1, PLATFORM_UART_BAUD);
    platform_uart_parity_set(PLATFORM_UART0, configured_parities[PLATFORM_UART0]);
    platform_uart_parity_set(PLATFORM_UART1, configured_parities[PLATFORM_UART1]);
    platform_uart_loop_set(internal_loop);
}

void api_scope_handle(void)
{
    if (!scope.active) return;
    if (!scope.tx_started && systime_us() >= scope.trigger_us)
    {
        scope.tx_started = uart_task_start(PLATFORM_UART0, PLATFORM_UART1, scope.text, scope.text_size, print_scope_rx);
    }
    
    if (!scope.samples || !scope.rx_done) return;

    restore_uart_after_scope();
    
    printf("scope: %u samples, %u Hz, %u baud, uart1 ", (unsigned)scope.sample_count,
           (unsigned)PLATFORM_SAMPLE_RATE_HZ, (unsigned)scope.baud);
    
           print_uart("", scope.received, scope.received_size, scope.errors);
   
    for (uint32_t offset = 0; offset < scope.sample_count; offset += 16)
    {
        char line[16 * 5 + 1];
        size_t length = 0;
        for (uint32_t i = 0; i < 16 && offset + i < scope.sample_count; ++i)
        {
            length += (size_t)snprintf(line + length, sizeof line - length,
                                       i ? " %u" : "%u", (unsigned)scope.samples[offset + i]);
        }
        printf("%s\r\n", line);
    }
    printf("scope: end\r\n");
    scope.active = false;
}

static void cmd_scope(const command_t *command)
{
    uint32_t baud;
    if (command->argc != 2 || 
        !parse_u32(command->argv[0], &baud) ||
        baud < SCOPE_BAUD_MIN || 
        baud > PLATFORM_UART_BAUD ||
        strlen(command->argv[1]) > SCOPE_TEXT_MAX)
    {
        printf("error: usage scope <baud> <text>\r\n");
        return;
    }

    if (!check_uart_idle()) return;

    if (edge_task_busy())
    {
        printf("error: adc busy\r\n");
        return;
    }

    platform_uart_loop_set(false);
    platform_uart_baud_set(PLATFORM_UART0, baud);
    platform_uart_baud_set(PLATFORM_UART1, baud);
    platform_uart_parity_set(PLATFORM_UART0, PLATFORM_PARITY_NONE);
    platform_uart_parity_set(PLATFORM_UART1, PLATFORM_PARITY_NONE);

    if (!block_task_start(PLATFORM_BLOCK_MAX, save_scope_block))
    {
        restore_uart_after_scope();
        printf("error: adc busy\r\n");
        return;
    }

    scope.active = true;
    scope.tx_started = scope.rx_done = false;
    scope.samples = NULL;
    scope.baud = baud;
    scope.text_size = strlen(command->argv[1]);
    memcpy(scope.text, command->argv[1], scope.text_size);
    scope.trigger_us = systime_us() + SCOPE_PRETRIGGER_US;

}




const api_command_t api_commands[] = {
    {"info", "device passport", cmd_info},
    {"uptime", "time since reset", cmd_uptime}, 
    {"pi_start", "start calculation: pi_start [<terms>]", cmd_pi_start},
    {"pi", "calculation state or result", cmd_pi},
    {"profiling", "stopwatch results", cmd_profiling},
    {"profiling_reset", "reset stopwatches", cmd_profiling_reset},
    {"led_enable", "LED on", cmd_led_enable},
    {"led_disable", "LED off", cmd_led_disable},
    {"led_blink", "LED blinking", cmd_led_blink},
    {"led_period", "set period: led_period <period_ms>", cmd_led_period},
    {"button", "button state and press count", cmd_button},
    {"edge", "rise and fall of the RC line: edge pp|od", cmd_edge},
    {"loop", "UART0 loopback: loop internal|pins", cmd_loop},
    {"echo", "text through UART0 and back: echo <text>", cmd_echo},
    {"link", "text from UART0 to UART1 and back: link <text>", cmd_link},
    {"baud_shift", "shift UART1 baud rate: baud_shift <percent>", cmd_baud_shift},
    {"text", "32 characters from UART1 to UART0", cmd_text},
    {"parity", "parity of UART0 and UART1: parity <rx> <tx>", cmd_parity},
    {"block", "64 bytes from UART1 to UART0: block irq|poll", cmd_block},
    {"scope", "UART0 line on ADC: scope <baud> <text>", cmd_scope},

};

const uint32_t api_command_count = sizeof(api_commands) / sizeof(api_commands[0]);









