# Day 04：FreeRTOS 队列通信与触摸 LED 业务验证

> 工作日期：2026-08-21  
> 工程：STM32H747XIH6 双核工业控制器复刻项目  
> 工作范围：CM7、FreeRTOS、触摸轮询、队列通信、LED 业务响应

## 一、今日目标

Day 04 的目标不是继续增加外设，而是把 Day 03 的两个独立任务连接起来，完成一次完整的 FreeRTOS 任务间通信：

1. `TouchTask` 负责采集触摸屏数据；
2. 使用 `TouchEvent_t` 描述触摸事件；
3. 使用 FreeRTOS 队列传递事件；
4. `TouchMonitor_Task` 接收事件并执行 LED 业务逻辑；
5. 通过串口日志和开发板现象验证任务通信链路。

本日采用“采集任务”和“业务任务”分离的结构，为后续接入屏幕按钮、LVGL 或 Modbus 控制逻辑做准备。

## 二、最终任务架构

```text
FT5446U / FT5x06 兼容触摸驱动
            │
            ▼
      TouchTask
  轮询并识别触摸状态
            │ xQueueSend()
            ▼
  touch_event_queue_handle
            │ xQueueReceive()
            ▼
   TouchMonitor_Task
  解析事件并控制 LED
```

当前任务运行在 CM7，CM4 的双核启动和点灯逻辑保持不变。

## 三、核心数据结构

文件：`CM7/Core/App/Inc/app_tasks.h`

```c
typedef enum
{
    TOUCH_EVENT_DOWN,
    TOUCH_EVENT_MOVE,
    TOUCH_EVENT_UP
} TouchEventType_t;

typedef struct
{
    TouchEventType_t type;
    uint16_t x;
    uint16_t y;
    TickType_t timestamp;
} TouchEvent_t;
```

`TouchEvent_t` 是任务之间传递的消息格式：

| 成员 | 含义 |
|---|---|
| `type` | 触摸按下、移动或抬起 |
| `x` | 触摸点 X 坐标 |
| `y` | 触摸点 Y 坐标 |
| `timestamp` | 事件产生时的 FreeRTOS Tick 值 |

队列传递的是结构体内容的拷贝，不是局部变量地址，因此 `TouchTask` 中的局部 `event` 在发送后可以安全复用。

## 四、队列实现

文件：`CM7/Core/App/Src/app_tasks.c`

```c
static QueueHandle_t touch_event_queue_handle = NULL;

touch_event_queue_handle = xQueueCreate(16, sizeof(TouchEvent_t));
```

当前队列最多保存 16 个 `TouchEvent_t`。句柄使用 `static` 限制在当前源文件内部，使用 `QueueHandle_t` 作为当前 FreeRTOS 推荐的队列句柄类型。

发送端使用非阻塞方式：

```c
if (xQueueSend(touch_event_queue_handle, &event, 0U) != pdPASS)
{
    printf("touch_event_queue_handle full\r\n");
}
```

这样不会让触摸采集任务因为业务任务暂时未读取而长时间阻塞；如果队列已满，则通过日志暴露丢事件情况。

接收端使用永久等待：

```c
if (xQueueReceive(
        touch_event_queue_handle,
        &received_event,
        portMAX_DELAY) == pdPASS)
{
    /* 处理接收到的事件 */
}
```

队列为空时，`TouchMonitor_Task` 会进入阻塞态，不会进行无意义的轮询。

## 五、TouchTask 的事件生成

文件：`CM7/Core/App/Src/app_tasks.c`

`TouchTask` 每 1 ms 运行一次，调用既有的 `ft5206_scan(0U)` 更新全局触摸状态 `tp_dev`。任务使用 `was_down` 保存上一轮是否按下，使用 `last_x`、`last_y` 保存上一有效坐标。

状态判定逻辑为：

| 当前状态 | 上一状态 | 坐标变化 | 生成事件 |
|---|---|---|---|
| 按下 | 未按下 | 不要求 | `TOUCH_EVENT_DOWN` |
| 按下 | 按下 | 有变化 | `TOUCH_EVENT_MOVE` |
| 未按下 | 按下 | 不要求 | `TOUCH_EVENT_UP` |
| 未按下 | 未按下 | 不适用 | 不生成事件 |

本日复查并修正了两个容易造成逻辑错误的点：

1. `event_ready` 必须在每轮循环开始时清零，否则第一次事件产生后会被重复发送；
2. `TOUCH_EVENT_UP` 必须在扫描结果显示“当前未按下”时判断，不能把它放在“扫描成功检测到触摸”的分支内部。

## 六、TouchMonitor_Task 的 LED 业务

当前业务逻辑如下：

- 收到 `TOUCH_EVENT_DOWN` 或 `TOUCH_EVENT_MOVE`：LED 输出低电平；
- 收到 `TOUCH_EVENT_UP`：LED 输出高电平；
- 同时通过 USART1 输出事件类型、坐标和 Tick 时间。

这一步验证了采集任务不需要直接操作 LED，业务任务只需要处理结构化事件即可。后续可以将 LED 操作替换为屏幕按钮状态更新、LVGL 控件事件回调、Modbus 线圈控制或设备运行状态机输入。

## 七、今日遇到的问题与修正

### 7.1 队列接收返回值

`xQueueReceive()` 的返回值是 `BaseType_t`，成功返回 `pdPASS`，失败返回 `pdFAIL`。不能使用 `== NULL` 判断成功。

### 7.2 队列接收参数顺序

函数参数顺序必须是：

```c
xQueueReceive(队列句柄, 接收缓冲区地址, 最大等待 Tick 数);
```

接收缓冲区必须传 `&received_event`，而不是再次传入队列句柄。

### 7.3 LED_Task 空转

当 LED 控制逻辑移动到 `TouchMonitor_Task` 后，原来的 LED 周期任务没有业务内容。如果保留该任务，必须通过 `vTaskDelay()` 阻塞，不能使用空的死循环，否则会持续消耗 CPU。

当前代码使用：

```c
vTaskDelay(pdMS_TO_TICKS(1000U));
```

## 八、验证证据

### 8.1 开发板验证

主人已在开发板上确认：

- 串口能够输出触摸事件信息；
- 触摸按下、移动、抬起能够进入对应业务分支；
- LED 能够根据触摸事件改变状态；
- 触摸释放后不会持续重复旧事件；
- 未出现队列持续满载的异常现象。

### 8.2 CM4 构建验证

```powershell
mingw32-make -W Makefile BUILD_DIR=E:/STM32H7/day04_verify/CM4 all
```

结果：返回 `exit=0`，生成 `Template_CM4.elf`、`.hex` 和 `.bin`。

链接规模：

```text
text=3064  data=16  bss=1568  dec=4648  hex=1228
```

### 8.3 CM7 构建验证

```powershell
mingw32-make -W Makefile BUILD_DIR=E:/STM32H7/day04_verify/CM7 all
```

结果：返回 `exit=0`，生成 `Template_CM7.elf`、`.hex` 和 `.bin`。编译过程包含 `app_tasks.c`、FreeRTOS 内核、CMSIS-RTOS V2 适配层和 CM7 启动文件。

链接规模：

```text
text=33544  data=112  bss=21216  dec=54872  hex=d658
```

上述临时验证目录已在验证后删除，没有加入工程仓库。

## 九、当前限制

1. 触摸仍使用轮询方式，没有接入 FT5446U 的 INT/EXTI 中断；
2. 当前事件只处理第一个触摸点；
3. `printf` 尚未通过互斥锁统一保护，多任务日志增多后需要建立日志任务或 UART Mutex；
4. 顶层 `Makefile/Makefile` 仍使用旧的 `cd CM4`、`cd CM7` 路径，当前使用 `Makefile/CM4` 和 `Makefile/CM7` 直接构建；
5. Windows 下原有 `build` 目录的 Makefile 创建规则存在兼容性问题，后续可单独整理构建脚本。

## 十、Day04 结论

Day 04 完成了 FreeRTOS 队列通信和一次真实业务闭环。当前系统已经从“任务各自运行”进入“任务之间传递数据并驱动业务”的阶段，具备继续接入 GUI、Modbus 或设备状态机的基础。

## 十一、下一步建议

Day 05 建议学习和实现 FreeRTOS 同步机制：

1. 为 USART1 日志增加 Mutex，避免多个任务同时输出；
2. 学习二值信号量、计数信号量与队列的适用场景；
3. 根据触摸事件增加一个简单的显示状态响应；
4. 复查任务栈使用量、空闲任务占用率和队列高水位；
5. 完成后再进入 Modbus RTU 的通信任务设计。
