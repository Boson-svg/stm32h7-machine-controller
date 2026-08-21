#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"
#include "dht11_conf.h"
#include "bsp_delay.h"


typedef struct
{
    uint8_t humidity;
    uint8_t humidity_dec;
    uint8_t temperature;
    uint8_t temperature_dec;
} DHT11_Data_t;

void DHT11_Init(void);
int DHT11_Read(DHT11_Data_t *data);

#endif
