#include "usart3.h"

#define RS485_DIR_GPIO       GPIOA
#define RS485_DIR_PIN        GPIO_Pin_0

void UART3_Configuration(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    /* USART3: PC10 = TX, PC11 = RX */
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_USART3);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_USART3);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* PA0 = RS485 DE/RE: 1发送，0接收 */
    GPIO_InitStructure.GPIO_Pin = RS485_DIR_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(RS485_DIR_GPIO, &GPIO_InitStructure);
    GPIO_ResetBits(RS485_DIR_GPIO, RS485_DIR_PIN);

    USART_InitStructure.USART_BaudRate = 115200U;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART3, &USART_InitStructure);
    USART_ClearFlag(USART3, USART_FLAG_RXNE | USART_FLAG_TC);
    USART_Cmd(USART3, ENABLE);
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2U;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2U;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void UART3_SendBytes(uint8_t *buf, uint16_t len)
{
    uint16_t i;

    for (i = 0U; i < len; i++)
    {
        while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET)
        {
        }
        USART_SendData(USART3, buf[i]);
    }

    while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET)
    {
    }
}
