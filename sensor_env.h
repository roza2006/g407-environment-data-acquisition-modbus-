#ifndef __SENSOR_ENV_H
#define __SENSOR_ENV_H

#include "stm32f4xx.h"

/* Device status register bit definitions (Modbus address 0x0003). */
#define ENV_STATUS_PM25_ERROR       0x0001U
#define ENV_STATUS_PM10_ERROR       0x0002U
#define ENV_STATUS_BH1750_ERROR     0x0004U
#define ENV_STATUS_DATA_NOT_READY   0x0008U
#define ENV_STATUS_SENSOR_TIMEOUT   0x0010U

typedef struct
{
    uint16_t pm25;
    uint16_t pm10;
    uint16_t illuminance;
    uint16_t status;
} EnvSensor_Typedef;

void EnvSensor_InitData(EnvSensor_Typedef *env);
void EnvSensor_Read(EnvSensor_Typedef *env);

#endif
