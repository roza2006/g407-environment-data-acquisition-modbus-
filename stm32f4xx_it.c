#include "stm32f4xx_it.h"
#include "delay.h"
#include "usart6_var.h"
#include "modbus_rtu.h"
/*上位机从 RS485 发报文 → USART3 收到 1 字节 → 触发中断，存入`mb_rx_buf`。
每收到一个字节，重置 TIM7。只要持续收到字节，TIM7 一直被重置，不会进中断。
总线安静一段时间（3.5 字符时间），没有新字节，TIM7 计数溢出触发中断。`mb_frame_ready=1`，主循环里`Modbus_RTU_Poll()`检测这个标志，开始解析 Modbus 报文。*/
extern unsigned char screen_refresh_flag;

void NMI_Handler(void){}
void HardFault_Handler(void){while(1);}
void MemManage_Handler(void){while(1);}
void BusFault_Handler(void){while(1);}
void UsageFault_Handler(void){while(1);}
void SVC_Handler(void){}
void DebugMon_Handler(void){}
void PendSV_Handler(void){}

void SysTick_Handler(void)
{
    static uint32_t screen_cnt = 0;

    screen_cnt++;
    if (screen_cnt >= 1000U)
    {
        screen_cnt = 0;
        screen_refresh_flag = 1;
    }

    if (Uart6Time > 0)
    {
        Uart6Time--;
        if (Uart6Time == 0)
        {
            Uart6ReceiveFinish = 1;
        }
    }

    TimingDelay_Decrement();
}

void USART6_IRQHandler(void)/*没用，不用管*/
{
    if (USART_GetITStatus(USART6, USART_IT_RXNE) != RESET)
    {
        USART_ClearITPendingBit(USART6, USART_IT_RXNE);
        Uart6ReceiveBuf[Uart6RXlenth] = USART_ReceiveData(USART6);
        Uart6RXlenth++;
        Uart6Time = 5;
    }
}

void USART3_IRQHandler(void)
{
    uint8_t ch;

    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        /* 读取 DR 后 RXNE 自动清除。 */
        ch = (uint8_t)USART_ReceiveData(USART3);

        /* 主循环尚未处理上一帧时，不覆盖旧数据。 */
        if (mb_frame_ready == 0U)
        {
            if (mb_rx_idx < MB_RX_BUF_LEN)
            {
                mb_rx_buf[mb_rx_idx] = ch;
                mb_rx_idx++;

                /* 每收到一个字节，重新计算帧静默时间。 */
                TIM_Cmd(TIM7, DISABLE);
                TIM_SetCounter(TIM7, 0U);
                TIM_ClearITPendingBit(TIM7, TIM_IT_Update);
                TIM_Cmd(TIM7, ENABLE);
            }
            else
            {
                /* 缓冲区溢出：丢弃本帧。 */
                mb_rx_idx = 0U;
                TIM_Cmd(TIM7, DISABLE);
                TIM_SetCounter(TIM7, 0U);
                TIM_ClearITPendingBit(TIM7, TIM_IT_Update);
            }
        }
    }

    /* 读取 SR 和 DR，清除可能出现的溢出错误。 */
    if (USART_GetFlagStatus(USART3, USART_FLAG_ORE) != RESET)
    {
        volatile uint16_t temp;
        temp = USART3->SR;
        temp = USART3->DR;
        (void)temp;
    }
}

void TIM7_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM7, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM7, TIM_IT_Update);
        TIM_Cmd(TIM7, DISABLE);
        TIM_SetCounter(TIM7, 0U);

        if (mb_rx_idx > 0U)
        {
            mb_frame_ready = 1U;
        }
    }
}
