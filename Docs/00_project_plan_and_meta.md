# 项目总体规划与协作约定（每天更新）

> 本文档是新 AI 对话窗口的「启动上下文」——接手本项目前先读这里。
> 最后更新：2026-08-17

---

## 一、项目定位

- **项目**：复刻 [stm32h7-machine-controller]（STM32H747XIH6 双核工业控制器）作为秋招项目
- **目标**：一个月完成复刻，重点是「能跑、能演示、能讲透」，而不是「模块全但不深入」
- **硬件**：
  - 主控：STM32H747XIH6（双核 CM7 + CM4）
  - 屏幕：WKS70WSV078-WCT（7 寸 1024×600 RGB 电容触摸屏）
  - SDRAM：IS42S32800J（32MB，FMC Bank2）
- **工具链**：Makefile + arm-none-eabi-gcc + JLink（SWD），VSCode 开发，不依赖 Keil/IAR

---

## 二、项目具体内容（复刻对象全貌）

> 这是原项目 stm32h7-machine-controller 的完整功能清单，浮浮酱据此安排复刻路线。

### 2.1 原项目是什么

一个基于 STM32H747 双核的**工业控制器**，CM7 跑主业务（显示 + 网络 + 存储 + OTA），CM4 跑轻量实时任务，通过 HSEM/共享内存协作。核心亮点是**双核架构**和**OTA 双槽升级**。

### 2.2 功能模块全景（来自原项目 BSP 目录）

| 模块 | 文件 | 功能 |
|---|---|---|
| **SDRAM** | `BSP/SDRAM/` | FMC 外扩 32MB 内存，LTDC 帧缓冲 |
| **MPU** | `BSP/MPU/` | 内存保护区域配置（SDRAM 开窗）|
| **LCD** | `BSP/LCD/` | LTDC RGB 屏驱动 + 画点/清屏 |
| **LVGL** | `BSP/LVGL/` | GUI 库移植层（disp/indev 接口）|
| **TOUCH** | `BSP/TOUCH/` | 电容触摸（软件 I2C + FT5206/GT9xxx）|
| **Log** | `BSP/Log/` | 分级日志系统 |
| **Modbus** | `BSP/Modbus/` | Modbus RTU 从站 + USB-CDC 双通道 |
| **W5500** | `BSP/W5500/` | 以太网（含 driver 子目录）|
| **ESP8266** | `BSP/ESP8266/` | WiFi（AT 指令，已决定砍掉）|
| **OTA** | `BSP/OTA/` | OTA 升级（协议/传输/flash/manager，19 个文件）|
| **ADC** | `BSP/ADC/` | 模数采集 |
| **DHT11** | `BSP/DHT11/` | 温湿度传感器 |
| **watchdog** | `BSP/bsp_watchdog.c` | 独立看门狗封装 |

### 2.3 内核外设（CM7/Core/Src）

`adc.c / dma.c / dma2d.c / fmc.c / freertos.c / gpio.c / iwdg.c / ltdc.c / sdmmc.c / spi.c / usart.c`

CM4 侧只有：`dma.c / main.c`（轻量实时任务）。

### 2.4 双核共享（Common/）

`boot_meta.c/h`、`boot_slot.c/h`、`bsp_flash_ram.c/h` —— OTA 双槽的元数据、槽位管理、Flash 擦写（放 RAM 执行），双核都会链接。

---

## 三、复刻路线（一个月）

### 3.1 四周总体计划

| 周 | 目标 | 关键产出物 | 验收标准 |
|---|---|---|---|
| **W1** | 双核 + 显示 | 双核启动、FMC/SDRAM、LTDC+LVGL | 双核各自点灯、屏幕点亮 |
| **W2** | RTOS + 存储 + Modbus | FreeRTOS 多任务、日志、SDMMC+FATFS、Modbus RTU 从站 | Modbus Poll 读写线圈 |
| **W3** | 以太网 + OTA（最难） | W5500 TCP、双槽分区、bootloader、SOIP、CRC32、回滚 | 上位机一键升级、断电可回滚 |
| **W4** | 稳定 + 增量 + 包装 | IWDG、复位原因、增量功能、架构图+文档 | 长跑不重启、能脱稿讲透 |

### 3.2 模块优先级（按面试价值 ÷ 耗时排序）

| 档位 | 模块 | 理由 |
|---|---|---|
| 🔴 必做 | 双核启动、OTA 双槽、Modbus、W5500 | 核心故事，面试 80% 问题来自这里 |
| 🟡 够用就行 | 显示、FreeRTOS 基础、SDRAM/MPU | OTA/双核的前置依赖 |
| ⚪ 直接砍 | 自定义 LVGL 复杂界面、DHT11、ADC、USB-CDC 双通道 Modbus、ESP8266 | 高耗时低回报 |

### 3.3 主链路优先级（时间不够时的砍单顺序）

```
双核启动 > OTA 升级回滚 > Modbus > 以太网 > 显示 > 存储
```

只要双核 + OTA 在，项目故事就立得住。

### 3.4 关键决策（已定，勿再讨论）

1. **W5500 vs ESP8266 → 选 W5500**：以太网 TCP 更硬核，W5500 有现成驱动栈，比 ESP8266 AT 指令省时间；
2. **先裸机后 RTOS**：外设先在裸机跑通，最后再移植 FreeRTOS（调试变量少、定位快）；
3. **先串口后 LTDC**：串口是黑屏时唯一的"眼睛"；
4. **用现成驱动但读懂**：驱动代码从原项目搬，但必须逐行读懂、用自己的话注释，不盲目抄；
5. **复刻路径**：直接沿用原项目的分层架构（BSP 模块化 + 双核分离），不重新发明目录。

---

## 四、原项目路径索引（新 AI 快速找代码用）

> 原项目路径：`E:\STM32H7\stm32h7-machine-controller`
> 复刻工程路径：`E:\STM32H7\Template`

### 4.1 原项目关键路径

```
E:\STM32H7\stm32h7-machine-controller\
├── GPIO.ioc                        # 原项目 CubeMX 配置（引脚/时钟/外设参数都在这里）
├── BSP\                            # 板级支持包（复刻时逐个模块搬）
│   ├── SDRAM\sdram.c/h             # SDRAM 驱动
│   ├── MPU\mpu.c/h                 # MPU 内存保护
│   ├── LCD\lcd.c/h, lcd_ltdc.c/h   # LTDC 驱动 + 画点/清屏
│   ├── LVGL\lv_port_*.c/h          # LVGL 移植层
│   ├── TOUCH\*.c/h                 # 触摸（ctiic/ft5206/gt9xxx/touch）
│   ├── Log\bsp_log.c/h             # 日志
│   ├── Modbus\bsp_modbus_*.c/h     # Modbus
│   ├── W5500\bsp_w5500_*.c/h       # 以太网
│   ├── OTA\bsp_ota_*.c/h           # OTA（19 个文件）
│   └── ...
├── CM7\Core\Src\                   # CM7 外设源文件（fmc.c/ltdc.c/usart.c/freertos.c/iwdg.c...）
├── CM4\Core\Src\                   # CM4 源文件（main.c/dma.c）
├── Common\Src\, Inc\               # 双核共享（boot_meta/boot_slot/bsp_flash_ram）
├── Makefile\CM7\Makefile           # CM7 构建脚本（源文件列表/链接脚本）
├── Makefile\CM4\Makefile           # CM4 构建脚本
└── .vscode\                        # 烧录/调试配置（launch.json/tasks.json）
```

### 4.2 复刻工程（Template）对应关系

| 原项目 | 复刻工程 Template | 状态 |
|---|---|---|
| `BSP/SDRAM/` | `BSP/SDRAM/` | ✅ 已搬 |
| `BSP/MPU/` | `BSP/MPU/` | ✅ 已搬 |
| `BSP/LCD/` | `BSP/LCD/` | ⏳ 待搬（下一步）|
| `BSP/TOUCH/` | `BSP/TOUCH/` | ⏳ 待搬 |
| 其余 BSP | 对应 BSP/ | 未搬 |
| `CM7/Core/Src/fmc.c` | `CM7/Core/Src/fmc.c` | ✅ CubeMX 已生成 |
| `CM7/Core/Src/ltdc.c` | `CM7/Core/Src/ltdc.c` | ✅ CubeMX 已生成 |

---

| 周 | 目标 | 关键产出物 | 验收标准 |
|---|---|---|---|
| **W1** | 双核 + 显示 | 双核启动、FMC/SDRAM、LTDC+LVGL | 双核各自点灯、屏幕点亮 |
| **W2** | RTOS + 存储 + Modbus | FreeRTOS 多任务、日志、SDMMC+FATFS、Modbus RTU 从站 | Modbus Poll 读写线圈 |
| **W3** | 以太网 + OTA（最难） | W5500 TCP、双槽分区、bootloader、SOIP、CRC32、回滚 | 上位机一键升级、断电可回滚 |
| **W4** | 稳定 + 增量 + 包装 | IWDG、复位原因、增量功能、架构图+文档 | 长跑不重启、能脱稿讲透 |

### 模块优先级（按面试价值 ÷ 耗时排序）

| 档位 | 模块 | 理由 |
|---|---|---|
| 🔴 必做 | 双核启动、OTA 双槽、Modbus、W5500 | 核心故事，面试 80% 问题来自这里 |
| 🟡 够用就行 | 显示、FreeRTOS 基础、SDRAM/MPU | OTA/双核的前置依赖 |
| ⚪ 直接砍 | 自定义 LVGL 复杂界面、DHT11、ADC、USB-CDC 双通道 Modbus、ESP8266 | 高耗时低回报 |

### 关键决策（已定，勿再讨论）

1. **W5500 vs ESP8266 → 选 W5500**：以太网 TCP 更硬核，W5500 有现成驱动栈，比 ESP8266 AT 指令省时间；
2. **先裸机后 RTOS**：外设先在裸机跑通，最后再移植 FreeRTOS（调试变量少、定位快）；
3. **先串口后 LTDC**：串口是黑屏时唯一的"眼睛"；
4. **用现成驱动但读懂**：驱动代码从原项目搬，但必须逐行读懂、用自己的话注释，不盲目抄；
5. **复刻路径**：直接沿用原项目的分层架构（BSP 模块化 + 双核分离），不重新发明目录。

---

## 五、当前进度（每天更新）

**已完成 ✅**

| 里程碑 | 状态 | 日期 |
|---|---|---|
| 双核点灯（HSEM + EnableBootCore） | ✅ | 08-16 |
| SDRAM 32MB 读写验证 | ✅ | 08-16 |
| USART1 + printf 重定向 | ✅ | 08-17 |
| LTDC 点屏（显示纯红） | ✅ | 08-17 |

**进行中 / 下一步**

1. 搬正式 LCD 驱动（`ltdc_fill`/`ltdc_clear` 用 DMA2D 硬件填充，替换手写 for 循环）；
2. 搬 `ltdc_gpio_init()`（背光/复位/供电正式封装）；
3. 配触摸（软件 I2C + FT5206），串口打印坐标。

**详细日志**：见 [logs/01_display_bringup_log.md](logs/01_display_bringup_log.md)

---

## 六、协作约定（主人给浮浮酱的约束，新窗口必读）

> 这些是主人明确提出的、全局配置之外的要求，必须严格遵守。

### 4.1 身份与语言（来自全局 CLAUDE.md）
- 用**简体中文**回复；
- 猫娘人设「幽浮喵」：自称"浮浮酱"，称呼用户"主人"，用颜文字（不是 emoji）。

### 4.2 工作方式约束（对话中主人明确提出）

1. **移植文件前必须解释**：主人明确批评过"默默用 cp 命令搬文件不解释"。每次迁移/复制任何文件（BSP 模块、驱动、中间件）之前，先讲清楚：这文件是谁写的、作用是什么、为什么需要、关键函数有哪些，再动手。**绝不默默代劳。**

2. **不要凭印象猜，要核对事实**：主人说过"我不是别凭印象的，我是专门去看了的"。给出结论前必须查证（读原项目代码、查 HAL 源码、读 .ioc），不要凭记忆或推测回答。

3. **主人倾向自己动手操作**：主人的角色是"学习者 + 执行者"，浮浮酱的角色是"指导 + 核对 + 讲解"。**能指导的不要代劳**——像 CubeMX 配置、写代码、烧录验证，让主人自己做，浮浮酱提供思路、参数、核对。

4. **边学边干，理解原理优先**：主人会频繁问"为什么"，浮浮酱要讲透原理（为什么是这个值、为什么是这个顺序），而不是只给结果。

### 4.3 主人的工作习惯
- 用 **PowerShell** 跑目录/文件命令，用 **Git Bash** 跑编译命令（别混用语法）；
- 用 **CubeMX** 配外设（自己操作），配完让浮浮酱核对生成的代码；
- 遇到报错会把**报错原文**贴给浮浮酱，要求精准定位。

---

## 七、浮浮酱的协作经验（踩过的坑，避免重犯）

### 5.1 技术经验

1. **CubeMX 三大铁律**：
   - 自定义代码放 `USER CODE BEGIN/END` 块，否则 Generate 会覆盖；
   - 新建 `.c` 必须手动加进 Makefile 的 `C_SOURCES`，新头文件目录加 `C_INCLUDES`；
   - 生成后逐项核对：引脚映射、时序、Layer、GPIO speed——CubeMX 默认值 ≠ 板子实际值。

2. **引脚映射是复刻高频翻车点**：SDRAM 片选（SDNE1/SDCKE1 在 PH6/PH7 不是 PB5/PB6）、LTDC 数据线（PJ/PK/PI 组），都要对照原项目 .ioc 改。

3. **高速外设 GPIO speed 必须 VERY_HIGH**：LTDC（51.2MHz）、FMC 的引脚不能用 LOW，否则信号跟不上黑屏。

4. **Makefile 工程特点**：`CPU/FPU/FLOAT-ABI` 三项要核对（CM4 曾被误写成 cortex-m7）；编译产物要手动确认 .elf 更新。

### 5.2 方法论经验

1. **定位硬件坑的正确姿势**：拿原项目代码硬对比 → 查 HAL 源码 → 读寄存器 → 不凭印象猜。浮浮酱曾两次误判（缓存、Layer 1），最后靠"你的 ltdc.c vs 原项目 ltdc.c"硬对比才找到 GPIO speed 根因。

2. **解释优于代劳**：主人是学习者，跳过解释 = 跳过学习点（曾因默默搬 mpu.c 被批评）。

3. **逐项核对比一次搬太多更稳**：LTDC 配置分「时序/引脚/层」三层，逐层核对，比一次搬完出错了难定位好得多。

---

## 八、复刻踩坑记录索引

| 坑 | 根因 | 见 |
|---|---|---|
| LTDC 黑屏 | GPIO speed LOW | 01 文档 §6.4 |
| SDRAM 读不到 | 片选/时钟引脚错 | 01 文档 §3.3 |
| LTDC 引脚错位 | CubeMX 默认组 | 01 文档 §6.2 |
| 屏只显示 480 行 | Active Height 漏填 | 01 文档 §6.2 |
| Layer 全零 | 层参数没配 | 01 文档 §6.2 |
| include 被冲掉 | Generate 重置块外代码 | 01 文档 §7.2 |
| printf 无效 | retarget.c 没进 Makefile | 01 文档 §5.3 |
| CM4 编译参数错 | CPU=m7 应为 m4 | 01 文档 §1.3 |

---

## 九、文档结构约定

```
Docs/
├── 00_project_plan_and_meta.md      # 元文档：总体规划 + 进度 + 约定（每天更新，放根目录）
├── logs/                            # 开发日志（按阶段）
│   └── 01_display_bringup_log.md
├── references/                      # 参考资料（原理图、手册、datasheet）
│   └── STM32H747XIH6 CB V1.1_SCH.pdf
└── guides/                          # 排错/工具指南
    └── STM32_IntelliSense_Troubleshooting.md
```

- `00_project_plan_and_meta.md`（本文档）：总体规划 + 进度 + 约定，**每天完成后更新**；
- `logs/01_display_bringup_log.md`：显示链路阶段的详细复盘；
- 后续每个阶段追加 `logs/02_xxx.md`、`logs/03_xxx.md`...；
- 参考资料放 `references/`，工具排错放 `guides/`。
