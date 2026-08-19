#include "lcd_ltdc.h"

uint32_t *g_ltdc_framebuf[2];
_ltdc_dev lcdltdc;   /* 屏幕参数全局变量（h 里是 extern 声明，这里才是真正定义） */

/** @brief 初始化屏幕参数结构体和帧缓冲指针（必须在使用 ltdc_fill 前调用） */
void ltdc_param_init(void)
{
    lcdltdc.pwidth  = 1024;   // 面板宽
    lcdltdc.pheight = 600;    // 面板高
    lcdltdc.width   = 1024;   // 横屏显示宽
    lcdltdc.height  = 600;    // 横屏显示高
    lcdltdc.dir     = 1;      // 横屏
    lcdltdc.pixsize = 2;      // RGB565 = 2 字节/像素
    lcdltdc.activelayer = 0;  // 当前层 Layer 0
    lcdltdc.pixformat = LTDC_PIXEL_FORMAT_RGB565;

    g_ltdc_framebuf[0] = (uint32_t *)0xD0000000;  // 帧缓冲指向 SDRAM
}

/** @brief LTDC 纯色填充矩形（DMA2D） */
void ltdc_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{
    uint32_t psx, psy, pex, pey;
    uint32_t timeout = 0;
    uint16_t offline;
    uint32_t addr;

    if (lcdltdc.dir)
    {
        psx = sx;
        psy = sy;
        pex = ex;
        pey = ey;
    }
    else
    {
        if (ex >= lcdltdc.pheight)
        {
            ex = lcdltdc.pheight - 1;
        }

        if (sx >= lcdltdc.pheight)
        {
            sx = lcdltdc.pheight - 1;
        }

        psx = sy;
        psy = lcdltdc.pheight - ex - 1;
        pex = ey;
        pey = lcdltdc.pheight - sx - 1;
    }

    offline = lcdltdc.pwidth - (pex - psx + 1);
    addr = ((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * psy + psx));

    __HAL_RCC_DMA2D_CLK_ENABLE();

    DMA2D->CR &= ~(DMA2D_CR_START);
    DMA2D->CR = DMA2D_R2M;
    DMA2D->OPFCCR = LTDC_PIXEL_FORMAT_RGB565;
    DMA2D->OOR = offline;

    DMA2D->OMAR = addr;
    DMA2D->NLR = ((pey - psy + 1) | (pex - psx + 1) << 16);
    DMA2D->OCOLR = color;
    DMA2D->CR |= DMA2D_CR_START;

    while ((DMA2D->ISR & (DMA2D_FLAG_TC)) == 0)
    {
        timeout++;

        if (timeout > 0X1FFFFF)
            break;
    }

    DMA2D->IFCR |= DMA2D_FLAG_TC;
}

/** @brief LTDC 清屏 @param color 填充颜色 */
void ltdc_clear(uint32_t color)
{
    ltdc_fill(0, 0, lcdltdc.width - 1, lcdltdc.height - 1, color);
}

/** @brief LTDC 相关 GPIO 初始化（背光、复位、供电） */
void ltdc_gpio_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    LTDC_BL_GPIO_CLK_ENABLE();
    LTDC_RST_GPIO_CLK_ENABLE();
    LCD_PWREN_GPIO_CLK_ENABLE();

    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOJ_CLK_ENABLE();

    gpio_init_struct.Pin = LTDC_BL_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(LTDC_BL_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = LTDC_RST_GPIO_PIN;
    HAL_GPIO_Init(LTDC_RST_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Pin = LCD_PWREN_GPIO_PIN;
    HAL_GPIO_Init(LCD_PWREN_GPIO_PORT, &gpio_init_struct);

    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init_struct.Alternate = GPIO_AF14_LTDC;

    gpio_init_struct.Pin = GPIO_PIN_15;
    HAL_GPIO_Init(GPIOI, &gpio_init_struct);

    gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | /* R1, R2 */
                           GPIO_PIN_7 | GPIO_PIN_8 | /* G0, G1 */
                           GPIO_PIN_12 | GPIO_PIN_13 |
                           GPIO_PIN_14; /* B0..B2 */
    HAL_GPIO_Init(GPIOJ, &gpio_init_struct);

    LCD_PWREN(1);

    /* 复位时序：拉低 -> 延时 -> 拉高 */
    LTDC_RST(1);  HAL_Delay(10);
    LTDC_RST(0);  HAL_Delay(50);
    LTDC_RST(1);  HAL_Delay(200);

    /* 开背光 */
    LTDC_BL(1);
}
