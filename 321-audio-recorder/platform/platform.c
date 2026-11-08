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

    float u = adc_read() * ADC_VREF_V / ADC_LEVELS;

    return 27.0 - (u - 0.706) / 0.001721;
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






