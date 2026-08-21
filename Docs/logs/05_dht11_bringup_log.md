# Day 05：DHT11驱动与FreeRTOS采样任务验证

> 工作日期：2026-08-22  
> 工程：STM32H747XIH6双核工业控制器复刻项目  
> 工作范围：PD12 GPIO、DWT微秒延时、DHT11单总线协议、FreeRTOS任务

## 一、今日目标

1. 确认DHT11实际使用的GPIO和CubeMX配置；
2. 在已有`bsp_delay`模块中加入DWT周期计数器延时；
3. 手写DHT11初始化、响应检测、40位数据读取和校验和校验；
4. 创建DHT11 FreeRTOS采样任务；
5. 通过串口日志验证温湿度数据，并观察现有任务资源状态。

## 二、硬件与CubeMX配置

### 2.1 引脚确认

本工程DHT11数据线使用PD12，归属CM7。PD1已经用于FMC SDRAM数据线，不能作为DHT11数据线使用。

```text
DHT11 VCC   -> 3.3V
DHT11 GND   -> GND
DHT11 DATA  -> PD12
PD12 DATA   -> 外接4.7K~10K上拉电阻
```

### 2.2 GPIO参数

```text
Mode  = GPIO_MODE_OUTPUT_OD
Pull  = GPIO_PULLUP
Speed = GPIO_SPEED_FREQ_MEDIUM
初始电平 = GPIO_PIN_SET
```

开漏输出的高电平不是主动驱动，而是释放总线，由上拉电阻将数据线拉高；低电平由MCU主动拉低。

## 三、DWT微秒延时

文件：`BSP/TOUCH/bsp_delay.c`

### 3.1 初始化流程

```c
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
DWT->CYCCNT = 0U;
DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

`DWT->CYCCNT`按照CPU周期递增。DHT11的响应和数据位宽度只有几十微秒，不能使用FreeRTOS Tick或毫秒级`HAL_Delay()`判断。

### 3.2 长延时溢出问题

如果直接使用：

```c
uint32_t cycles = (SystemCoreClock / 1000000U) * us;
```

在高主频和20 ms延时下可能超过32位整数范围。因此当前实现使用64位计算，并将长延时拆成多个不超过`0x7FFFFFFF`周期的片段，保证DWT计数器回绕时仍可正确比较。

## 四、DHT11协议实现

### 4.1 主机起始信号

```text
MCU拉低数据线至少18 ms
MCU释放数据线约30 us
切换为输入
```

当前任务环境下，20 ms低电平使用`vTaskDelay()`。GPIO输出状态会在任务阻塞期间保持低电平，因此任务睡眠不会改变DHT11起始信号。

### 4.2 传感器响应

```text
DHT11拉低约80 us
DHT11拉高约80 us
随后发送40位数据
```

驱动通过`dht11_wait_level()`等待指定电平，并设置100 us超时，避免传感器断线时永久阻塞。

### 4.3 数据位解析

每一位数据都包含一个约50 us的低电平和一个高电平：

```text
数据0：高电平约26~28 us
数据1：高电平约70 us
```

当前实现记录高电平持续的DWT周期数，并将40 us转换成对应的CPU周期数后进行比较。

### 4.4 数据校验

五个字节的格式为：

```text
raw[0]：湿度整数
raw[1]：湿度小数
raw[2]：温度整数
raw[3]：温度小数
raw[4]：校验和
```

校验规则：

```c
checksum = raw[0] + raw[1] + raw[2] + raw[3];
checksum == raw[4]
```

只有校验通过后，驱动才会写入输出结构体。

## 五、FreeRTOS任务配置

文件：`CM7/Core/App/Src/app_tasks.c`

```c
#define DHT11_TASK_STACK_SIZE 512U
#define DHT11_TASK_PRIORITY   2U
```

任务使用512个FreeRTOS栈字，优先级为2。触摸任务优先级为3，系统监控任务优先级为1，因此DHT11采样不会抢占1 ms触摸任务。

任务启动流程为：

```text
DHT11_Task创建
    -> DHT11_Init()
    -> 上电等待1 s
    -> DHT11_Read()
    -> 输出串口日志
    -> 延时2 s
    -> 重复采样
```

`DHT11_Task`通过`AppTasks_Init()`创建，`freertos.c`只负责在用户代码区域调用：

```c
Mutex_Init();
Queue_Init();
AppTasks_Init();
```

这样CubeMX重新生成FreeRTOS代码时，应用任务仍然位于自己的`CM7/Core/App`目录中。

## 六、运行验证证据

开发板串口输出：

```text
monitor: led=141, touch=389, monitor=475, queue_used=0, queue_free=16
DHT11: humidity=44.0%, temperature=28.9C
monitor: led=141, touch=389, monitor=475, queue_used=0, queue_free=16
monitor: led=141, touch=389, monitor=475, queue_used=0, queue_free=16
DHT11: humidity=42.0%, temperature=28.5C
monitor: led=141, touch=389, monitor=475, queue_used=0, queue_free=16
monitor: led=141, touch=389, monitor=475, queue_used=0, queue_free=16
DHT11: humidity=41.0%, temperature=28.9C
```

验证结论：

1. DHT11任务成功创建并持续运行；
2. 采样周期约为2 s；
3. 温湿度数据连续变化，通信和校验流程有效；
4. 触摸任务和监控任务仍然正常运行；
5. 触摸队列没有积压，当前为`queue_used=0`、`queue_free=16`；
6. 任务栈水位稳定，暂未发现栈溢出迹象。

## 七、当前限制与后续改进

1. DHT11读取阶段暂时关闭中断，以保证几十微秒级采样时序；读取过程约持续数毫秒，后续可改为定时器输入捕获或边沿中断测量；
2. 当前DHT11任务只输出日志，还没有写入统一的应用共享数据结构；
3. 后续需要建立`bsp_app_data`，由DHT11任务写入，Modbus和LVGL作为消费者读取；
4. 若实际器件确认为标准DHT11，通常小数位应为0；当前日志出现温度小数位非0，后续可再次核对传感器型号和模块资料。

## 八、Day 05 结论

Day 05完成了DHT11从底层GPIO时序到FreeRTOS周期采样任务的完整闭环。当前工程已经具备“传感器任务采集数据、日志模块输出、系统监控任务观察资源状态”的基础架构。下一步进入ADC采集与共享数据层，为Modbus寄存器和LVGL仪表盘提供统一数据源。
