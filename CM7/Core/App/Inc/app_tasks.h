#ifndef APP_TASKS_H
#define APP_TASKS_H

#include "main.h"
#include "stdio.h"
#include "stdint.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "ft5206.h"
#include "touch.h"


void AppTasks_Init(void);

typedef enum{
    TOUCH_EVENT_DOWN,
    TOUCH_EVENT_MOVE,
    TOUCH_EVENT_UP
}TouchEventType_t;

typedef struct{
    TouchEventType_t type;
    uint16_t x;
    uint16_t y;
    TickType_t timestamp;
}TouchEvent_t;

#endif