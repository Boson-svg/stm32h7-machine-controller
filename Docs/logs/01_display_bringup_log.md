# 复刻日志 · 第一阶段：从空工程到屏幕点亮

> 记录时间：2026-08-17
> 项目：复刻 stm32h7-machine-controller（STM32H747XIH6 双核工业控制器）
> 阶段目标：打通「环境 → 双核启动 → 外扩 SDRAM → 串口调试 → LTDC 点屏」这条主线
> 屏幕型号：WKS70WSV078-WCT（7 寸 1024×600 RGB 电容触摸屏）
> SDRAM 型号：IS42S32800J（256Mbit = 32MB，FMC Bank2）

---

## 〇、本阶段总览

本阶段完成了从「一个空的双核 Template 工程」到「屏幕显示纯红色」的完整链路搭建，涉及四大外设子系统。整个过程遵循一条原则：**先让硬件在裸机上跑通，再进入 RTOS**。

最终验证结果：CM7 与 CM4 双核各自点灯、SDRAM 32MB 读写自检通过、串口可打印调试信息、LTDC 驱动 RGB 屏显示纯红色。

---

## 一、环境搭建与工程基础

### 1.1 工具链确认

在开始写代码前，先确认了编译、烧录、调试三套工具全部就绪：

| 工具 | 路径 | 用途 |
|---|---|---|
| arm-none-eabi-gcc 15.2.1 | `C:/Users/tbbbs/AppData/Roaming/xPacks/.../arm-none-eabi-gcc` | 交叉编译 |
| make / mingw32-make | `C:/mingw64/bin/` | 构建 |
| JLink V9.62 | `D:/JLink_V962/` | 烧录 + 调试（SWD）|

关键点：**Makefile 工程靠 `make` 构建，JLink 负责烧录和调试，全流程走命令行 + VSCode，不依赖 Keil/IAR 商业 IDE。**

### 1.2 工程结构设计

复刻工程采用与原项目一致的分层架构：

```
Template/
├── CM7/Core/          # Cortex-M7 主核（跑主业务）
├── CM4/Core/          # Cortex-M4 从核
├── Common/            # 双核共享代码（启动同步）
├── BSP/               # 板级支持包（按外设模块划分）
│   ├── SDRAM/
│   ├── MPU/
│   ├── LCD/
│   └── ...（预留）
├── Drivers/           # ST 官方 HAL（不动）
├── Makefile/          # 双核独立 Makefile
└── Docs/              # 本文档所在，面试材料
```

**分层原则（面试可讲）**：应用层 → BSP 层 → HAL 驱动层，单向依赖；双核各自独立编译、独立烧录、独立链接。

### 1.3 修复的工程 Bug

**Bug：CM4 内核编译参数错误**

Template 的 `Makefile/CM4/Makefile` 里，`CPU = -mcpu=cortex-m7`（写成了 M7，应为 M4）。这个错误不会报编译错、能正常链接，但会用 M7 指令集编译 M4 内核代码，在浮点运算和指令层面埋雷，双核运行时可能偶发诡异问题。

**修复**：改成 `-mcpu=cortex-m4`。

**教训**：Makefile 的三项 `CPU / FPU / FLOAT-ABI` 是隐蔽错误的温床，遇到"编译过但跑飞"要优先查这三项。

---

## 二、双核启动（CM7 + CM4）

### 2.1 STM32H747 双核架构

H747 内部是两颗物理独立的内核：

| 内核 | Flash 起始地址 | 角色 |
|---|---|---|
| CM7 | 0x08000000 | 主核，跑主业务 |
| CM4 | 0x08100000 | 从核，跑轻量实时任务 |

两核各自有独立的 `main.c`、独立的启动文件（startup）、独立的链接脚本，本质是"同一芯片里的两台独立 MCU"，通过 HSEM（硬件信号量）+ 共享内存通信。

### 2.2 HSEM 握手启动时序

双核启动的同步过程（面试重点）：

1. **CM4 先上电**，进入 `main` 后立即执行 `HAL_PWREx_EnterSTOPMode(PWR_STOPENTRY_WFE, PWR_D2_DOMAIN)`，D2 域进入 STOP 模式，CM4 深睡等待唤醒；
2. **CM7 上电**，`SystemClock_Config()` 配好系统时钟；
3. CM7 调用 `HAL_RCCEx_EnableBootCore(RCC_BOOT_C2)` 显式启动 CM4 内核；
4. CM7 通过 HSEM：`HAL_HSEM_FastTake(HSEM_ID_0)` → `HAL_HSEM_Release(HSEM_ID_0, 0)`，用硬件信号量通知 CM4；
5. CM4 被 HSEM 中断唤醒，继续执行。

**关键坑**：CM7 的 main 里必须有 `HAL_RCCEx_EnableBootCore(RCC_BOOT_C2)` 这行。如果板子的 option bytes 没有把 CM4 设为"上电自动启动"，缺了这行 CM4 永远不会被唤醒，双核点灯会退化成单核。

### 2.3 双核点灯验证

- CM7：`while(1)` 里 `HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_3)` + `HAL_Delay(500)`；
- CM4：`while(1)` 里 `HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1)` + `HAL_Delay(200)`。

两核各自点各自的 LED，频率不同（500ms vs 200ms），两灯各自闪烁 = 双核都跑起来了，HSEM 握手成功。

---

## 三、外扩 SDRAM（IS42S32800J）

### 3.1 为什么需要 SDRAM

STM32H747 内部 SRAM 约 1MB，而一块 1024×600×RGB565 的屏幕，**一帧画面就要 1024×600×2 = 1.2MB**，放不下。所以必须外扩 SDRAM 作为 LTDC 的帧缓冲。

### 3.2 芯片结构（从 datasheet 第一页读懂）

IS42S32800J 是 256Mbit 同步 DRAM，组织结构：

```
2M × 32bit × 4 bank = 8M × 32bit = 256Mbit = 32MB
```

| 参数 | 值 | 地址线 |
|---|---|---|
| 行地址（Row） | 4096 行 | A0~A11（12 位）|
| 列地址（Column） | 512 列 | A0~A8（9 位）|
| 内部 Bank | 4 个 | BA0~BA1 |

**推导链**：256Mbit ÷ 32bit = 8M 字 → ÷ 4 bank = 2M 字/bank = 4096 行 × 512 列。

### 3.3 FMC 配置（CubeMX）

关键参数（对应 datasheet 的 Address Table）：

| CubeMX 选项 | 值 | 说明 |
|---|---|---|
| SDRAM Bank | Bank 2（SDCKE1+SDNE1）| 映射到 0xD0000000 |
| Address（行地址） | 12 bits | A0~A11 |
| Data（数据宽度） | 32 bits | D0~D31 |
| Column bits | 9 | 512 列 |
| CAS Latency | 2 | 对照时钟频率表 |
| SDClockPeriod | 2 | SDCLK = ker_ck/2 = 120MHz |

**关键坑：片选/时钟使能引脚必须改成 PH6/PH7**

CubeMX 默认 SDNE1=PB6、SDCKE1=PB5，但板子实际焊接在 PH6/PH7。不改的话 CPU 以为在跟 SDRAM 通信，实际信号根本没接到芯片上，SDRAM 完全读不到数据。

### 3.4 上电初始化时序（核心，面试必考）

SDRAM 是易失性 DRAM，上电后必须按顺序初始化才能读写：

```
1. 时钟使能（CLK_ENABLE）    → 等 SDRAM 时钟稳定
2. 预充电（PALL）            → 把所有 bank 复位到空闲
3. 自动刷新（AutoRefresh ×8）→ 给电容充电
4. 加载模式寄存器（Load Mode）→ 设定 CAS=2、突发长度=8
```

对应代码（原项目 `sdram.c`）：

```c
sdram_send_cmd(1, FMC_SDRAM_CMD_CLK_ENABLE, 1, 0);
HAL_Delay(1);
sdram_send_cmd(1, FMC_SDRAM_CMD_PALL, 1, 0);
sdram_send_cmd(1, FMC_SDRAM_CMD_AUTOREFRESH_MODE, 8, 0);
// 组装模式寄存器值，发送 LOAD_MODE 命令
```

### 3.5 模式寄存器（12 位，从 datasheet 逐位查）

```
A2~A0  = Burst Length    → BURST_LENGTH_1 (000)
A3     = Burst Type      → SEQUENTIAL (0)
A6~A4  = CAS Latency     → CAS_LATENCY_2 (010)
A8~A7  = Operating Mode  → STANDARD (00)
A9     = Write Burst     → SINGLE (1)
```

最终值 = `0x0220`。**关键约束**：模式寄存器的 CAS Latency 必须和 FMC 控制器侧的 `hsdram2.Init.CASLatency` 严格一致，否则"芯片延迟 2 个时钟出数据，控制器却等 3 个时钟来读"，直接读到乱码。

### 3.6 读写自检（sdram_test 原理）

核心思想：**写递增序列 → 读回核对 → 出错即停**。

```c
// 写阶段：每隔 16KB 采样，写 0,1,2,3...
*(volatile uint32_t *)(0xD0000000 + i) = temp++;

// 读阶段：读回核对是否递增
temp = *(volatile uint32_t *)(0xD0000000 + i);
if (temp <= sval) break;  // 没递增 = SDRAM 坏了
```

**语法要点**：`*(volatile uint32_t *)地址 = 值` 是"把一个内存地址当 32 位变量读写"的标准写法。`volatile` 告诉编译器"每次都要真去读内存，不许优化"。

**验证结果**：Watch 窗口看 `*(uint32_t*)0xD0000000` = 0、`0xD0004000` = 1、`0xD0008000` = 2，递增正确，32MB 读写正常。

---

## 四、MPU 内存保护（易被忽略但关键）

### 4.1 两个 MPU 函数

| 函数 | 来源 | 作用 |
|---|---|---|
| `MPU_Config()` | CubeMX 自动生成 | region 0 = 整个 4GB 禁止访问 |
| `mpu_memory_protection()` | 原项目 BSP | region 1~7 给各内存开窗 |

**CubeMX 对 H7 的特殊行为**：只要新建 H7 工程，CubeMX 就自动生成 `MPU_Config()`（不需要你在界面配），这是 ST 为了防止 H7 缓存一致性问题的默认兜底。

### 4.2 为什么 SDRAM 必须配 MPU

`MPU_Config()` 把整个 4GB 设成"禁止访问"，SDRAM（0xD0000000）也在被禁范围内。如果不跑 `mpu_memory_protection()` 给 SDRAM 开窗，CPU 一访问 SDRAM 就 HardFault。

`mpu_memory_protection()` 里 region 6 的配置：

```c
mpu_set_protection(0xD0000000U, MPU_REGION_SIZE_32MB, MPU_REGION_NUMBER6, 0,
                   MPU_REGION_FULL_ACCESS, MPU_ACCESS_NOT_SHAREABLE,
                   MPU_ACCESS_CACHEABLE, MPU_ACCESS_BUFFERABLE);
```

**经典坑**：SDRAM 配成 cacheable+bufferable 时，CPU 写的数据会先进 D-Cache，LTDC 硬件读不到最新数据。所以帧缓冲区域要么配成"不可缓存"，要么写完后手动 Clean D-Cache。

---

## 五、串口调试（USART1 + printf 重定向）

### 5.1 为什么先配串口

LTDC 点屏是最容易黑屏的一步，黑屏时"眼睛"就是串口。配好串口后，可以在 `lcd_init()` 前后打 `printf`，一秒定位卡在哪个函数。

### 5.2 printf 重定向原理

```
printf("hello")  →  _write(fd, ptr, len)  →  __io_putchar(ch)  →  HAL_UART_Transmit(&huart1, &ch, 1, ...)
```

用户不需要改 printf，只需在链路最后一环 `__io_putchar` 里填"把字符发给串口"。

```c
int __io_putchar(int ch)
{
    uint8_t c = (uint8_t)ch;
    if (ch == '\n') {
        uint8_t cr = '\r';
        HAL_UART_Transmit(&huart1, &cr, 1, 100);  // 补发 \r
    }
    HAL_UART_Transmit(&huart1, &c, 1, 100);
    return ch;
}
```

### 5.3 两个关键坑

1. **`\n` 换行问题**：Windows 串口助手需要 `\r\n` 才是回车换行，`__io_putchar` 里遇 `\n` 要补发 `\r`；
2. **新文件必须加进 Makefile**：写了 `retarget.c` 不等于它会编译，必须列进 `C_SOURCES`，否则 printf 会跳到 `__io_putchar` 的空声明（weak 空指针），触发 HardFault。

---

## 六、LTDC 点屏（1024×600 RGB）

### 6.1 显示链路

```
LTDC 控制器 ──RGB 数据线──► 屏幕
    │
    ├─ 帧缓冲放 SDRAM @ 0xD0000000
    ├─ DMA2D 做颜色填充加速
    └─ 像素时钟来自 PLL3 = 51.2MHz
```

LTDC 是"自动刷屏引擎"：把像素写进 SDRAM 帧缓冲，LTDC 会硬件自动按 1024×600 时序不停刷到屏幕，CPU 不用管。

### 6.2 LTDC 配置分三层

**① 时序层（Display）**：1024×600 的 6 个时序参数（对照屏幕 datasheet）：

| 参数 | 值 |
|---|---|
| Active Width/Height | 1024 / 600 |
| HSYNC / VSYNC | 20 / 3 |
| HBP / VBP | 140 / 20 |
| HFP / VFP | 160 / 12 |

**② 引脚层**：RGB565 的 16 根数据线（R3~R7、G2~G7、B3~B7）+ 5 根同步线（HSYNC/VSYNC/CLK/DE），映射到 PJ/PK/PI 组。

**③ 层（Layer 0）**：像素格式 RGB565、帧缓冲地址 0xD0000000、窗口 1024×600、Alpha 255。

### 6.3 PLL3 像素时钟

HSE 25MHz ÷ M25 = 1MHz → × N256 = 256MHz → ÷ R5 = 51.2MHz，作为 LTDC 像素时钟。

### 6.4 关键坑：GPIO 速度必须 VERY_HIGH（本次黑屏的根因）

LTDC 引脚 `GPIO_InitStruct.Speed` 默认是 `GPIO_SPEED_FREQ_LOW`。但像素时钟 51.2MHz，RGB 数据线要以这个频率翻转，LOW 速度的驱动能力（slew rate）太低，信号边沿爬升太慢，屏幕收不到有效视频信号 → 纯黑。

**修复**：改成 `GPIO_SPEED_FREQ_VERY_HIGH`（并在 CubeMX 的 GPIO 表格页从根源改，避免下次 Generate 被覆盖回 LOW）。

### 6.5 屏幕三个控制引脚（不在 LTDC 外设里）

| 引脚 | 功能 |
|---|---|
| PI11 | 屏供电（5V）|
| PH5 | 复位 |
| PB0 | 背光 |

这三个是普通 GPIO，CubeMX 不会自动配，需手动初始化，并按「供电 → 复位 → 背光」的顺序拉高。

### 6.6 上电时序

```c
LCD_PWREN(1);   // 供电开
LCD_RST(1); HAL_Delay(10); LCD_RST(0); HAL_Delay(50); LCD_RST(1); HAL_Delay(200);  // 复位脉冲
LCD_BL(1);      // 背光开
```

---

## 七、排查方法论沉淀（最重要的收获）

### 7.1 定位硬件坑的正确姿势

1. **拿原项目代码硬对比**：你的配置 vs 原项目配置，逐项 diff；
2. **查 HAL 源码**：不猜 HAL 函数行为，直接读 `stm32h7xx_hal_*.c` 看它到底怎么写的；
3. **读寄存器**：黑屏时用 JLink 读 LTDC_GCR / SRCR / L1CR 等寄存器，直接看哪一位没置位；
4. **不要凭印象猜**：本次两次误判（缓存、Layer 1）就是教训，最后靠硬对比才找到 GPIO speed 根因。

### 7.2 CubeMX 复刻的三个铁律

1. **自定义代码放 USER CODE 块**：CubeMX Generate 会重置块外代码（包括 includes）；
2. **新建 .c 必须加进 Makefile**：否则不参与编译；
3. **生成后逐项核对**：引脚映射、时序、Layer、GPIO speed，CubeMX 默认值 ≠ 板子实际值。

---

## 八、下一步计划

1. 搬正式 LCD 驱动（`ltdc_fill` / `ltdc_clear`，DMA2D 硬件填充），替换手写 for 循环清屏；
2. 搬 `ltdc_gpio_init()`（背光/复位/供电正式封装）；
3. 配触摸（软件 I2C + FT5446U/FT5x06 兼容驱动），串口打印坐标；
4. 之后进入 FreeRTOS + Modbus。

---

## 附录：关键地址速查

| 地址 | 用途 |
|---|---|
| 0x08000000 | CM7 Flash |
| 0x08100000 | CM4 Flash |
| 0xC0000000 | FMC SDRAM Bank1 |
| 0xD0000000 | FMC SDRAM Bank2（LTDC 帧缓冲）|
