#include "edge-task/edge-task.h"

#include <stddef.h>
#include "systime/systime.h"
#include "platform/platform.h"


float   EMPTY_V = 0.05f;
float   RISE_V = 2.0f;
float   FULL_V = 3.2f;
float   FALL_V = 1.3f;

#define TIMEOUT_US  2000000

typedef enum
{
    EDGE_IDLE,
    EDGE_DISCHARGE,
    EDGE_RISE,
    EDGE_CHARGE,
    EDGE_FALL,
} edge_state_t;

static edge_task_callback_t edge_callback;
static edge_state_t edge_state = EDGE_IDLE;
static platform_line_mode_t set_mode;
static uint32_t  rase_time = 0;
static uint32_t  fall_time = 0;
static uint64_t  start_time = 0;
static bool     edge_ok = false;
static float adc_data;

// edge-task.c
// пороги EMPTY_V 0.05f, RISE_V 2.0f, FULL_V 3.2f, FALL_V 1.3f, TIMEOUT_US 2000000
// состояние, режим линии, функция результата, начало фазы, время фронта вверх

// enter(): уровень на линии через platform_line_set(), начало фазы — сейчас, новое состояние
// finish(): состояние IDLE, затем вызов функции результата

// edge_task_start(): замер уже идёт — false; иначе запомнить режим и функцию,
// ноль на линии, состояние DISCHARGE

// edge_task_handle(): в IDLE ничего; иначе прошедшее время фазы и напряжение узла,
// переходы по таблице, фаза дольше TIMEOUT_US — finish(false, 0)



bool edge_task_start(platform_line_mode_t mode, edge_task_callback_t on_done)
{

    if(on_done == NULL || edge_state != EDGE_IDLE) return false;

    set_mode = mode;
    edge_callback = on_done;
    edge_state = EDGE_DISCHARGE;
    start_time = systime_us();

    platform_line_set(set_mode, false); //discharge capacitor

    return true;
}

void edge_task_handle(void)
{


    switch(edge_state)
    {
        case EDGE_IDLE: return;
        
        
        adc_data = platform_voltage_read();

            if((systime_us() - start_time) > TIMEOUT_US)
            {
                platform_line_set(set_mode, false); //discharge capacitor
                edge_ok = false;
                edge_state = EDGE_IDLE;
                edge_callback(set_mode, edge_ok, rase_time, fall_time);
            }


        case EDGE_DISCHARGE:

            
            if(adc_data < EMPTY_V) 
            {
                edge_state = EDGE_RISE;

                platform_line_set(set_mode, true); //charge capacitor
                rase_time = systime_us();
            }
 
        break;

        case EDGE_RISE:
        
            adc_data = platform_voltage_read();
            if(adc_data > RISE_V) 
            {
                edge_state = EDGE_CHARGE;

                rase_time = systime_us() - rase_time;
                
            }

        break;

        case EDGE_CHARGE:
                
                adc_data = platform_voltage_read();
                if(adc_data > FULL_V) 
                {
                edge_state = EDGE_FALL;

                fall_time = systime_us();
                platform_line_set(set_mode, false); //discharge capacitor

                }
        break;

       case EDGE_FALL:
        
            if(adc_data < FALL_V)
            {
                edge_state = EDGE_IDLE;

                fall_time = systime_us() - fall_time;
                edge_ok = true;

                edge_callback(set_mode, edge_ok, rase_time, fall_time);
            }
        break;

    }

    
}

bool edge_task_busy(void) { return edge_state != EDGE_IDLE; }


