#ifndef I2C_H
#define I2C_H
#include "stm32f4xx.h"

// 	引脚定义
//SCL	PF13
//SDA	PF14

//SCL		PF13
#define  	SCL_L      		GPIO_ResetBits(GPIOF, GPIO_Pin_13)
#define  	SCL_H      		GPIO_SetBits(GPIOF, GPIO_Pin_13)

//SDA		PF14
#define  	SDA_L      		GPIO_ResetBits(GPIOF, GPIO_Pin_14)
#define  	SDA_H      		GPIO_SetBits(GPIOF, GPIO_Pin_14)

#define  	SDA_READ()     	GPIO_ReadInputDataBit(GPIOF, GPIO_Pin_14)

//  函数声明
void I2C_GPIO_Configuration(void);
void I2C_SDA_Input(void);
void I2C_SDA_Output(void);
void I2C_delay(void);
void I2C_Start(void);
void I2C_Stop(void);
void I2C_SendACK(unsigned char ack);
void I2C_ReceiveACK(void);
void I2C_SendByte(unsigned char dat);
unsigned char I2C_ReceiveByte(void);

#endif
