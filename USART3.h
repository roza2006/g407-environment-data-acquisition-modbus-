#ifndef __USART3_H
#define __USART3_H

#include "stm32f4xx.h"

void UART3_Configuration(void);
void UART3_SendBytes(uint8_t *buf, uint16_t len);

#endif
