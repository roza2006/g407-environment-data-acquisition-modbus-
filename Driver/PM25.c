#include "PM25.h"
#include "usart6.h"
#include "usart6_var.h"
#include "delay.h"

unsigned int PM25 = 0U;
unsigned int PM10 = 0U;
volatile uint8_t PM25_LastFrameValid = 0U;

void setPM25Mode(unsigned char mode)
{
    unsigned int check_sum;
    unsigned char cmd[7] = {0x42, 0x4D, 0xE1, 0x00, 0x00, 0x00, 0x00};
    unsigned char cnt;

    cmd[4] = mode;
    check_sum = FucCheckSum(cmd, 5U);
    cmd[5] = (unsigned char)(check_sum >> 8U);
    cmd[6] = (unsigned char)check_sum;

    Uart6RXlenth = 0U;
    Uart6ReceiveFinish = 0U;
    cnt = 5U;

    while (1)
    {
        USART6_Senddata(cmd, 7U);
        delay_ms(500U);

        if (Uart6ReceiveFinish == 1U)
        {
            if ((Uart6RXlenth == 8U) &&
                (Uart6ReceiveBuf[0] == 0x42U) &&
                (Uart6ReceiveBuf[1] == 0x4DU) &&
                (Uart6ReceiveBuf[4] == 0xE1U) &&
                (Uart6ReceiveBuf[5] == 0x00U))
            {
                check_sum = (unsigned int)Uart6ReceiveBuf[6] * 256U;
                check_sum += Uart6ReceiveBuf[7];

                if (check_sum == FucCheckSum(Uart6ReceiveBuf, 6U))
                {
                    return;
                }
            }

            Uart6RXlenth = 0U;
            Uart6ReceiveFinish = 0U;
        }

        cnt--;
        if (cnt == 0U)
        {
            return;
        }
    }
}

void PM25Collection(void)
{
    unsigned char request[7] = {0x42, 0x4D, 0xE2, 0x00, 0x00, 0x01, 0x71};
    unsigned int check_sum;
    unsigned int received_check_sum;

    PM25_LastFrameValid = 0U;
    Uart6RXlenth = 0U;
    Uart6ReceiveFinish = 0U;

    USART6_Senddata(request, 7U);
    delay_ms(100U);

    /* PMS response: 0x42 0x4D, frame length 0x001C, total 32 bytes. */
    if ((Uart6ReceiveFinish == 1U) &&
        (Uart6RXlenth >= 32U) &&
        (Uart6ReceiveBuf[0] == 0x42U) &&
        (Uart6ReceiveBuf[1] == 0x4DU) &&
        (Uart6ReceiveBuf[2] == 0x00U) &&
        (Uart6ReceiveBuf[3] == 0x1CU))
    {
        check_sum = FucCheckSum(Uart6ReceiveBuf, 30U);
        received_check_sum = (unsigned int)Uart6ReceiveBuf[30] * 256U;
        received_check_sum += Uart6ReceiveBuf[31];

        if (check_sum == received_check_sum)
        {
            PM25 = (unsigned int)Uart6ReceiveBuf[12] * 256U +
                   Uart6ReceiveBuf[13];
            PM10 = (unsigned int)Uart6ReceiveBuf[14] * 256U +
                   Uart6ReceiveBuf[15];
            PM25_LastFrameValid = 1U;
        }
    }

    Uart6RXlenth = 0U;
    Uart6ReceiveFinish = 0U;
}

unsigned int FucCheckSum(unsigned char *data, unsigned char len)
{
    unsigned char i;
    unsigned int sum = 0U;

    for (i = 0U; i < len; i++)
    {
        sum += data[i];
    }

    return sum;
}
