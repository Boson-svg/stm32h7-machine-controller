# stm32h7-machine-controller

基于 STM32H747XIH6 双核（Cortex-M7 + Cortex-M4）的工业控制器复刻项目。

## 项目简介

一个 STM32H747 双核工业控制器，CM7 跑主业务（显示 + 网络 + 存储 + OTA），CM4 跑轻量实时任务，双核通过 HSEM 硬件信号量与共享内存协作。核心亮点是**双核架构**与 **OTA 双槽升级**。

## 硬件

- 主控：STM32H747XIH6（双核 CM7 + CM4）
- 屏幕：WKS70WSV078-WCT（7 寸 1024×600 RGB 电容触摸屏）
- SDRAM：IS42S32800J（32MB，FMC Bank2）
- 调试：JLink（SWD）

## 工具链

- 编译器：arm-none-eabi-gcc（xPack 15.2.1）
- 构建：Makefile
- 调试/烧录：JLink + VSCode（cortex-debug）

## 目录结构

```
Template/
├── CM7/               # Cortex-M7 主核（主业务）
├── CM4/               # Cortex-M4 从核（轻量实时任务）
├── Common/            # 双核共享代码
├── BSP/               # 板级支持包（按外设模块划分）
├── Drivers/           # ST 官方 HAL 库
├── Middlewares/       # 第三方中间件（LVGL 等）
├── Makefile/          # 双核独立构建脚本
└── Docs/              # 项目文档（规划/日志/参考）
```

## 当前进度

- [x] 双核点灯（HSEM 握手 + EnableBootCore）
- [x] SDRAM 32MB 读写验证
- [x] USART1 + printf 重定向
- [x] LTDC 点屏（显示纯红）
- [ ] 正式 LCD 驱动（DMA2D 加速）
- [ ] 触摸屏（FT5206）
- [ ] FreeRTOS + Modbus
- [ ] W5500 以太网 + OTA 双槽升级

详细规划与踩坑记录见 `Docs/`。

## 构建

```bash
# 编译 CM7
cd Makefile/CM7 && make

# 编译 CM4
cd Makefile/CM4 && make
```

烧录与调试配置见 `.vscode/launch.json` 与 `.vscode/tasks.json`。
