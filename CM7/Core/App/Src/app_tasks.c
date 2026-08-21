#include "app_tasks.h"

#define LED_TASK_STACK_SIZE 256U
#define LED_TASK_PRIORITY 2U
static TaskHandle_t led_task_handle = NULL;
static void LED_Task(void *argument);

#define TOUCH_TASK_STACK_SIZE 512U
#define TOUCH_TASK_PRIORITY 3U
#define TOUCH_TASK_PERIOD_MS 1U
static TaskHandle_t touch_task_handle = NULL;
static void TouchTask(void *argument);

#define TOUCHMONITOR_TASK_STACK_SIZE 512U
#define TOUCHMONITOR_TASK_PRIORITY 3U
static TaskHandle_t touchmonitor_task_handle = NULL;
static void TouchMonitor_Task(void *argument);
static QueueHandle_t touch_event_queue_handle = NULL;
TouchEvent_t touch_event = {0};

#define SYSTEM_MONITOR_TASK_STACK_SIZE 512U
#define SYSTEM_MONITOR_TASK_PRIORITY 1U
static TaskHandle_t system_monitor_task_handle = NULL;
static void SystemMonitor_Task(void *argument);

#define DHT11_TASK_STACK_SIZE 512U
#define DHT11_TASK_PRIORITY 2U
static TaskHandle_t dht11_task_handle = NULL;
static void DHT11_Task(void *argument);

static SemaphoreHandle_t uart_mutex_handle = NULL;
static void log_print(char *format, ...);

static void log_print(char *format, ...)
{
    va_list args;

    if (xSemaphoreTake(uart_mutex_handle, pdMS_TO_TICKS(100U)) == pdPASS)
    {
        va_start(args, format);
        (void)vprintf(format, args);
        va_end(args);
        (void)xSemaphoreGive(uart_mutex_handle);
    }
}

void Mutex_Init(void)
{
    uart_mutex_handle = xSemaphoreCreateMutex();
    configASSERT(uart_mutex_handle != NULL);
}

void Queue_Init(void)
{
    touch_event_queue_handle = xQueueCreate(16, sizeof(TouchEvent_t));
    configASSERT(touch_event_queue_handle != NULL);
}

void AppTasks_Init(void)
{
    BaseType_t result;

    result = xTaskCreate(
        LED_Task,
        "LedTask",
        LED_TASK_STACK_SIZE,
        NULL,
        LED_TASK_PRIORITY,
        &led_task_handle);

    configASSERT(result == pdPASS);

    result = xTaskCreate(
        TouchTask,
        "TouchTask",
        TOUCH_TASK_STACK_SIZE,
        NULL,
        TOUCH_TASK_PRIORITY,
        &touch_task_handle);

    configASSERT(result == pdPASS);

    result = xTaskCreate(
        TouchMonitor_Task,
        "TouchMonitorTask",
        TOUCHMONITOR_TASK_STACK_SIZE,
        NULL,
        TOUCHMONITOR_TASK_PRIORITY,
        &touchmonitor_task_handle);

    configASSERT(result == pdPASS);

    result = xTaskCreate(
        SystemMonitor_Task,
        "SystemMonitorTask",
        SYSTEM_MONITOR_TASK_STACK_SIZE,
        NULL,
        SYSTEM_MONITOR_TASK_PRIORITY,
        &system_monitor_task_handle);
    configASSERT(result == pdPASS);

    result = xTaskCreate(
        DHT11_Task,
        "DHT11Task",
        DHT11_TASK_STACK_SIZE,
        NULL,
        DHT11_TASK_PRIORITY,
        &dht11_task_handle);
    configASSERT(result == pdPASS);
}

static void LED_Task(void *argument)
{
    // TickType_t last_wake_time;

    (void)argument;

    // last_wake_time = xTaskGetTickCount();
    log_print("LED Task Start \r\n");
    for (;;)
    {
        // HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_3);
        // vTaskDelayUntil(
        //     &last_wake_time,
        //     pdMS_TO_TICKS(500U));
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}

static void TouchTask(void *argument)
{
    TickType_t last_wake_time;
    uint8_t was_down = 0U;
    uint16_t last_x = 0U;
    uint16_t last_y = 0U;
    TouchEvent_t event;
    uint8_t event_ready = 0U;
    uint8_t is_down;

    (void)argument;

    last_wake_time = xTaskGetTickCount();

    log_print("Touch Task Start\r\n");

    for (;;)
    {
        event_ready = 0U;

        (void)ft5206_scan(0U);

        is_down = ((tp_dev.sta & TP_PRES_DOWN) != 0U);

        if ((is_down != 0U) && (was_down == 0U))
        {
            event.type = TOUCH_EVENT_DOWN;
            event.x = tp_dev.x[0];
            event.y = tp_dev.y[0];
            event_ready = 1;
        }
        else if ((is_down != 0U) &&
                 ((tp_dev.x[0] != last_x) ||
                  (tp_dev.y[0] != last_y)))
        {
            event.type = TOUCH_EVENT_MOVE;
            event.x = tp_dev.x[0];
            event.y = tp_dev.y[0];
            event_ready = 1U;
        }
        else if ((is_down == 0U) && (was_down != 0U))
        {
            event.type = TOUCH_EVENT_UP;
            event.x = last_x;
            event.y = last_y;
            event_ready = 1U;
        }

        if (event_ready != 0U)
        {
            event.timestamp = (uint32_t)xTaskGetTickCount();

            if (xQueueSend(touch_event_queue_handle, &event, 0U) != pdPASS)
            {
                log_print("touch_event_queue_handle full\r\n");
            }
        }

        if (is_down != 0)
        {
            last_x = tp_dev.x[0];
            last_y = tp_dev.y[0];
        }

        was_down = is_down;
        /*
         * 周期性任务使用 vTaskDelayUntil()
         * 保证任务按照固定时间基准运行。
         */
        vTaskDelayUntil(
            &last_wake_time,
            pdMS_TO_TICKS(TOUCH_TASK_PERIOD_MS));
    }
}

static void TouchMonitor_Task(void *argument)
{
    TouchEvent_t received_event;
    (void)argument;

    for (;;)
    {
        if (xQueueReceive(touch_event_queue_handle, &received_event, portMAX_DELAY) == pdPASS)
        {
            if (received_event.type == TOUCH_EVENT_DOWN || received_event.type == TOUCH_EVENT_MOVE)
            {
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
            }
            else if (received_event.type == TOUCH_EVENT_UP)
            {
                HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
            }

            log_print(
                "Touch event: type=%d, x=%u, y=%u, tick=%lu\r\n",
                received_event.type,
                received_event.x,
                received_event.y,
                (unsigned long)received_event.timestamp);
        }
    }
}

static void SystemMonitor_Task(void *argument)
{
    UBaseType_t led_stack_left;
    UBaseType_t touch_stack_left;
    UBaseType_t monitor_stack_left;
    UBaseType_t queue_used;
    UBaseType_t queue_free;

    for (;;)
    {
        led_stack_left = uxTaskGetStackHighWaterMark(led_task_handle);
        touch_stack_left = uxTaskGetStackHighWaterMark(touch_task_handle);
        monitor_stack_left = uxTaskGetStackHighWaterMark(touchmonitor_task_handle);

        queue_used = uxQueueMessagesWaiting(touch_event_queue_handle);
        queue_free = uxQueueSpacesAvailable(touch_event_queue_handle);

        log_print(
            "monitor: led=%lu, touch=%lu, monitor=%lu, "
            "queue_used=%lu, queue_free=%lu\r\n",
            (unsigned long)led_stack_left,
            (unsigned long)touch_stack_left,
            (unsigned long)monitor_stack_left,
            (unsigned long)queue_used,
            (unsigned long)queue_free);

        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}

static void DHT11_Task(void *argument)
{
    DHT11_Data_t data;

    (void)argument;

    log_print("DHT11 Task Start\r\n");

    DHT11_Init();

    /*
     * DHT11上电后需要等待一段时间再读取。
     */
    vTaskDelay(pdMS_TO_TICKS(1000U));

    for (;;)
    {
        if (DHT11_Read(&data) == 0)
        {
            log_print(
                "DHT11: humidity=%u.%u%%, temperature=%u.%uC\r\n",
                data.humidity,
                data.humidity_dec,
                data.temperature,
                data.temperature_dec);
        }
        else
        {
            log_print("DHT11 read failed\r\n");
        }

        /*
         * DHT11两次读取之间至少间隔约2秒。
         */
        vTaskDelay(
            pdMS_TO_TICKS(DHT11_SAMPLE_INTERVAL_MS));
    }
}