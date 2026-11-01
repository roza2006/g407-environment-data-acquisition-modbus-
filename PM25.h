#ifndef __PM25_H
#define __PM25_H

#include "stm32f4xx.h"

extern unsigned int PM25;
extern unsigned int PM10;
extern volatile uint8_t PM25_LastFrameValid;

void setPM25Mode(unsigned char mode);
void PM25Collection(void);
unsigned int FucCheckSum(unsigned char *i, unsigned char ln);

#endif
