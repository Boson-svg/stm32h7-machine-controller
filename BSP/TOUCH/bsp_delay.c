#include "bsp_delay.h"
#include "core_cm7.h"

void delay_us(uint32_t us)
{
    uint32_t ticks = (SystemCoreClock / 1000000U) * us / 5U;
    while (ticks-- > 0U) {__NOP();}
}

void delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

void dwt_init(void)
{
    /* 开启DWT访问 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* 清零计数器 */
    DWT->CYCCNT = 0;

    /* 开启CYCCNT */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void dwt_delay_us(uint32_t us)
{
   uint64_t total_cycles;
   uint32_t start;
   uint32_t current_cycles;

   total_cycles = ((uint64_t)SystemCoreClock * (uint64_t)us) / 1000000ULL;

   while (total_cycles > 0ULL)
   {
        if (total_cycles > 0x7FFFFFFFU)
        {
            current_cycles = 0x7FFFFFFFU;
        }
        else
        {
            current_cycles = (uint32_t)total_cycles;
        }

        start = DWT->CYCCNT;

        while((uint32_t)(DWT->CYCCNT - start) < current_cycles)
        {

        }

        total_cycles -= current_cycles;
   }
}
