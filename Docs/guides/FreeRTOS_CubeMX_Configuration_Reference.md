# STM32H747 + FreeRTOS + CubeMX 配置参考

> 适用工程：E:\STM32H7\Template  
> 主控：STM32H747XIH6；CM7 运行主业务，CM4 暂不运行 FreeRTOS。  
> 最后更新：2026-08-19

## 1. 文档使用说明

本文将 FreeRTOS 配置整理为“参数—作用—参考当前设置—本项目推荐设置”的形式，方便在 CubeMX 配置时逐项核对。

“参考当前设置”来自本次整理的参数表，不代表已经全部写入或核对过本项目的 .ioc。真正修改前，应以 CubeMX 界面、当前 .ioc 和生成的 freertos.c/freertos.h 为准。

“Day03 推荐”只服务于第一阶段的最小验证：在 CM7 上运行 LED 任务和串口任务，先证明调度器、Tick、任务栈和 Heap 正常，再逐步加入触摸、显示和通信任务。

当前工程已经验证：

- CM7/CM4 双核启动；
- SDRAM、MPU、LTDC、DMA2D；
- RGB565 屏幕显示；
- 软件 I2C；
- FT5446U 的 FT5x06 兼容触摸驱动；
- USART1 printf 重定向。

因此，FreeRTOS 移植阶段先保持这些裸机驱动不变，只增加最小任务系统。

---

## 2. 本项目的总体推荐

| 配置项目 | Day03 最小验证 | 后续项目建议 | 说明 |
|---|---:|---:|---|
| 运行核心 | CM7 | CM7 | CM4 暂不加入 FreeRTOS |
| 调度方式 | 抢占式 | 抢占式 | 高优先级任务可抢占低优先级任务 |
| Tick 频率 | 1000 Hz | 1000 Hz | 1 Tick 约为 1 ms |
| 最大任务优先级 | 8 | 8~16 | 按实际任务数扩展，不盲目使用 56 |
| Tick 类型 | 32 位 | 32 位 | STM32H7 使用 32 位 Tick |
| 内存管理 | Dynamic + heap_4 | Dynamic + heap_4 | 适合任务、队列、信号量 |
| FreeRTOS Heap | 32 KB 起步 | 32~64 KB 起步 | 根据最小剩余 Heap 调整 |
| 栈溢出检测 | Level 2 | Level 2 | 调试阶段打开 |
| malloc 失败钩子 | Enabled | Enabled | 及时定位内存不足 |
| 软件定时器 | 可开启 | Enabled | 后续可用于消抖和超时 |
| 互斥锁 | Enabled | Enabled | 保护共享外设 |
| 递归互斥锁 | Disabled | 按需开启 | 无递归加锁需求时关闭 |
| 计数信号量 | Enabled | Enabled | 资源计数和事件计数 |
| FreeRTOS MPU | Disabled | Disabled | 与工程 BSP/MPU 不是一回事 |
| FPU | 与编译器/端口配置一致 | 核对后启用 | 不是简单打开 MCU 时钟 |
| 运行时间统计 | Disabled | 稳定后再开启 | 需要额外高精度计时基准 |

---

## 3. Kernel settings（内核设置）

| 参数 | 参考当前设置 | 作用 | Day03 推荐 | 说明 |
|---|---|---|---|---|
| **USE_PREEMPTION** | Enabled | 是否使用抢占式调度 | Enabled | 适合显示、通信、触摸等任务 |
| **CPU_CLOCK_HZ** | SystemCoreClock | CPU 主频，用于 Tick 或端口计算 | SystemCoreClock | 不建议写死频率 |
| **TICK_RATE_HZ** | 1000 | 系统节拍频率 | 1000 | 1000 Hz 对应 1 ms Tick |
| **MAX_PRIORITIES** | 56 | 任务优先级数量 | 初始 8 | 范围为 0 到 MAX_PRIORITIES-1 |
| **USE_SB_COMPLETED_CALLBACK** | 0 | Stream Buffer 完成回调 | 保持默认 | 当前没有使用 Stream Buffer |
| **USE_MINI_LIST_ITEM** | 1 | 内核链表优化选项 | 保持默认 | 属于内核实现细节 |
| **MINIMAL_STACK_SIZE** | 128 Words | Idle 任务最小栈 | 128~256 Words | Words 不是字节，128 Words 通常为 512 字节 |
| **MAX_TASK_NAME_LEN** | 16 | 任务名最大长度 | 16 | 足够调试和日志显示 |
| **USE_16_BIT_TICKS** | Disabled | 是否使用 16 位 Tick | Disabled | 使用 32 位 Tick |
| **IDLE_SHOULD_YIELD** | Enabled | Idle 是否主动让出 CPU | Enabled | 保持默认 |

### Tick 和延时

~~~text
TICK_RATE_HZ = 1000
1 Tick       = 1 ms
osDelay(1)   ≈ 1 ms
osDelay(100) ≈ 100 ms
~~~

Tick 越高，时间分辨率越高，但 Tick 中断也越频繁。1000 Hz 适合作为本项目触摸屏和 GUI 的初始设置。

注意：

- osDelay 只能在任务上下文中调用；
- 中断中不能调用 osDelay；
- 软件 I2C 的 SCL/SDA 位时序不能使用 osDelay；
- 软件 I2C 仍使用微秒级忙等待，触摸任务只控制扫描周期。

---

## 4. MPU/FPU 设置

| 参数 | 参考当前设置 | 作用 | 本项目建议 |
|---|---|---|---|
| **ENABLE_MPU** | Disabled | 启用 FreeRTOS MPU port 和受保护任务 | Disabled |
| **ENABLE_FPU** | Disabled | 按 FPU 场景配置任务上下文 | 与编译器和生成端口核对 |

工程已有的 MPU 文件：

~~~text
E:\STM32H7\Template\BSP\MPU\mpu.c
E:\STM32H7\Template\BSP\MPU\mpu.h
~~~

工程 BSP/MPU 用于配置 SDRAM 等内存区域的 Cache、Buffer 和访问属性；FreeRTOS MPU 用于限制不同任务访问哪些内存区域。两者目的不同，Day03 关闭 FreeRTOS MPU 不会影响已有 SDRAM MPU。

当前 Makefile 已使用：

~~~text
-mfpu=fpv5-d16
-mfloat-abi=hard
~~~

所以 FPU 必须同时核对编译器参数、启动代码和 FreeRTOS Cortex-M7 端口。LED 和串口任务不依赖浮点运算，先保证调度器稳定，再根据 LVGL、图像算法等需求处理 FPU。

---

## 5. Task Notifications（任务通知）

| 参数 | 参考当前设置 | 作用 | Day03 推荐 |
|---|---|---|---|
| **USE_TASK_NOTIFICATIONS** | Enabled | 轻量级任务通知机制 | Enabled |
| **RECORD_STACK_HIGH_ADDRESS** | Disabled | 记录任务栈高地址，便于调试 | 调试阶段可 Enabled |

任务通知适合 UART 接收完成、DMA 完成、触摸事件和 CM7 内部的一对一通知。共享资源应使用互斥锁，多个事件或计数资源应根据场景使用队列、信号量或事件组。

---

## 6. Memory management（内存管理）

| 参数 | 参考当前设置 | 作用 | Day03 推荐 |
|---|---|---|---|
| Memory Allocation | Dynamic / Static | 内核对象使用动态还是静态分配 | Dynamic |
| **TOTAL_HEAP_SIZE** | 15360 Bytes | FreeRTOS 动态内存池大小 | 32768 Bytes 起步 |
| **HEAP_CLEAR_MEMORY_ON_FREE** | 0 | 释放内存时是否清零 | 0 |
| Memory Management Scheme | heap_4 | Heap 实现 | heap_4 |

### Heap 方案

| 方案 | 特点 | 本项目评价 |
|---|---|---|
| heap_1 | 只能申请，不能释放 | 不适合当前项目 |
| heap_2 | 可释放，但碎片合并能力较弱 | 一般不选 |
| heap_3 | 包装 C 库 malloc/free | 控制力较弱 |
| **heap_4** | 支持释放并合并相邻空闲块 | 当前最适合 |
| heap_5 | 支持多个不连续内存区域 | 多内存区时再考虑 |

32 KB 是起步建议，不是固定答案。后续任务、队列、信号量、网络协议栈和 LVGL 加入后，需要根据最低剩余 Heap 调整。

调试时可观察：

~~~c
size_t free_heap = xPortGetFreeHeapSize();
size_t min_heap  = xPortGetMinimumEverFreeHeapSize();
~~~

FreeRTOS Heap 用于任务和内核对象；LTDC 帧缓冲区位于 SDRAM。DMA2D/LTDC 缓冲区还要考虑 DMA 可访问性、D-Cache、MPU 属性和链接脚本，不能简单地全部放进 FreeRTOS Heap。

---

## 7. Hook function（钩子函数）

| 参数 | 参考当前设置 | 作用 | Day03 推荐 |
|---|---|---|---|
| **USE_IDLE_HOOK** | Disabled | Idle 任务运行时回调 | Disabled |
| **USE_TICK_HOOK** | Disabled | 每个 Tick 回调 | Disabled |
| **USE_MALLOC_FAILED_HOOK** | Disabled | malloc 失败时回调 | Enabled |
| **USE_DAEMON_TASK_STARTUP_HOOK** | Disabled | Timer Service 启动回调 | Disabled |
| **CHECK_FOR_STACK_OVERFLOW** | Disabled | 检测任务栈溢出 | Level 2 |

建议：

~~~c
#define configCHECK_FOR_STACK_OVERFLOW 2
~~~

Level 2 的检测能力比 Level 1 更强，但不能替代合理的任务栈大小。调试阶段可以实现 vApplicationStackOverflowHook 和 vApplicationMallocFailedHook，在钩子中停止系统并观察调用现场。

初期不建议使用 Tick Hook 塞入业务逻辑。触摸扫描、串口解析和显示刷新应放在普通任务中，通过延时、队列或通知调度。

---

## 8. Runtime statistics（运行统计）

| 参数 | 参考当前设置 | 作用 | Day03 推荐 |
|---|---|---|---|
| **GENERATE_RUN_TIME_STATS** | Disabled | 统计任务 CPU 占用率 | Disabled |
| **USE_TRACE_FACILITY** | Enabled | 支持查询任务状态 | 可保持 Enabled |
| **USE_STATS_FORMATTING_FUNCTIONS** | Disabled | 生成格式化统计字符串 | Disabled |

Day03 先不启用运行时间统计。它通常需要额外的高精度计时器、统计宏和资源验证。待 LED、串口和触摸任务稳定后，再使用统计功能分析 CPU 占用率。

---

## 9. Co-routine（协程）

| 参数 | 参考当前设置 | Day03 推荐 |
|---|---|---|
| **USE_CO_ROUTINES** | Disabled | Disabled |
| **MAX_CO_ROUTINE_PRIORITIES** | 2 | 保持默认 |

当前项目使用普通任务、队列、通知和信号量，不需要启用 FreeRTOS 协程。

---

## 10. Software Timer（软件定时器）

| 参数 | 参考当前设置 | Day03 推荐 | 说明 |
|---|---|---|---|
| **USE_TIMERS** | Enabled | Enabled 或按需启用 | 可用于消抖、超时和周期事件 |
| **TIMER_TASK_PRIORITY** | 2 | 2 | 不高于关键实时任务 |
| **TIMER_QUEUE_LENGTH** | 10 | 10~20 | 命令多时再增加 |
| **TIMER_TASK_STACK_DEPTH** | 256 Words | 128~256 Words | 回调复杂时再增加 |

软件定时器回调运行在 Timer Service 任务中，不是独立任务。回调函数应短小，不能执行长循环、阻塞式 I2C 或大块图像处理。

---

## 11. Mutex / Semaphore（互斥锁和信号量）

| 参数 | 参考当前设置 | Day03 推荐 | 使用场景 |
|---|---|---|---|
| **USE_MUTEXES** | Enabled | Enabled | 保护 UART、I2C、显示资源 |
| **USE_RECURSIVE_MUTEXES** | Enabled | Disabled，按需开启 | 同一任务递归加锁时才需要 |
| **USE_COUNTING_SEMAPHORES** | Enabled | Enabled | 资源计数或事件计数 |

互斥锁具有优先级继承机制，适合保护共享资源；二值信号量适合事件通知；计数信号量适合累计事件或多个相同资源。

触摸和显示的初期版本：

- 先保持触摸轮询；
- 不在软件 I2C 每一位的时序中加锁；
- 多任务访问同一条 I2C 总线时，在完整事务外层加锁；
- 不要在持有互斥锁时执行长时间 osDelay。

---

## 12. STM32 中断优先级

| 参数 | 参考当前设置 | 作用 | 本项目建议 |
|---|---:|---|---:|
| **LIBRARY_LOWEST_INTERRUPT_PRIORITY** | 15 | 最低中断优先级 | 15 |
| **LIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY** | 5 | 可以调用 FreeRTOS FromISR API 的最高中断优先级 | 5 |

STM32 NVIC 通常是数字越小优先级越高：

~~~text
0    最高
15   最低
~~~

当最高可调用系统 API 的优先级为 5 时：

~~~text
优先级 0~4：不能调用 FreeRTOS API
优先级 5~15：可以调用 FromISR 版本 API
~~~

中断中必须使用 FromISR 版本 API，不能调用 osDelay 或 vTaskDelay。DMA、UART、EXTI、触摸 INT 等中断以后接入 FreeRTOS 时，要重新核对优先级。

---

## 13. CubeMX 配置顺序

### Day03 最小配置

1. 打开 E:\STM32H7\Template\Template.ioc。
2. 确认运行核心为 CM7。
3. 只在 CM7 启用 FreeRTOS。
4. CM4 不启用 FreeRTOS，不修改现有 CM4 LED 代码。
5. 创建一个默认任务，例如 StartDefaultTask。
6. 先保持默认 Tick 配置，不额外切换定时器。
7. 选择 heap_4，Heap 设置为 32 KB 起步。
8. 开启栈溢出检测 Level 2。
9. 开启 malloc 失败钩子。
10. 保留抢占式调度和 1000 Hz Tick。
11. 生成后检查 USER CODE BEGIN/END 区域。

### 生成后检查文件

~~~text
E:\STM32H7\Template\CM7\Core\Inc\freertos.h
E:\STM32H7\Template\CM7\Core\Src\freertos.c
E:\STM32H7\Template\CM7\Core\Inc\FreeRTOSConfig.h
E:\STM32H7\Template\CM7\Core\Src\main.c
E:\STM32H7\Template\Makefile\CM7\Makefile
~~~

不同 CubeMX 版本可能把 FreeRTOSConfig 放在不同目录，最终以生成结果为准。重点确认 FreeRTOS 源文件已经进入 CM7 Makefile、include path 已加入、main 先初始化硬件再调用 MX_FREERTOS_Init，最后调用 osKernelStart。

---

## 14. Day03 最小任务设计

第一版只创建两个任务：

| 任务 | 建议优先级 | 周期 | 目的 |
|---|---:|---:|---|
| LedTask | 低 | 500 ms | 验证调度和任务延时 |
| UartTask | 普通 | 1000 ms | 验证任务运行、printf 和串口 |

任务内可以使用：

~~~c
osDelay(500);
~~~

或：

~~~c
vTaskDelay(pdMS_TO_TICKS(500));
~~~

暂时不要同时加入 FT5206 扫描、软件 I2C 时序替换、LTDC 大面积刷屏、DMA2D 长传输、Modbus 或 W5500。先让最小调度器稳定，再逐模块接入。

---

## 15. 与现有外设的边界

### 软件 I2C

正确分层：

~~~text
FreeRTOS 触摸任务
    │
    ├── 每隔 10~20 ms 调用一次 ft5206_scan
    │
    └── ctiic.c 内部继续使用微秒级忙等待
~~~

osDelay 只控制两次扫描之间的周期，不能替换 SCL 的位时序。

### LTDC 和 DMA2D

- 不要让多个任务同时修改同一个帧缓冲区；
- 大面积刷屏不要放进高优先级任务；
- 后续使用 Cache 时处理 DMA2D/LTDC 与 D-Cache 一致性；
- 显示任务和触摸任务之间优先考虑消息队列或事件通知。

### printf

如果 printf 仍使用阻塞式 HAL_UART_Transmit，高频打印会阻塞任务。Day03 只做少量日志验证，后续逐步改成环形缓冲区、UART DMA 和独立日志任务。

---

## 16. 验证清单

### 编译

~~~powershell
cd E:\STM32H7\Template\Makefile
C:\mingw64\bin\mingw32-make.exe all
~~~

通过标准：

- CM4、CM7 编译成功；
- 没有 implicit declaration；
- 没有 FreeRTOS 头文件缺失或重复定义；
- 生成 CM7 的 ELF、HEX、BIN。

### 运行

烧录前先完成代码检查。烧录后验证：

| 验收项 | 预期结果 |
|---|---|
| CM4 LED | 保持原有运行状态 |
| CM7 LED | 按任务周期闪烁 |
| USART1 | 周期输出任务日志 |
| 屏幕 | 初始化后仍显示原来的颜色 |
| 触摸 | 初始阶段保持原裸机轮询 |
| 系统状态 | 不进入 HardFault，不随机卡死 |

调试时可观察 uxSchedulerRunning、当前任务、任务栈剩余量、xPortGetFreeHeapSize 和 xPortGetMinimumEverFreeHeapSize。

---

## 17. 常见误区

### 误区 1：FreeRTOS 优先级和 NVIC 优先级相同

不是：

~~~text
FreeRTOS 任务优先级：数值越大，任务优先级越高
STM32 NVIC 优先级：数值越小，中断优先级越高
~~~

### 误区 2：软件 I2C 延时改成 osDelay

不能。osDelay 是 Tick 级延时，无法满足 I2C 位时序。软件 I2C 继续使用微秒忙等待。

### 误区 3：FreeRTOS MPU 等于工程 MPU

不是。BSP/MPU 配置 SDRAM 等内存属性；FreeRTOS MPU 限制任务访问权限。

### 误区 4：Heap 越大越好

Heap 太小会分配失败，太大则可能挤占其他 RAM。应根据最低剩余 Heap 调整。

### 误区 5：所有业务函数都放高优先级任务

高优先级任务长时间运行会影响其他任务。任务应短小、可阻塞、按周期运行。

### 误区 6：CubeMX 生成后覆盖用户代码

自定义 include、任务调用和业务逻辑放在 USER CODE BEGIN/END 区域，或放到独立用户源文件并加入 Makefile。

---

## 18. 后续任务顺序

~~~text
最小 LED/UART 任务
        │
        ▼
触摸轮询任务
        │
        ▼
触摸数据消息队列
        │
        ▼
显示任务 + DMA2D
        │
        ▼
日志任务
        │
        ▼
Modbus RTU 任务
~~~

当前不直接进入 LVGL、W5500 或 OTA。先把任务、延时、队列、互斥锁和错误处理基础打牢。

---

## 19. 参考路径

本项目：

~~~text
E:\STM32H7\Template\Template.ioc
E:\STM32H7\Template\CM7\Core\Src\main.c
E:\STM32H7\Template\CM7\Core\Src\ltdc.c
E:\STM32H7\Template\CM7\Core\Src\dma2d.c
E:\STM32H7\Template\BSP\MPU\mpu.c
E:\STM32H7\Template\BSP\TOUCH\ctiic.c
E:\STM32H7\Template\BSP\TOUCH\ft5206.c
E:\STM32H7\Template\Makefile\CM7\Makefile
~~~

原项目：

~~~text
E:\STM32H7\stm32h7-machine-controller\CM7\Core\Src\freertos.c
E:\STM32H7\stm32h7-machine-controller\CM7\Core\Inc\FreeRTOSConfig.h
E:\STM32H7\stm32h7-machine-controller\Middlewares\Third_Party\FreeRTOS\
~~~

> 原项目路径用于对比和学习。实际复刻时，先理解 CubeMX 生成的最小版本，再逐步对比原项目的任务划分和配置差异。
