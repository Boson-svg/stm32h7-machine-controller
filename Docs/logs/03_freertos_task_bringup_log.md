# 复刻日志 · Day 03：FreeRTOS 基础移植与任务化触摸

> 记录时间：2026-08-20  
> 项目：复刻 stm32h7-machine-controller（STM32H747XIH6 双核工业控制器）  
> 本日目标：在 CM7 上接入 FreeRTOS，手写第一个应用任务，并把已验证的触摸轮询迁移到任务中。  
> 本日结果：LED_Task、TouchTask 均已在开发板上运行正常。

---

## 一、本日成果

| 里程碑 | 状态 | 验收事实 |
|---|---|---|
| CM7 FreeRTOS 生成与接入 | 已完成 | CubeMX 生成 FreeRTOS 内核、CMSIS-RTOS V2 适配层和 CM7 移植文件 |
| FreeRTOS 编译路径 | 已完成 | Makefile 已加入 FreeRTOS 头文件、内核源文件、移植层和内存管理文件 |
| IntelliSense 配置 | 已完成 | VSCode 能够解析 `FreeRTOS.h` 等头文件 |
| AppTasks 模块 | 已完成 | 建立 `CM7/Core/App/Inc` 和 `CM7/Core/App/Src`，任务代码与 CubeMX 生成代码分离 |
| LED_Task | 已完成 | 使用原生 `xTaskCreate()` 创建，使用 `vTaskDelayUntil()` 周期翻转 PA3 |
| TouchTask | 已完成 | 使用原生 `xTaskCreate()` 创建，周期调用 `ft5206_scan()`，触摸运行正常 |
| 双任务调度 | 已完成 | LED 任务和触摸任务可以同时运行 |

本日的核心成果不是简单“打开 FreeRTOS”，而是完成了以下架构转变：

```text
裸机 main while 循环
        ↓
FreeRTOS 调度器
        ↓
LED_Task + TouchTask
```

---

## 二、FreeRTOS 配置结果

### 2.1 运行核心与任务归属

本次只在 Cortex-M7 上启用 FreeRTOS，Cortex-M4 仍然保持原来的独立裸机程序。

| 项目 | 当前配置 | 说明 |
|---|---|---|
| Runtime Core | Cortex-M7 | CM7 负责显示、触摸和主业务 |
| Power Domain | D1 | DMA2D、LTDC 等主业务外设所在域 |
| Cortex-M4 Assignment | Disabled | 本日没有让 CM4 使用同一套 FreeRTOS |
| Scheduler | FreeRTOS Kernel | CubeMX 生成内核和端口层 |
| API 入口 | CMSIS-RTOS V2 + 原生 FreeRTOS API | 内核启动由 CMSIS，应用任务使用原生 API |

### 2.2 关键 FreeRTOS 参数

配置文件：

`CM7/Core/Inc/FreeRTOSConfig.h`

| 参数 | 当前值 | 本日作用 |
|---|---:|---|
| `configUSE_PREEMPTION` | `1` | 启用抢占式调度 |
| `configTICK_RATE_HZ` | `1000` | 1 个 Tick 约等于 1 ms |
| `configMAX_PRIORITIES` | `56` | 兼容当前 CMSIS-RTOS 优先级映射 |
| `configTOTAL_HEAP_SIZE` | `15360` | 动态创建任务时使用的 FreeRTOS 堆空间 |
| `configSUPPORT_DYNAMIC_ALLOCATION` | `1` | 允许使用 `xTaskCreate()` |
| `configCHECK_FOR_STACK_OVERFLOW` | `2` | 启用栈溢出检查 |
| `configUSE_MALLOC_FAILED_HOOK` | `1` | 启用内存申请失败钩子 |
| `configGENERATE_RUN_TIME_STATS` | `0` | 暂不统计任务运行时间 |
| `configUSE_MUTEXES` | `1` | 为后续 UART 日志、共享资源保护保留能力 |

本日没有手写 FreeRTOS 内核、启动端口或 SysTick 中断，而是使用 CubeMX 生成的内核集成代码。这些文件属于系统基础设施，适合复用；应用任务和任务需求则由自己设计。

---

## 三、调度器启动流程

当前入口位于 `CM7/Core/Src/main.c`：

```text
HAL_Init()
    ↓
SystemClock_Config()
    ↓
GPIO/FMC/USART/LTDC/DMA2D 初始化
    ↓
SDRAM、LCD、FT5446U 初始化
    ↓
osKernelInitialize()
    ↓
MX_FREERTOS_Init()
    │
    ├── CubeMX 创建 defaultTask
    └── 调用 AppTasks_Init()
            ├── 创建 LED_Task
            └── 创建 TouchTask
    ↓
osKernelStart()
    ↓
FreeRTOS 接管 CPU 调度
```

`osKernelStart()` 之后，`main.c` 后面的裸机 `while (1)` 不再是主要执行路径。之后的业务逻辑必须放入任务、队列、信号量或其他 RTOS 对象中。

---

## 四、AppTasks 模块设计

### 4.1 文件结构

本日实际使用的目录是：

```text
CM7/Core/App/
├── Inc/
│   └── app_tasks.h
└── Src/
    └── app_tasks.c
```

这里没有把应用任务直接写入 CubeMX 的 `freertos.c`，而是把任务实现放到独立模块中。

### 4.2 头文件只暴露公共接口

`app_tasks.h` 只声明：

```c
void AppTasks_Init(void);
```

任务函数、任务句柄和任务私有宏均保留在 `app_tasks.c`，避免外部模块直接依赖任务内部实现。

### 4.3 CubeMX 安全接入方式

在 `freertos.c` 的用户区域中加入：

```c
/* USER CODE BEGIN Includes */
#include "app_tasks.h"
/* USER CODE END Includes */
```

在 `MX_FREERTOS_Init()` 的用户线程区域中调用：

```c
/* USER CODE BEGIN RTOS_THREADS */
AppTasks_Init();
/* USER CODE END RTOS_THREADS */
```

这样 `main.c` 仍然保持 CubeMX 的启动顺序，而自定义任务调用位于 `USER CODE` 保护区内，重新生成代码时不会被覆盖。

---

## 五、LED_Task 实现与理解

### 5.1 任务需求

| 项目 | 设计值 |
|---|---:|
| 任务名称 | `LedTask` |
| 创建 API | `xTaskCreate()` |
| 优先级 | `2` |
| 栈深度 | `256` 个 `StackType_t` 单位 |
| 周期 | `500 ms` |
| GPIO | `PA3` |
| 周期延时 | `vTaskDelayUntil()` |

### 5.2 运行机制

```text
LED_Task 被调度
    ↓
翻转 PA3
    ↓
计算下一个绝对唤醒时刻
    ↓
阻塞到下一个周期
    ↓
其他任务获得 CPU
```

使用 `vTaskDelayUntil()` 而不是简单的 `vTaskDelay()`，是为了让周期基准更加稳定。任务实际执行时间不会不断累加到下一次周期中。

### 5.3 本日遇到的链接错误

初版在头文件中声明了：

```c
extern TaskHandle_t LEDTaskHandle;
```

但没有在任何 `.c` 文件中定义实体，最终出现：

```text
undefined reference to `LEDTaskHandle'
```

随后将任务句柄改为 `app_tasks.c` 内部私有变量：

```c
static TaskHandle_t led_task_handle = NULL;
```

同时把 `static void LED_Task(void *)` 从头文件移到 `.c` 文件，解决了：

```text
'LED_Task' declared 'static' but never defined
```

这次问题说明：

- `extern` 只是声明，不会分配变量空间；
- `static` 函数声明不应该放在公共头文件中；
- 编译通过不代表链接一定成功，变量符号还要在链接阶段解析。

---

## 六、TouchTask 实现与理解

### 6.1 触摸初始化与触摸扫描的分工

`ft5206_init()` 仍然在启动调度器前执行，负责：

- 配置 RST/INT GPIO；
- 初始化软件 I2C；
- 产生触摸芯片复位时序；
- 写入工作模式、阈值和扫描周期；
- 读取版本信息。

`TouchTask` 不重复初始化触摸芯片，只负责周期调用：

```c
ft5206_scan(0U);
```

### 6.2 任务需求

| 项目 | 设计值 |
|---|---:|
| 任务名称 | `TouchTask` |
| 创建 API | `xTaskCreate()` |
| 优先级 | `3` |
| 栈深度 | `512` 个 `StackType_t` 单位 |
| 调度周期 | `1 ms` |
| 触摸方式 | 轮询 |
| 数据输出 | `tp_dev.x[0]`、`tp_dev.y[0]` |

触摸任务优先级设置为 3，高于 LED 任务的 2，因为触摸响应属于交互输入，实时性要求更高。

### 6.3 为什么任务周期是 1 ms

当前 `ft5206_scan()` 内部还保留了扫描节流逻辑，大约每 10 次函数调用才进行一次完整 I2C 读取。因此：

```text
TouchTask 1 ms × 内部 10 次计数 ≈ 10 ms 一次实际 I2C 扫描
```

如果 TouchTask 设置为 10 ms，则实际触摸读取周期可能接近 100 ms，触摸响应会变慢。

### 6.4 TouchTask 数据流

```text
TouchTask
    ↓
ft5206_scan(0U)
    ↓
软件 I2C 读取 FT5446U
    ↓
更新全局 tp_dev
    ├── tp_dev.sta
    ├── tp_dev.x[0]
    └── tp_dev.y[0]
    ↓
串口输出有效触摸坐标
    ↓
vTaskDelayUntil(1 ms)
```

本日暂时没有创建触摸消息队列，也没有把 INT 引脚接入 EXTI。这样可以先验证“已有触摸驱动能否稳定运行在 RTOS 任务中”，减少一次引入的变量。

---

## 七、TouchTask 与原裸机轮询的区别

### 原来的裸机思路

```c
while (1)
{
    ft5206_scan(0);
}
```

这个循环会独占 CPU，其他业务只能依靠中断或手动插入执行代码。

### 当前 FreeRTOS 思路

```c
for (;;)
{
    ft5206_scan(0U);
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1U));
}
```

任务扫描完成后主动阻塞，调度器可以运行 LED、日志和后续显示任务。

---

## 八、本日验证边界

### 已验证

1. CM7 FreeRTOS 内核能够启动；
2. `AppTasks_Init()` 能够创建原生 FreeRTOS 任务；
3. `LED_Task` 能够正常执行并周期翻转 LED；
4. `TouchTask` 能够正常执行；
5. TouchTask 能够调用现有 FT5446U/FT5x06 兼容驱动；
6. LED 任务和触摸任务能够同时运行；
7. 触摸任务仍然能够输出实际触摸坐标；
8. 触摸驱动仍然能够识别按下、移动、抬起。

### 尚未完成

1. 没有创建 TouchQueue，触摸数据仍然直接通过全局 `tp_dev` 使用；
2. 没有创建独立 DisplayTask；
3. 没有给 UART `printf` 增加互斥锁；
4. 没有使用 FT5446U INT 引脚和 EXTI 中断；
5. `ft5206_scan()` 仍然包含阻塞式软件 I2C 和微秒延时；
6. `defaultTask` 仍然保留，后续可以移除并完全由 `AppTasks_Init()` 管理应用任务；
7. 还没有进行长时间运行、任务栈余量和 FreeRTOS 堆余量测试。

---

## 九、本日踩坑与经验

### 9.1 Makefile 的两类路径必须同时配置

新建任务模块后需要同时加入：

```makefile
C_SOURCES:
../../CM7/Core/App/Src/app_tasks.c

C_INCLUDES:
-I../../CM7/Core/App/Inc
```

只添加 `.c` 源文件，编译器仍然可能找不到 `app_tasks.h`；只添加头文件目录，链接阶段又可能找不到任务实现。

### 9.2 CubeMX 生成代码与自定义代码分层

推荐关系：

```text
main.c
    负责硬件初始化、内核启动

freertos.c
    负责 CubeMX 的 FreeRTOS 初始化入口
    在 USER CODE 区域调用 AppTasks_Init()

app_tasks.c
    负责应用任务实现
```

不要把完整任务逻辑塞进 `freertos.c`，也不要把受 CubeMX 管理的生成区域当成永久代码区。

### 9.3 `printf` 不应成为高频任务的主要工作

当前 LED 任务只在启动时打印一次，TouchTask 只在检测到有效扫描时输出坐标。后续任务数量增加后，多个任务同时使用 `printf` 可能产生：

- 日志字符交错；
- UART 阻塞导致任务周期抖动；
- 日志过多导致调试串口难以阅读。

下一阶段需要设计统一的日志任务或 UART 互斥保护。

---

## 十、Day 03 结论

Day 03 完成了从裸机轮询到 FreeRTOS 多任务的第一步：保留 CubeMX 生成的内核和启动框架，在独立的 `AppTasks` 模块中使用原生 FreeRTOS API 创建 `LED_Task` 和 `TouchTask`。LED 任务验证了调度器和周期延时，TouchTask 验证了已有 FT5446U 触摸驱动能够在 RTOS 环境中继续工作。

本日采用“先轮询、后中断；先直接验证、后消息解耦”的策略，避免同时修改触摸驱动、任务调度和通信机制。当前已经具备继续加入队列、显示任务和 Modbus 任务的基础。

---

## 十一、下一步计划

### Day 04：任务通信与显示解耦

1. 复查并固定 `TouchTask` 的触摸事件结构；
2. 创建 `TouchEvent_t` 数据结构；
3. 创建触摸消息队列 `TouchQueue`；
4. 让 TouchTask 将坐标和按下/移动/抬起状态发送到队列；
5. 创建 DisplayTask，从队列接收触摸事件；
6. 使用屏幕颜色或简单图形响应触摸；
7. 评估 UART 日志互斥保护；
8. 编译并进行开发板运行验证。

Day 04 的目标不是立即移植 LVGL，而是先掌握“任务之间如何通过队列传递数据”，这会直接成为后续 Modbus、日志和 GUI 任务的通用架构。
