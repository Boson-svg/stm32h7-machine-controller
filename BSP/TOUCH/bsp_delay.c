#include "bsp_delay.h"

void delay_us(uint32_t us)
{
    uint32_t ticks = (SystemCoreClock / 1000000U) * us / 5U;
    while (ticks-- > 0U) {__NOP();}
}

void delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}