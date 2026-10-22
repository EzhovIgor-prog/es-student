#include "platform/platform.h"

#include "pico/stdlib.h"

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

