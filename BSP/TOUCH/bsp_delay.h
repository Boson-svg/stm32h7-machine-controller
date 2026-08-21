#ifndef __BSP_DELAY_H
#define __BSP_DELAY_H

#include "main.h"
#include "stm32h7xx_hal.h"

void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

void dwt_init(void);
void dwt_delay_us(uint32_t us);

#endif
