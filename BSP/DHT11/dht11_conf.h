#ifndef __DHT11_CONF_
#define __DHT11_CONF_

#include "main.h"

#define DHT11_GPIO_PORT              GPIOD
#define DHT11_GPIO_PIN               GPIO_PIN_12
#define DHT11_GPIO_CLK_ENABLE()      __HAL_RCC_GPIOD_CLK_ENABLE()

#define DHT11_START_LOW_MS 20U
#define DHT11_SAMPLE_INTERVAL_MS 2000U
#define DHT11_TIMEOUT_US 100U
#define DHT11_BIT_SAMPLE_US 40U
#endif
