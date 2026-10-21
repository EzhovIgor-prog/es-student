#include "pi-task/pi-task.h"
#include <stddef.h>
#include "pico/stdlib.h"
#include "systime/systime.h"

// 585 тактов при 125 МГц: 100 * 585 / 125 = 468 мкс.
#define PI_TERMS_PER_CALL 100

static pi_state_t pi_state;
static uint32_t pi_k;
static uint32_t pi_terms;
static double pi_sum;
static double pi_sign;
static double pi_result;
static uint64_t started_us;
static uint64_t finished_us;

void pi_task_init(void)
{
    pi_state = PI_NOT_STARTED;
}

bool pi_task_start(uint32_t terms)
{
    if ((terms == 0) || (pi_state == PI_RUNNING))
    {
        return false;
    }

    started_us = systime_us();
    pi_state = PI_RUNNING;
    pi_k = 0;
    pi_terms = terms;
    pi_sum = 0.0;
    pi_sign = 1.0;
    
    
    return true;
}

void pi_task_handle(void)
{
    if (pi_state != PI_RUNNING)
    {
        return;
    }
    // Сначала вычитаем: даже для UINT32_MAX конец порции не переполнится.
    uint32_t remaining = pi_terms - pi_k;
    uint32_t portion = remaining < PI_TERMS_PER_CALL ? remaining : PI_TERMS_PER_CALL;
    uint32_t end = pi_k + portion;
    for (; pi_k < end; pi_k++)
    {
        pi_sum += pi_sign / (2.0 * pi_k + 1.0);
        pi_sign = -pi_sign;
    }
    if (pi_k == pi_terms)
    {
        pi_result = 4.0 * pi_sum;
        finished_us = systime_us();
        pi_state = PI_READY;
    }
}

pi_state_t pi_task_get_state(void)
{
    return pi_state;
}

bool pi_task_get_result(double *result, uint64_t *elapsed_ms)
{
    if ((pi_state != PI_READY) || (result == NULL) || (elapsed_ms == NULL))
    {
        return false;
    }
    *result = pi_result;
    *elapsed_ms = (finished_us - started_us) / 1000;
    return true;
}
