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


    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_set_temp_sensor_enabled(true);  //включить питание датчика температуры чипа

    gpio_init(RC_CHARGE_PIN);
    gpio_set_dir(RC_CHARGE_PIN, GPIO_OUT);
    gpio_put(RC_CHARGE_PIN, false);

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