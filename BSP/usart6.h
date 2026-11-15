#ifndef __USART6_H
#define __USART6_H
#include "stm32f4xx.h"

void UART6_Configuration(unsigned int baud);
void USART6_Senddata(unsigned char *Data, unsigned int length);

#endif
