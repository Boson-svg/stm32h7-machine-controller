#include "dht11.h"
#include "FreeRTOS.h"
#include "task.h"
#include "core_cm7.h"

static void dht11_pin_output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    HAL_GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStruct);
}

static void dht11_pin_input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    HAL_GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStruct);
}
static GPIO_PinState dht11_line_read(void)
{
    return HAL_GPIO_ReadPin(
        DHT11_GPIO_PORT,
        DHT11_GPIO_PIN);
}

static void dht11_line_high(void)
{
    HAL_GPIO_WritePin(
        DHT11_GPIO_PORT,
        DHT11_GPIO_PIN,
        GPIO_PIN_SET);
}

static void dht11_line_low(void)
{
    HAL_GPIO_WritePin(
        DHT11_GPIO_PORT,
        DHT11_GPIO_PIN,
        GPIO_PIN_RESET);
}

static int dht11_wait_level(
    GPIO_PinState level,
    uint32_t timeout_us)
{
    while (timeout_us > 0U)
    {
        if (dht11_line_read() == level)
        {
            return 0;
        }

        dwt_delay_us(1U);
        timeout_us--;
    }

    return -1;
}

void DHT11_Init(void)
{
    DHT11_GPIO_CLK_ENABLE();

    dwt_init();

    dht11_pin_output();
    dht11_line_high();
}

static int dht11_read_byte(uint8_t *value)
{
    uint8_t data = 0;
    uint32_t start = 0;
    uint32_t high_time = 0;
    uint32_t threshold_cycles;

    threshold_cycles =
        (uint32_t)(((uint64_t)SystemCoreClock *
                    DHT11_BIT_SAMPLE_US) /
                   1000000ULL);

    for (uint8_t i = 0; i < 8; i++)
    {
        if (dht11_wait_level(GPIO_PIN_SET, DHT11_TIMEOUT_US) != 0)
        {
            return -1;
        }

        start = DWT->CYCCNT;

        if (dht11_wait_level(GPIO_PIN_RESET, DHT11_TIMEOUT_US) != 0)
        {
            return -1;
        }

        high_time = DWT->CYCCNT - start;

        data <<= 1;

        if (high_time > threshold_cycles)
        {
            data |= 1;
        }
    }

    *value = data;
    return 0;
}

int DHT11_Read(DHT11_Data_t *data)
{
    uint8_t raw[5] = {0};
    uint8_t checksum;
    uint32_t primask;
    int result = -1;
    int i;

    if (data == NULL)
    {
        return -1;
    }

    /*
     * 主机起始信号：
     * 拉低至少18ms。
     * 这里使用HAL_Delay，避免DWT长时间延时溢出。
     */
    dht11_pin_output();
    dht11_line_low();
    vTaskDelay(pdMS_TO_TICKS(DHT11_START_LOW_MS));

    /*
     * 进入严格时序阶段。
     * DHT11的40位数据读取大约需要数毫秒。
     */

     primask = __get_PRIMASK();
     __disable_irq();

     dht11_line_high();
     dwt_delay_us(30U);

     dht11_pin_input();

    /* DHT11响应：80us低电平 */
     if (dht11_wait_level(
            GPIO_PIN_RESET, 
            DHT11_TIMEOUT_US) != 0)
     {
         goto exit;
     }

     /* DHT11响应：80us高电平 */
     if (dht11_wait_level(
             GPIO_PIN_SET,
             DHT11_TIMEOUT_US) != 0)
     {
         goto exit;
     }

   /* 等待第一个数据位的低电平阶段 */
    if (dht11_wait_level(
            GPIO_PIN_RESET,
            DHT11_TIMEOUT_US) != 0)
    {
        goto exit;
    }

    for (i = 0; i < 5; i++)
    {
        if (dht11_read_byte(&raw[i]) != 0)
        {
            goto exit;
        }
    }

    checksum = (uint8_t)(
        raw[0] +
        raw[1] +
        raw[2] +
        raw[3]);

    if (checksum != raw[4])
    {
        goto exit;
    }

    data->humidity = raw[0];
    data->humidity_dec = raw[1];
    data->temperature = raw[2];
    data->temperature_dec = raw[3];

    result = 0;

exit:
    /*
     * 无论成功还是失败，都释放总线。
     */
    dht11_pin_output();
    dht11_line_high();

    __set_PRIMASK(primask);

    return result;
}