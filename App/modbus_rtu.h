#ifndef __MODBUS_RTU_H
#define __MODBUS_RTU_H

#include "stm32f4xx.h"
#include "sensor_env.h"

#define MB_RX_BUF_LEN             64U
#define MODBUS_SLAVE_ADDR         0x01U

/* Holding register map. */
#define MODBUS_REG_PM25           0x0000U
#define MODBUS_REG_PM10           0x0001U
#define MODBUS_REG_ILLUMINANCE    0x0002U
#define MODBUS_REG_DEVICE_STATUS  0x0003U
#define MODBUS_REG_COUNT          4U

/* Standard Modbus exception codes used by this slave. */
#define MODBUS_EX_ILLEGAL_FUNCTION    0x01U
#define MODBUS_EX_ILLEGAL_ADDRESS     0x02U
#define MODBUS_EX_ILLEGAL_VALUE       0x03U
#define MODBUS_EX_DEVICE_FAILURE      0x04U

extern volatile uint8_t  mb_rx_buf[MB_RX_BUF_LEN];
extern volatile uint16_t mb_rx_idx;
extern volatile uint8_t  mb_frame_ready;

uint16_t Modbus_CRC16(const uint8_t *buf, uint16_t len);
void Modbus_RTU_Init(void);
void Modbus_RTU_Poll(EnvSensor_Typedef *pEnv);
uint8_t Modbus_CRC_SelfTest(void);

#endif
