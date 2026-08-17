# STM32 + VS Code IntelliSense 排查指南

本文用于排查 STM32 工程中 VS Code C/C++ IntelliSense 报告的“未定义类型、未定义句柄、HAL 函数不可见、自动补全失效”等问题。

## 1. 先判断问题类型

先看诊断来源：

```text
C/C++: IntelliSense
```

这表示编辑器分析器报错，不一定代表真实编译失败。先用工程实际使用的编译器做语法检查：

```powershell
arm-none-eabi-gcc `
  -std=c11 `
  -DCORE_CM7 `
  -DUSE_HAL_DRIVER `
  -DSTM32H747xx `
  -ICM7/Core/Inc `
  -IDrivers/STM32H7xx_HAL_Driver/Inc `
  -IDrivers/CMSIS/Include `
  -mcpu=cortex-m7 `
  -mthumb `
  -fsyntax-only `
  CM7/Core/Src/retarget.c
```

判断原则：

| 结果 | 优先排查方向 |
| --- | --- |
| GCC 通过，IntelliSense 报错 | VS Code 配置、Profile、缓存或插件冲突 |
| GCC 也失败 | 源码、头文件、宏或真实构建参数 |

## 2. 识别级联错误

例如：

```c
uint8_t c = (uint8_t)ch;
HAL_UART_Transmit(&huart1, &c, 1, 100);
```

如果 `stdint.h` 没有被正确解析，可能同时出现：

```text
uint8_t 未定义
应输入 ";"
huart1 未定义
HAL_UART_Transmit 不可见
```

通常第一个头文件或类型错误才是根因，后面的“应输入 ;”是解析失败后的级联错误。不要先修改 C 语法来掩盖这类问题。

## 3. 查看 IntelliSense 的最终配置

不能只看 `.vscode/c_cpp_properties.json`。配置可能来自多个层级：

```text
用户 settings.json
    ↓
当前 VS Code Profile settings.json
    ↓
工作区 .vscode/settings.json
    ↓
工作区 .vscode/c_cpp_properties.json
    ↓
compile_commands.json
    ↓
EIDE、CMake 或其他配置提供插件
```

执行：

```text
Ctrl+Shift+P
C/C++: Log Diagnostics
```

重点检查以下内容：

```text
compilerPath
intelliSenseMode
includePath
systemIncludePath
defines
compileCommands
```

`compilerPath` 必须指向编译器可执行文件，例如：

```text
C:/.../arm-none-eabi-gcc.exe
```

不能填写头文件目录，例如：

```text
D:/Keil/ARM/ARMCC/include
```

## 4. 防止工具链混用

一个 GCC 工程不应同时把 Keil ARMCC 系统头文件加入 IntelliSense：

```text
错误组合：ARM GCC 编译器 + Keil ARMCC include
正确组合：ARM GCC 编译器 + GCC/Newlib + STM32 HAL + CMSIS
```

特别检查日志中是否出现：

```text
--sys_include=D:\Keil\ARM\ARMCC\INCLUDE
```

如果当前工程使用 ARM GCC，应移除 Profile 或用户设置中指向 Keil 的以下配置：

```json
"C_Cpp.default.compilerPath"
"C_Cpp.default.systemIncludePath"
"C_Cpp.default.includePath"
"C_Cpp.default.browse.path"
"C_Cpp.default.compileCommands"
```

修改全局或 Profile 配置前建议先备份。更稳妥的做法是在工程的 `.vscode/settings.json` 中明确设置本工程的 ARM GCC 路径。

## 5. 检查 IntelliSense 是否被关闭

搜索所有用户、Profile 和工作区设置：

```json
"C_Cpp.intelliSenseEngine": "disabled"
```

该值会关闭语义分析和自动补全。STM32 工程应使用：

```json
"C_Cpp.intelliSenseEngine": "default"
```

如果工程中存在多个 C/C++ 配置，建议同时指定 Windows ARM 模式：

```json
"C_Cpp.default.intelliSenseMode": "windows-gcc-arm"
```

## 6. 双核 STM32 工程的检查方法

CM4 和 CM7 必须使用匹配的头文件路径、宏和 CPU 参数：

```text
CM7 源文件
  ├─ CM7/Core/Inc
  ├─ CORE_CM7
  └─ -mcpu=cortex-m7

CM4 源文件
  ├─ CM4/Core/Inc
  ├─ CORE_CM4
  └─ -mcpu=cortex-m4
```

检查以下风险：

- CM7 配置是否误用了 `CM4/Core/Inc`。
- `main.h`、`usart.h` 等同名头文件是否被错误路径优先找到。
- `CORE_CM7`、`CORE_CM4` 是否同时定义。
- HAL 模块宏是否来自正确核心的 `stm32h7xx_hal_conf.h`。
- 当前配置索引是否确实选择了 CM7：`CppProperties.currentConfigurationIndex`。

双核工程不建议只依赖一个全局配置覆盖所有源文件。优先使用真实的 `compile_commands.json`，让每个源文件获得自己的编译参数。

## 7. 头文件包含原则

源文件或模块头文件应直接包含自己使用的依赖。例如 `retarget.h` 使用 UART 和固定宽度整数类型，可以明确包含：

```c
#include <stdint.h>
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_uart.h"
#include "usart.h"
```

避免为了声明一个函数而加入无关头文件，例如 `retarget.c` 不使用 `stdio.h` 时，不必在 `retarget.h` 中包含它。

直接包含依赖不能修复错误的 IntelliSense 配置，但可以减少间接包含带来的不确定性。

## 8. 配置修改后的验证顺序

每次修改配置后按以下顺序操作：

1. 保存 `.vscode/settings.json` 和 `.vscode/c_cpp_properties.json`。
2. 执行 `C/C++: Log Diagnostics`，确认最终 `compilerPath` 正确。
3. 确认日志中没有错误工具链路径，例如 `D:\Keil\ARM\ARMCC\include`。
4. 执行 `C/C++: Reset IntelliSense Database`。
5. 执行 `Developer: Reload Window`。
6. 关闭并重新打开出现问题的 `.c` 文件。
7. 再用 `arm-none-eabi-gcc -fsyntax-only` 验证真实编译器。

如果日志出现类似以下内容，说明翻译单元已经重新建立：

```text
IntelliSense 引擎 = default
--gcc
--c11
Parsed file (update): .../retarget.c
```

## 9. 常见误区

### 误区一：看到“未定义类型”就修改源码

先确认 `stdint.h` 是否被正确加载。配置错误时，修改为 `unsigned char` 只能隐藏一个报错，不能恢复 HAL 类型和自动补全。

### 误区二：只修改 c_cpp_properties.json

用户设置、Profile 设置和配置提供插件可能覆盖或合并它。必须以 `C/C++: Log Diagnostics` 的最终参数为准。

### 误区三：把“完整构建失败”和 IntelliSense 错误混为一谈

例如 `main.c` 缺少 SDRAM 函数声明是实际 C 编译问题；`uint8_t` 红线但 GCC 通过则是 IntelliSense 配置问题。两者应分开处理。

### 误区四：在双核工程里把 CM4 和 CM7 头文件路径全部混在一起

这样可能让同名 `main.h` 被错误配置先找到，产生大量与实际硬件核心无关的假错误。

## 10. 快速检查清单

```text
[ ] 报错来源是否为 IntelliSense，而非 GCC/Make
[ ] arm-none-eabi-gcc -fsyntax-only 是否通过
[ ] compilerPath 是否指向 arm-none-eabi-gcc.exe
[ ] 是否混入 Keil ARMCC include
[ ] C_Cpp.intelliSenseEngine 是否为 default
[ ] intelliSenseMode 是否为 windows-gcc-arm
[ ] 当前配置是否为正确的 CM4/CM7
[ ] CORE_CM4/CORE_CM7 是否匹配
[ ] includePath 是否包含正确 Core/Inc
[ ] compile_commands.json 是否匹配当前源文件
[ ] 是否已重置 IntelliSense 数据库
[ ] 是否已重新加载 VS Code 窗口
```

## 结论

STM32 IntelliSense 报错的排查核心是：先区分真实编译和编辑器分析，再查看 C/C++ 扩展的最终配置。对双核工程尤其要防止 CM4/CM7 配置混用，以及 ARM GCC、Keil ARMCC 两套工具链头文件混用。
