#include "platform/platform.h"

#include "pico/stdlib.h"
#include "hardware/adc.h"

// вход АЦП: вывод GP26, канал 0
#define ADC_PIN 26
#define ADC_CHANNEL 0
// встроенный датчик температуры — канал 4
#define ADC_TEMP_CHANNEL 4

// опорное напряжение АЦП на плате Pico и число уровней 12-разрядного кода
#define ADC_VREF_V 3.3f
#define ADC_LEVELS 4096
#define RC_CHARGE_PIN 22

static repeating_timer_t blink_timer;
static bool blink_running;
static volatile bool blink_on;
static volatile uint32_t blink_toggles;

static const uint LED_PIN = PICO_DEFAULT_LED_PIN;
static const uint BUTTON_PINS[PLATFORM_BUTTON_QTY] = {15, 14};

// кто сейчас работает с АЦП; меняют и функции, и обработчики прерываний
typedef enum {
    ADC_FREE,
    ADC_CAPTURE,
} adc_user_t;

static volatile adc_user_t adc_user;

static repeating_timer_t capture_timer;
static uint16_t capture_samples[PLATFORM_CAPTURE_MAX];
static uint32_t capture_times[PLATFORM_CAPTURE_MAX];
static uint32_t capture_count = 1000;
static uint32_t capture_period_us;
static volatile uint32_t capture_index = 0;
static volatile uint32_t capture_handler_us;




void platform_init(void)
{
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, false);

    for (platform_button_t j = 0; j < PLATFORM_BUTTON_QTY; j++)
    {
        gpio_init(BUTTON_PINS[(uint)j]);
        gpio_set_dir(BUTTON_PINS[(uint)j], GPIO_IN);
        gpio_pull_up(BUTTON_PINS[(uint)j]);
    }

    adc_init();
    adc_gpio_init(ADC_PIN);


}


void platform_led_set(bool on)
{
    gpio_put(LED_PIN, on);
}

bool platform_button_read(platform_button_t id)
{
    return (id < PLATFORM_BUTTON_QTY && !gpio_get(BUTTON_PINS[id]));
}

float platform_voltage_read(void)
{
    adc_select_input(ADC_CHANNEL);
    return adc_read() * ADC_VREF_V / ADC_LEVELS;
}

float platform_temp_read(void)
{
    adc_select_input(ADC_TEMP_CHANNEL);

    float u = (adc_read() * ADC_VREF_V) / ADC_LEVELS;

    return 27.0 - ((u - 0.706) / 0.001721);
}

void platform_charge_set(bool on)
{
        gpio_put(RC_CHARGE_PIN, on);
}


// обработчик будильника индикатора
static bool on_blink(repeating_timer_t *timer)
{
    blink_on = !blink_on;
    gpio_put(LED_PIN, blink_on);
    blink_toggles++;

    // true — вызвать снова через тот же период
    return true;
}

bool platform_blink_start(uint32_t period_ms)
{
    if (period_ms < 10 || period_ms > 60000)
    {
        return false;
    }

    platform_blink_stop();
    blink_toggles = 0;

    // минус перед периодом — отсчитывать от начала прошлого вызова
    blink_running = add_repeating_timer_ms(-(int32_t)period_ms, on_blink, NULL, &blink_timer);
    return blink_running;
}

uint32_t platform_blink_stop(void)
{
    if (blink_running)
    {
        cancel_repeating_timer(&blink_timer);
        blink_running = false;
    }

    blink_on = false;
    gpio_put(LED_PIN, 0);
    return blink_toggles;
}


// обработчик будильника захвата: время и отсчёт, затем время своей работы
static bool on_capture(repeating_timer_t *timer)
{
    uint32_t start_us = time_us_32();
    capture_times[capture_index] = start_us;
    capture_samples[capture_index] = adc_read();
    capture_index++;
    capture_handler_us += time_us_32() - start_us;

    if (capture_index < capture_count)
    {
        return true;
    }
    //cancel_repeating_timer(&capture_timer);
    // захват окончен: АЦП свободен, будильник снимается
    adc_user = ADC_FREE;
    return false;
}




bool platform_capture_start(uint32_t count, uint32_t period_us)
{
    if (adc_user != ADC_FREE || count == 0 || count > PLATFORM_CAPTURE_MAX || period_us == 0)
    {
        return false;
    }

    adc_select_input(ADC_CHANNEL);
    capture_count = count;
    capture_period_us = period_us;
    capture_index = 0;
    capture_handler_us = 0;
    adc_user = ADC_CAPTURE;

  
   
    if (!add_repeating_timer_us(-(int64_t)period_us, on_capture, NULL, &capture_timer))
    {
        adc_user = ADC_FREE;
        return false;
    }
   

    return true;
}

// platform_adc_busy(): АЦП не свободен

// platform_capture_done(): захват запускали, и набраны все отсчёты

// platform_capture_get(): адрес отсчётов, их число и период;
// по разностям соседних времён — наименьший и наибольший интервал;
// доля обработчика — его общее время в процентах от времени
// между первым и последним отсчётом;
// у одного отсчёта интервалов нет, тогда всё это нули


bool platform_adc_busy(void)
{
    if(adc_user == ADC_CAPTURE)
    {
        return true;
    }

    return false;
}

bool platform_capture_done(void)
{
    if((adc_user == ADC_FREE) && (capture_index == capture_count))
    {
        return true;
    }

    return false;
}

void platform_capture_get(platform_capture_t *capture)
{


    uint16_t    i, min_interval, max_interval, j; 

    min_interval = max_interval = capture_times[1] - capture_times[0];

    for(i = 0; i< (capture_count-1); i++)
    {
        j = capture_times[i+1] - capture_times[i];    
        if(j > max_interval) max_interval = j;
        if(j < min_interval) min_interval = j;
    }


    capture->samples = capture_samples;
    capture->count = capture_count;
    capture->period_us = capture_period_us;
    capture->min_interval_us = min_interval;
    capture->max_interval_us = max_interval;
    capture->handler_percent = ((float)capture_handler_us / (float)((capture_count * capture_period_us)/100));

}

