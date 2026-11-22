#include "platform/platform.h"


#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/dma.h"
#include "hardware/sync.h"


// принятое обработчиком прерывания: блок длиннее очереди приёма
#define RX_BUFFER_SIZE 128

static volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint32_t rx_count;

static const uint LED_PIN = PICO_DEFAULT_LED_PIN;
static const uint BUTTON_PIN = 15;

// кто сейчас работает с АЦП; меняют и функции, и обработчики прерываний
typedef enum {
    ADC_FREE,
    ADC_CAPTURE,
    ADC_BLOCK,
    ADC_STREAM,
} adc_user_t;

static volatile adc_user_t adc_user;

// выводы модуля UART0
#define UART0_TX_PIN 0
#define UART0_RX_PIN 1

// выводы модуля UART1
#define UART1_TX_PIN 8
#define UART1_RX_PIN 9






// вход АЦП: вывод GP26, канал 0
#define ADC_PIN 26
#define ADC_CHANNEL 0


// опорное напряжение АЦП на плате Pico и число уровней 12-разрядного кода
#define ADC_VREF_V 3.3f
#define ADC_LEVELS 4096

// вывод, который через резистор 1 кОм ведёт линию RC-цепи
#define LINE_PIN 22


static int block_channel;
static uint16_t block_samples[PLATFORM_BLOCK_MAX];
static uint32_t block_start_us;
static volatile uint32_t block_end_us;
static volatile bool block_ready;


// АЦП в непрерывном режиме с шагом 44,1 кГц, буфер АЦП с запросом DMA
// на каждый отсчёт
static void adc_free_running(bool bits8)
{
    adc_select_input(ADC_CHANNEL);
    adc_fifo_setup(true, true, 1, false, bits8);
    adc_set_clkdiv(0);
}

static void adc_one_shot(void)
{
    adc_run(false);
    adc_fifo_drain();
    adc_fifo_setup(false, false, 0, false, false);
    adc_set_clkdiv(0);
}

// обработчик прерывания DMA_IRQ_1: блок захвачен
static void on_block_done(void)
{
    dma_channel_acknowledge_irq1(block_channel);
    block_end_us = time_us_32();
    adc_one_shot();
    block_ready = true;
    adc_user = ADC_FREE;
}

bool platform_block_start(uint32_t count)
{
    if (adc_user != ADC_FREE || count == 0 || count > PLATFORM_BLOCK_MAX)
    {
        return false;
    }

    adc_free_running(false);

    // 16-битные передачи из одного адреса в растущий, по запросу АЦП
    dma_channel_config cfg = dma_channel_get_default_config(block_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    block_ready = false;
    adc_user = ADC_BLOCK;

    // куда, откуда, сколько; true — запустить сразу
    dma_channel_configure(block_channel, &cfg, block_samples, &adc_hw->fifo, count, true);
    block_start_us = time_us_32();
    adc_run(true);

    return true;
}


bool platform_block_done(void)
{
    if(block_ready && (adc_user == ADC_FREE)) return true;

    return false;
}

void platform_erase_fDone(void)
{
    block_ready = false;

}

const uint16_t *platform_block_samples(void)
{
    return (uint16_t *) block_samples;
}

// время захвата блока, от запуска АЦП до последнего отсчёта
uint32_t platform_block_time_us(void)
{
    return block_end_us - block_start_us;
}






// обработчик UART0_IRQ: забирает из очереди приёма всё, что там есть
static void on_uart0_rx(void)
{
    while (uart_is_readable(uart0))
    {
        uint8_t byte = uart_get_hw(uart0)->dr & 0xff;
        if (rx_count < RX_BUFFER_SIZE)
        {
            rx_buffer[rx_count++] = byte;
        }
    }
}


static uart_inst_t *uart_of(platform_uart_t uart)
{
    return uart == PLATFORM_UART0 ? uart0 : uart1;
}


void platform_uart_loop_set(bool internal)
{
    uint function = internal ? GPIO_FUNC_NULL : GPIO_FUNC_UART;
    gpio_set_function(UART0_TX_PIN, function);
    gpio_set_function(UART0_RX_PIN, function);


    gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
    hw_clear_bits(&uart_get_hw(uart1)->cr, UART_UARTCR_LBE_BITS);
   
    if (internal)
    {
        hw_set_bits(&uart_get_hw(uart0)->cr, UART_UARTCR_LBE_BITS);
    }
    else
    {
        hw_clear_bits(&uart_get_hw(uart0)->cr, UART_UARTCR_LBE_BITS);
    }
}

// очередь приёма пуста, признаки ошибок сброшены
void platform_uart_clear(platform_uart_t uart)
{
    uart_inst_t *u = uart_of(uart);

    uint32_t irq_state = save_and_disable_interrupts();

    while (uart_is_readable(u)) //проверить наличие данных в буфере
    {
        (void)uart_get_hw(u)->dr;   //сбрость флаги прочитав регистр
    }

    // запись в UARTECR сбрасывает признаки ошибок
    uart_get_hw(u)->rsr = 0;

    if(uart == PLATFORM_UART0)
    {
        rx_count = 0;
    }
    restore_interrupts(irq_state);

}


// кладёт байт в очередь передачи; false — очередь полна
bool platform_uart_put(platform_uart_t uart, uint8_t byte)
{
    uart_inst_t *u = uart_of(uart);

    if (!uart_is_writable(u))
    {
        return false;
    }

    // место в очереди есть, поэтому uart_putc_raw() не ждёт
    uart_putc_raw(u, byte);
    return true;
}


// передатчик ещё работает: байты в очереди или в сдвиговом регистре
bool platform_uart_busy(platform_uart_t uart)
{
    return uart_get_hw(uart_of(uart))->fr & UART_UARTFR_BUSY_BITS;
}


// всё принятое с прошлой очистки, не больше size байтов
size_t platform_uart_read(platform_uart_t uart, uint8_t *data, size_t size)
{
    uart_inst_t *u = uart_of(uart);

    uint32_t irq_state = save_and_disable_interrupts();

    size_t n = 0;
    
    if(uart == PLATFORM_UART0)
    {
        for(n = 0; n < rx_count; n++)
        {
        data[n]  = rx_buffer[n];
        n++;   
            
        }
    }


    while (uart_is_readable(u) && n < size)
    {
        data[n++] = uart_get_hw(u)->dr & 0xff;
    }

    restore_interrupts(irq_state);

    return n;
}


// признаки ошибок приёма с прошлой очистки
uint32_t platform_uart_errors(platform_uart_t uart)
{
    return uart_get_hw(uart_of(uart))->rsr & 0xf;
}


uint32_t platform_uart_baud_set(platform_uart_t uart, uint32_t baud)
{
    return uart_set_baudrate(uart_of(uart), baud);
}

void platform_uart_parity_set(platform_uart_t uart, platform_parity_t parity)
{
    static const uart_parity_t parities[] = {
        UART_PARITY_NONE,
        UART_PARITY_EVEN,
        UART_PARITY_ODD,
    };

    uart_set_format(uart_of(uart), 8, 1, parities[parity]);
}

void platform_uart_rx_irq_set(bool on)
{
    // прерывание по четырём байтам в очереди и по паузе на линии
    uart_set_irqs_enabled(uart0, on, false);
}




void platform_init(void)
{
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, false);

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    adc_init();
    adc_gpio_init(ADC_PIN);

    
    gpio_init(LINE_PIN);
    platform_line_set(PLATFORM_LINE_PUSH_PULL, false);

    // делитель, формат 8N1, очереди, включение модуля
    uart_init(uart0,  PLATFORM_UART_BAUD);
    platform_uart_loop_set(false);  //pins enable

    uart_init(uart1,  PLATFORM_UART_BAUD);
    
    gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
    hw_clear_bits(&uart_get_hw(uart1)->cr, UART_UARTCR_LBE_BITS);


    irq_set_exclusive_handler(UART0_IRQ, on_uart0_rx);
    irq_set_enabled(UART0_IRQ, true);

    // канал блока сообщает о конце через DMA_IRQ_1
    block_channel = dma_claim_unused_channel(true);
    dma_channel_set_irq1_enabled(block_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_1, on_block_done);
    irq_set_enabled(DMA_IRQ_1, true);
}


void platform_led_set(bool on)
{
    gpio_put(LED_PIN, on);
}

bool platform_button_read(void)
{
    return !gpio_get(BUTTON_PIN);
}



float platform_voltage_read(void)
{
    adc_select_input(ADC_CHANNEL);
    return adc_read() * ADC_VREF_V / ADC_LEVELS;
}



void platform_line_set(platform_line_mode_t mode, bool high)
{
    if (mode == PLATFORM_LINE_PUSH_PULL)
    {
        // драйвер включён и сам задаёт уровень
        gpio_disable_pulls(LINE_PIN);
        gpio_put(LINE_PIN, high);
        gpio_set_dir(LINE_PIN, GPIO_OUT);
        return;
    }

    // открытый сток: в регистре выхода ноль, переключается только режим
    gpio_pull_up(LINE_PIN);
    gpio_put(LINE_PIN, 0);
    gpio_set_dir(LINE_PIN, high ? GPIO_IN : GPIO_OUT);
}
