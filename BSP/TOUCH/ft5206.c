#include "ft5206.h"
#include "ctiic.h"
#include "touch.h"
#include "lcd_ltdc.h"
#include "ltdc.h"
#include "bsp_delay.h"
#include "stdio.h"

/**
 * @brief  初始化 FT5206 触摸芯片
 * @retval 0 成功；1 芯片 ID/版本校验失败
 * @details 配 RST/INT 引脚 → ct_iic_init() → 复位时序 → 配置寄存器 → 读版本校验
 */

/*
 * @brief  FT5206初始化函数
 *
 * @return
 *      0 : 初始化成功
 *      1 : 初始化失败
 */
uint8_t ft5206_init(void)
{

    /*
     * GPIO初始化结构体
     *
     * HAL库中用于配置GPIO：
     * - 引脚
     * - 模式
     * - 上下拉
     * - 速度
     */
    GPIO_InitTypeDef gpio_init_struct = {0};



    /*
     * 开启FT5206相关GPIO时钟
     *
     * 一个是：
     * RST复位引脚
     *
     * 一个是：
     * INT中断引脚
     */
    FT5206_INT_GPIO_CLK_ENABLE();
    FT5206_RST_GPIO_CLK_ENABLE();



    /*
     * 配置RST复位引脚
     */

    gpio_init_struct.Pin = FT5206_RST_GPIO_PIN;


    /*
     * 推挽输出模式
     *
     * STM32输出高低电平控制FT5206复位
     */
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;


    /*
     * 上拉
     *
     * 默认保持高电平
     */
    gpio_init_struct.Pull = GPIO_PULLUP;


    /*
     * GPIO速度
     */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_MEDIUM;


    /*
     * 初始化RST GPIO
     */
    HAL_GPIO_Init(FT5206_RST_GPIO_PORT, &gpio_init_struct);




    /*
     * 配置INT中断输入引脚
     */

    gpio_init_struct.Pin = FT5206_INT_GPIO_PIN;


    /*
     * 输入模式
     *
     * FT5206输出INT信号通知MCU
     */
    gpio_init_struct.Mode = GPIO_MODE_INPUT;


    /*
     * 上拉输入
     */
    gpio_init_struct.Pull = GPIO_PULLUP;


    gpio_init_struct.Speed = GPIO_SPEED_FREQ_MEDIUM;


    /*
     * 初始化INT GPIO
     */
    HAL_GPIO_Init(FT5206_INT_GPIO_PORT, &gpio_init_struct);




    /*
     * 初始化软件模拟I2C
     *
     * 后面通过I2C读写FT5206寄存器
     */
    ct_iic_init();




    /*
     * FT5206硬件复位
     *
     * RST拉低：
     * 芯片进入复位状态
     */
    FT5206_RST(0);


    /*
     * 保持20ms
     */
    delay_ms(20);



    /*
     * RST拉高：
     * 芯片退出复位
     */
    FT5206_RST(1);


    /*
     * 等待芯片启动
     */
    delay_ms(50);




    /*
     * 临时缓存
     *
     * 用于保存写入/读取的数据
     *
     * 两个字节
     */
    uint8_t temp[2] = {0};




    /*
     * 设置设备模式寄存器
     *
     * temp[0]=0
     *
     * 表示进入正常工作模式
     */
    temp[0] = 0;

    ft5206_wr_reg(FT5206_DEVICE_MODE,
                  temp,
                  1);




    /*
     * 设置触摸模式
     *
     * ID_G_MODE:
     *
     * 控制FT5206中断输出模式
     */
    ft5206_wr_reg(FT5206_ID_G_MODE,
                  temp,
                  1);




    /*
     * 设置触摸检测阈值
     *
     * ID_G_THGROUP
     *
     * 数值越大：
     * 需要更强触摸信号才能触发
     *
     * 数值越小：
     * 灵敏度越高
     */
    temp[0] = 22;

    ft5206_wr_reg(FT5206_ID_G_THGROUP,
                  temp,
                  1);




    /*
     * 设置触摸检测周期
     *
     * Active模式扫描周期参数
     */
    temp[0] = 12;

    ft5206_wr_reg(FT5206_ID_G_PERIODACTIVE,
                  temp,
                  1);




    /*
     * 读取FT5206固件版本号
     *
     * 返回两个字节
     */
    ft5206_rd_reg(FT5206_ID_G_LIB_VERSION,
                  temp,
                  2);



    /*
     * 保存版本号
     *
     * temp[0]:
     * 高8位版本
     *
     * temp[1]:
     * 低8位版本
     */
    uint8_t b0 = temp[0];
    uint8_t b1 = temp[1];



    /*
     * 判断芯片版本是否正常
     *
     * 以下版本认为有效：
     *
     * b0=0x30 b1=0x03
     *
     * b1=0x01
     *
     * b1=0x02
     *
     * b0=0 b1=0
     *
     */
    if ((b0 == 0x30 && b1 == 0x03) ||
        (b1 == 0x01) ||
        (b1 == 0x02) ||
        (b0 == 0x00 && b1 == 0x00))
    {

        /*
         * 初始化成功
         */
        return 0;
    }



    /*
     * 版本不匹配
     *
     * 初始化失败
     */
    return 1;
}

/**
 * @brief  轮询扫描触摸点并更新 tp_dev
 * @param  mode  扫描模式（本驱动内部使用）
 * @retval 非 0 检测到触摸；0 无触摸
 */
/*
 * FT5206 五个触摸点坐标寄存器地址表
 *
 * FT5206最多支持5点触摸：
 * TP1_REG -> 第1个触摸点数据
 * TP2_REG -> 第2个触摸点数据
 * ...
 * TP5_REG -> 第5个触摸点数据
 *
 * 每个触摸点数据长度为4字节：
 * Byte0: X/Y高位 + 触摸事件信息
 * Byte1: 坐标低8位
 * Byte2: X/Y高位
 * Byte3: 坐标低8位
 */
static const uint16_t FT5206_TPX_TBL[5] =
{
    FT5206_TP1_REG,
    FT5206_TP2_REG,
    FT5206_TP3_REG,
    FT5206_TP4_REG,
    FT5206_TP5_REG
};


/*
 * @brief   FT5206触摸扫描函数
 *
 * @param   mode:
 *          扫描模式参数（当前未使用）
 *
 * @return
 *          1 : 检测到触摸
 *          0 : 无触摸
 *
 * 功能：
 * 1. 读取触摸点数量
 * 2. 判断是否存在有效触摸
 * 3. 读取每个触摸点坐标
 * 4. 根据屏幕方向进行坐标转换
 * 5. 更新全局触摸设备结构体 tp_dev
 */
uint8_t ft5206_scan(uint8_t mode)
{
    uint8_t sta = 0;        // 保存触摸状态寄存器值
    uint8_t buf[4];         // 保存单个触摸点4字节数据
    uint8_t i = 0;          // 循环变量
    uint8_t res = 0;        // 函数返回值
    uint16_t temp;          // 临时变量，用于生成触摸状态位
    static uint8_t t = 0;   // 静态计数器，用于降低扫描频率


    /*
     * 防止编译器产生unused warning
     *
     * 当前mode参数没有使用
     */
    (void)mode;


    /*
     * 扫描计数增加
     *
     * 不是每次调用都访问I2C，
     * 可以降低CPU负担
     */
    t++;


    /*
     * 每10次扫描一次触摸芯片
     *
     * t < 10：
     * 初始化阶段快速检测
     *
     * t % 10 == 0：
     * 正常情况下每10次检测一次
     */
    if ((t % 10) == 0 || t < 10)
    {

        /*
         * 读取触摸点数量寄存器
         *
         * FT5206_REG_NUM_FINGER：
         * 返回当前检测到几个触摸点
         *
         * 低4位有效
         */
        ft5206_rd_reg(FT5206_REG_NUM_FINGER, &sta, 1);

        //调试触摸状态
        // static uint8_t last_count = 0;
        // uint8_t current_count = (uint8_t)(sta & 0x0FU);

        // if ((last_count == 0U) && (current_count > 0U))
        // {
        //     printf("touch state: DOWN\r\n");
        // }
        // else if ((last_count > 0U) && (current_count == 0U))
        // {
        //     printf("touch state: UP\r\n");
        // }
        // else if (current_count > 0U)
        // {
        //     printf("touch state: MOVE\r\n");
        // }

        // last_count = current_count;

        /*
         * 判断触摸点数量是否有效
         *
         * sta & 0x0F：
         * 获取触摸数量
         *
         * <6：
         * FT5206最大支持5点
         */
        if ((sta & 0XF) && ((sta & 0XF) < 6))
        {


            /*
             * 根据触摸点数量生成触摸状态
             *
             * 例如：
             *
             * sta=3
             *
             * temp:
             * 1111111111111111 << 3
             *
             * 取反后：
             * 0000000000000111
             *
             * 表示前三个触摸点有效
             */
            temp = (uint16_t)(0XFFFFU << (sta & 0XF));


            /*
             * 更新触摸状态
             *
             * TP_PRES_DOWN:
             * 当前有触摸按下
             *
             * TP_CATH_PRES:
             * 捕获到触摸事件
             */
            tp_dev.sta = (uint16_t)(~temp) |
                         TP_PRES_DOWN |
                         TP_CATH_PRES;



            /*
             * 延时等待FT5206内部数据稳定
             */
            delay_us(4000U);



            /*
             * 读取最多5个触摸点坐标
             */
            for (i = 0; i < 5; i++)
            {


                /*
                 * 判断第i个触摸点是否有效
                 *
                 * bit0 -> 第1个点
                 * bit1 -> 第2个点
                 * ...
                 */
                if (tp_dev.sta & (1U << i))
                {


                    /*
                     * 读取第i个触摸点数据
                     *
                     * 每个点4字节
                     */
                    ft5206_rd_reg(FT5206_TPX_TBL[i], buf, 4);



                    /*
                     * 判断触摸方向
                     *
                     * touchtype bit0:
                     *
                     * 1:
                     * 交换X/Y
                     *
                     * 0:
                     * 正常读取
                     */
                    if (tp_dev.touchtype & 0X01)
                    {


                        /*
                         * FT5206坐标格式：
                         *
                         * Byte0低4位 + Byte1 = Y坐标
                         *
                         * Byte2低4位 + Byte3 = X坐标
                         */
                        tp_dev.y[i] =
                        (uint16_t)(((uint16_t)(buf[0] & 0X0F) << 8)
                                   + buf[1]);


                        tp_dev.x[i] =
                        (uint16_t)(((uint16_t)(buf[2] & 0X0F) << 8)
                                   + buf[3]);

                    }
                    else
                    {


                        /*
                         * 屏幕镜像处理
                         *
                         * X坐标反转：
                         *
                         * 新X = 屏幕宽度 - 原X
                         *
                         * 用于适配LCD方向
                         */
                        tp_dev.x[i] =
                        (uint16_t)
                        (
                          lcdltdc.width -
                          (((uint16_t)(buf[0] & 0X0F) << 8)
                          + buf[1])
                        );


                        /*
                         * Y坐标正常读取
                         */
                        tp_dev.y[i] =
                        (uint16_t)
                        (
                          ((uint16_t)(buf[2] & 0X0F) << 8)
                          + buf[3]
                        );

                    }



                    /*
                     * 判断触摸事件类型
                     *
                     * buf[0]高4位：
                     *
                     * 0x80表示有效触摸点
                     *
                     * 如果不是：
                     * 认为该点无效
                     */
                    if ((buf[0] & 0XF0) != 0X80)
                    {
                        tp_dev.x[i] = 0;
                        tp_dev.y[i] = 0;
                    }

                }
            }


            /*
             * 表示检测到了有效触摸
             */
            res = 1;



            /*
             * 如果第一个触摸点坐标为0
             *
             * 认为此次读取无效
             */
            if (tp_dev.x[0] == 0 &&
                tp_dev.y[0] == 0)
            {
                sta = 0;
            }


            /*
             * 清零计数器
             *
             * 下次重新开始计数
             */
            t = 0;
        }
    }



    /*
     * 如果没有检测到触摸
     */
    if ((sta & 0X1F) == 0)
    {


        /*
         * 如果之前处于按下状态
         *
         * 清除按下标志
         *
         * 表示一次触摸结束
         */
        if (tp_dev.sta & TP_PRES_DOWN)
        {
            tp_dev.sta &= ~TP_PRES_DOWN;
        }
        else
        {


            /*
             * 连续无触摸
             *
             * 清除坐标
             */
            tp_dev.x[0] = 0xffff;
            tp_dev.y[0] = 0xffff;


            /*
             * 保留高3位状态
             */
            tp_dev.sta &= 0XE000;

        }
    }



    /*
     * 防止计数器溢出
     */
    if (t > 240)
    {
        t = 10;
    }



    /*
     * 返回扫描结果
     *
     * 1: 有触摸
     * 0: 无触摸
     */
    return res;
}

/**
 * @brief  向 FT5206 寄存器写入数据
 * @param  reg  寄存器地址
 * @param  buf  写入数据缓冲区
 * @param  len  写入长度
 * @retval 0 成功；非 0 I2C 错误
 */
uint8_t ft5206_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t ret = 0;

    ct_iic_start();
    ct_iic_send_byte(FT5206_CMD_WR);
    ret = ct_iic_wait_ack();
    if (ret) { ct_iic_stop(); return ret; }

    ct_iic_send_byte((uint8_t)(reg & 0xFF));
    ret = ct_iic_wait_ack();
    if (ret) { ct_iic_stop(); return ret; }

    for (uint8_t i = 0; i < len; i++)
    {
        ct_iic_send_byte(buf[i]);
        ret = ct_iic_wait_ack();
        if (ret) break;
    }

    ct_iic_stop();
    return ret;
}

/**
 * @brief  从 FT5206 寄存器读取数据
 * @param  reg  寄存器地址
 * @param  buf  输出缓冲区
 * @param  len  读取长度
 */
void ft5206_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    ct_iic_start();
    ct_iic_send_byte(FT5206_CMD_WR);
    ct_iic_wait_ack();
    ct_iic_send_byte((uint8_t)(reg & 0xFF));
    ct_iic_wait_ack();

    ct_iic_start();
    ct_iic_send_byte(FT5206_CMD_RD);
    ct_iic_wait_ack();

    for (uint8_t i = 0; i < len; i++)
    {
        buf[i] = ct_iic_read_byte(i == (len - 1) ? 0 : 1);
    }

    ct_iic_stop();
}
