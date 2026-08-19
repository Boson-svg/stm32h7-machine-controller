#ifndef __LCD_LTDC_H
#define __LCD_LTDC_H

#include "main.h"
#include "ltdc.h"
#include "dma2d.h"

typedef struct {
    uint16_t width;    // 显示宽（横屏时=1024）
    uint16_t height;   // 显示高（横屏时=600）
    uint16_t pwidth;   // 面板宽（=1024）
    uint16_t pheight;  // 面板高（=600）
    uint8_t  dir;      // 方向：0竖屏 1横屏
    uint8_t  pixsize;  // 每像素字节数（RGB565=2）
    uint8_t  activelayer; // 当前层
    uint8_t  pixformat;   // 像素格式
} _ltdc_dev;

extern _ltdc_dev lcdltdc;
/******************************************************************************************/
/* LCD PWREN 引脚 定义
 * LCD_PWREN引脚用于IO控制LCD_5V的开启关闭, 开启LCD_5V需要将LCD_PWREN引脚输出1.
 */

#define LCD_PWREN_GPIO_PORT               GPIOI
#define LCD_PWREN_GPIO_PIN                GPIO_PIN_11
#define LCD_PWREN_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOI_CLK_ENABLE(); }while(0)   /* PI口时钟使能 */

#define LCD_PWREN(x)      do{ x ? \
                              HAL_GPIO_WritePin(LCD_PWREN_GPIO_PORT, LCD_PWREN_GPIO_PIN, GPIO_PIN_SET) : \
                              HAL_GPIO_WritePin(LCD_PWREN_GPIO_PORT, LCD_PWREN_GPIO_PIN, GPIO_PIN_RESET); \
                          }while(0)

#define LTDC_DE_GPIO_PORT               GPIOK
#define LTDC_DE_GPIO_PIN                GPIO_PIN_7
#define LTDC_DE_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOK_CLK_ENABLE(); }while(0)

#define LTDC_VSYNC_GPIO_PORT            GPIOI
#define LTDC_VSYNC_GPIO_PIN             GPIO_PIN_13
#define LTDC_VSYNC_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOI_CLK_ENABLE(); }while(0)

#define LTDC_HSYNC_GPIO_PORT            GPIOI
#define LTDC_HSYNC_GPIO_PIN             GPIO_PIN_12
#define LTDC_HSYNC_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOI_CLK_ENABLE(); }while(0)

#define LTDC_CLK_GPIO_PORT              GPIOI
#define LTDC_CLK_GPIO_PIN               GPIO_PIN_14
#define LTDC_CLK_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOI_CLK_ENABLE(); }while(0)

#define LTDC_BL_GPIO_PORT               GPIOB
#define LTDC_BL_GPIO_PIN                GPIO_PIN_0
#define LTDC_BL_GPIO_CLK_ENABLE()       do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

#define LTDC_RST_GPIO_PORT              GPIOH
#define LTDC_RST_GPIO_PIN               GPIO_PIN_5
#define LTDC_RST_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOH_CLK_ENABLE(); }while(0)

#define LTDC_BL(x)  do{ (x) ? HAL_GPIO_WritePin(LTDC_BL_GPIO_PORT, LTDC_BL_GPIO_PIN, GPIO_PIN_SET) \
                            : HAL_GPIO_WritePin(LTDC_BL_GPIO_PORT, LTDC_BL_GPIO_PIN, GPIO_PIN_RESET); }while(0)

#define LTDC_RST(x) do{ (x) ? HAL_GPIO_WritePin(LTDC_RST_GPIO_PORT, LTDC_RST_GPIO_PIN, GPIO_PIN_SET) \
                            : HAL_GPIO_WritePin(LTDC_RST_GPIO_PORT, LTDC_RST_GPIO_PIN, GPIO_PIN_RESET); }while(0)

/* RGB565 颜色定义（与 LTDC/DMA2D 的像素格式一致） */
#define WHITE           0xFFFF      /* 白色 */
#define BLACK           0x0000      /* 黑色 */
#define RED             0xF800      /* 红色 */
#define GREEN           0x07E0      /* 绿色 */
#define BLUE            0x001F      /* 蓝色 */
#define MAGENTA         0xF81F      /* 品红 = 蓝 + 红 */
#define YELLOW          0xFFE0      /* 黄色 = 绿 + 红 */
#define CYAN            0x07FF      /* 青色 = 绿 + 蓝 */

#define BROWN           0xBC40      /* 棕色 */
#define BRRED           0xFC07      /* 棕红色 */
#define GRAY            0x8430      /* 灰色 */
#define DARKBLUE        0x01CF      /* 深蓝色 */
#define LIGHTBLUE       0x7D7C      /* 浅蓝色 */
#define GRAYBLUE        0x5458      /* 灰蓝色 */
#define LIGHTGREEN      0x841F      /* 浅绿色 */
#define LGRAY           0xC618      /* 浅灰色 */
#define LGRAYBLUE       0xA651      /* 浅灰蓝色 */
#define LBBLUE          0x2B12      /* 浅棕蓝色 */

/** @brief LTDC 纯色填充矩形（DMA2D） */
void ltdc_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color);

/** @brief LTDC 清屏 @param color 填充颜色 */
void ltdc_clear(uint32_t color);

/** @brief LTDC 相关 GPIO 初始化（背光、复位、供电） */
void ltdc_gpio_init(void);

/** @brief 初始化屏幕参数结构体和帧缓冲指针（必须在使用 ltdc_fill 前调用） */
void ltdc_param_init(void);

#endif
