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
#define TOUCHMONITOR_TASK_PRIORITY  3U
static TaskHandle_t touchmonitor_task_handle = NULL;
static void TouchMonitor_Task(void *argument);
static QueueHandle_t touch_event_queue_handle = NULL;
TouchEvent_t touch_event = {0};

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

    touch_event_queue_handle = xQueueCreate(16, sizeof(TouchEvent_t));
    if (touch_event_queue_handle == NULL)
    {
        Error_Handler();
    }

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
}

static void LED_Task(void *argument)
{
    // TickType_t last_wake_time;

    (void)argument;

    // last_wake_time = xTaskGetTickCount();
    printf("LED Task Start \r\n");
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

    printf("Touch Task Start\r\n");

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
                printf("touch_event_queue_handle full\r\n");
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

            printf(
                "Touch event: type=%d, x=%u, y=%u, tick=%lu\r\n",
                received_event.type,
                received_event.x,
                received_event.y,
                (unsigned long)received_event.timestamp);
        }
    }
}
