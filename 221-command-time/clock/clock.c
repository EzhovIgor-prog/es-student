#include "clock.h"
#include <stdio.h>
#include <stddef.h>
#include "hardware/regs/addressmap.h"
#include "hardware/regs/sysinfo.h"
#include "hardware/clocks.h"


static void row(const char *name, uint32_t set_khz, uint32_t measured_khz)
{
    printf("%-8s %9u %12u\n", name, (unsigned)set_khz, (unsigned)measured_khz);
}

void clk_info(void)
{

   // uint32_t sys_hz = clock_get_hz(clk_sys);

   // uint32_t sys_khz = frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS);

    printf("%-12s %-12s %-6s\n", "signal", "set_khz", "measured_khz");
      
// clk_ref, clk_sys, clk_peri, clk_usb, clk_adc

    row("clk_ref", clock_get_hz(clk_ref) / 1000, frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_REF));
    row("clk_sys", clock_get_hz(clk_sys) / 1000, frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_SYS));
    row("clk_peri", clock_get_hz(clk_peri) / 1000, frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_PERI));
    row("clk_usb", clock_get_hz(clk_usb) / 1000, frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_USB));
    row("clk_adc", clock_get_hz(clk_adc) / 1000, frequency_count_khz(CLOCKS_FC0_SRC_VALUE_CLK_ADC));
    
    printf("%-8s %9s %12u\n", "rosc","-", (unsigned)frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC));
   // row("clk_adc", clock_get_hz(clk_adc) / 1000, frequency_count_khz(CLOCKS_FC0_SRC_VALUE_ROSC_CLKSRC));
}
