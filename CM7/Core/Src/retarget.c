#include "retarget.h"

int __io_putchar(int ch)
{
    uint8_t c = (uint8_t)ch;
    if (ch =='\n')
    {
        uint8_t cr = '\r';
        HAL_UART_Transmit(&huart1, &cr, 1, 100);
    }
    HAL_UART_Transmit(&huart1, &c, 1, 100);
    return ch;
}
