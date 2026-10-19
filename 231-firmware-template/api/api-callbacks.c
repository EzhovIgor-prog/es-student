#include "api/api-commands.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "device/device.h"
#include "profiling/profiling.h"



volatile double pi_result;



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



double calc_pi(uint n)
{
    double sum = 0.0;
    double sign = 1.0;

    for (int k = 0; k < n; k++) {
        // Прибавить очередной член ряда: ±1 / (2k + 1)
        sum += sign / (2.0 * k + 1.0);
        sign = -sign;
    }

    // Сумма ряда равна π/4
    return 4.0 * sum;
}




void cmd_calc_pi(const command_t *command)
{
    
    uint32_t terms = 1000000;
    if (command->argc > 1 || (command->argc == 1 && !parse_u32(command->argv[0], &terms)))
    {
        printf("error: usage calc_pi [<terms>]\r\n");
        return;
    }
    if (terms == 0)
    {
        printf("error: terms must be greater than 0\r\n");
        return;
    }
    
    
    
    //printf("calc_pi %u\r\n", (unsigned)terms);


    uint64_t start_us = time_us_64();
    pi_result = calc_pi(terms);
    uint64_t spent_us = time_us_64() - start_us;

    printf("pi: %.8f\n", pi_result);
    printf("time: %llu ms\n", spent_us / 1000);
}




void cmd_time_exec(const command_t *command)
{
   time_exec();
}

void cmd_time_reset(const command_t *command)
{
   time_reset();
}



const api_command_t api_commands[] = {
    { "info", "device passport", cmd_info },
    { "uptime", "time since reset", cmd_uptime }, 
    { "calc_pi", "calculate pi: calc_pi [terms]", cmd_calc_pi },
    { "main_time_exec", "superloop iteration: average and maximum", cmd_time_exec },
    { "main_time_reset", "reset maximum iteration", cmd_time_reset }, 
  
};

const uint32_t api_command_count = sizeof(api_commands) / sizeof(api_commands[0]);









