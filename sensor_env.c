#include "sensor_env.h"
#include "BH1750.h"
#include "PM25.h"
#include "delay.h"

void EnvSensor_InitData(EnvSensor_Typedef *env)
{
    env->pm25 = 0U;
    env->pm10 = 0U;
    env->illuminance = 0U;
    env->status = ENV_STATUS_DATA_NOT_READY;
}

void EnvSensor_Read(EnvSensor_Typedef *env)
{
    unsigned char data_buf[3] = {0};
    unsigned int illuminance_raw;
    uint16_t status = 0U;

    BH1750_ReadData(BH1750_SlaveAddress, data_buf);

    illuminance_raw = ((unsigned int)data_buf[0] << 8U) |
                      (unsigned int)data_buf[1];
    env->illuminance = (uint16_t)(illuminance_raw * 10U / 12U);

    /* Current BH1750 driver has no ACK/error return value. */
    if ((data_buf[0] == 0xFFU) && (data_buf[1] == 0xFFU))
    {
        status |= ENV_STATUS_BH1750_ERROR;
    }

    PM25Collection();

    if (PM25_LastFrameValid == 0U)
    {
        status |= ENV_STATUS_PM25_ERROR;
        status |= ENV_STATUS_PM10_ERROR;
        status |= ENV_STATUS_SENSOR_TIMEOUT;
    }
    else
    {
        env->pm25 = (uint16_t)PM25;
        env->pm10 = (uint16_t)PM10;
    }

    env->status = status;
}
