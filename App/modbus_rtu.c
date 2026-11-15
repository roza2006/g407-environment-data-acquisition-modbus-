#include "modbus_rtu.h"
#include "usart3.h"
#include "string.h"

#define MB_TX_BUF_LEN 64U

volatile uint8_t  mb_rx_buf[MB_RX_BUF_LEN];
volatile uint16_t mb_rx_idx = 0U;
volatile uint8_t  mb_frame_ready = 0U;

uint16_t Modbus_CRC16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint8_t bit_index;

    while (len > 0U)
    {
        crc ^= (uint16_t)(*buf);
        buf++;
        len--;

        for (bit_index = 0U; bit_index < 8U; bit_index++)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = (crc >> 1U) ^ 0xA001U;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

static void Modbus_SendFrame(uint8_t *tx_buf, uint16_t data_len)
{
    uint16_t crc;
    uint16_t tx_len = data_len;

    crc = Modbus_CRC16(tx_buf, data_len);
    tx_buf[tx_len++] = (uint8_t)(crc & 0x00FFU);
    tx_buf[tx_len++] = (uint8_t)((crc >> 8U) & 0x00FFU);

    GPIO_SetBits(GPIOA, GPIO_Pin_0);
    UART3_SendBytes(tx_buf, tx_len);
    GPIO_ResetBits(GPIOA, GPIO_Pin_0);
}

static void Modbus_SendException(uint8_t function, uint8_t exception_code)
{
    uint8_t tx_buf[5];

    tx_buf[0] = MODBUS_SLAVE_ADDR;
    tx_buf[1] = function | 0x80U;
    tx_buf[2] = exception_code;

    Modbus_SendFrame(tx_buf, 3U);
}

static void Modbus_ParseFrame(uint8_t *rxbuf,
                              uint16_t rxlen,
                              EnvSensor_Typedef *pEnv)
{
    uint8_t tx_buf[MB_TX_BUF_LEN];
    uint16_t reg_arr[MODBUS_REG_COUNT];
    uint16_t crc_calc;
    uint16_t crc_recv;
    uint16_t start_addr;
    uint16_t reg_cnt;
    uint16_t tx_data_len;
    uint16_t i;

    
    if (rxlen != 8U)
    {
        return;
    }

    crc_calc = Modbus_CRC16(rxbuf, rxlen - 2U);
    crc_recv = (uint16_t)rxbuf[rxlen - 2U] |
               ((uint16_t)rxbuf[rxlen - 1U] << 8U);

    
    if ((crc_calc != crc_recv) || (rxbuf[0] != MODBUS_SLAVE_ADDR))
    {
        return;
    }

    if (rxbuf[1] != 0x03U)
    {
        Modbus_SendException(rxbuf[1], MODBUS_EX_ILLEGAL_FUNCTION);
        return;
    }

    start_addr = ((uint16_t)rxbuf[2] << 8U) | (uint16_t)rxbuf[3];
    reg_cnt = ((uint16_t)rxbuf[4] << 8U) | (uint16_t)rxbuf[5];

    if (reg_cnt == 0U)
    {
        Modbus_SendException(0x03U, MODBUS_EX_ILLEGAL_VALUE);
        return;
    }

    if ((start_addr >= MODBUS_REG_COUNT) ||
        (reg_cnt > (MODBUS_REG_COUNT - start_addr)))
    {
        Modbus_SendException(0x03U, MODBUS_EX_ILLEGAL_ADDRESS);
        return;
    }

    if ((3U + reg_cnt * 2U + 2U) > MB_TX_BUF_LEN)
    {
        Modbus_SendException(0x03U, MODBUS_EX_ILLEGAL_VALUE);
        return;
    }

    if (pEnv == 0)
    {
        Modbus_SendException(0x03U, MODBUS_EX_DEVICE_FAILURE);
        return;
    }

    __disable_irq();
    reg_arr[MODBUS_REG_PM25] = pEnv->pm25;
    reg_arr[MODBUS_REG_PM10] = pEnv->pm10;
    reg_arr[MODBUS_REG_ILLUMINANCE] = pEnv->illuminance;
    reg_arr[MODBUS_REG_DEVICE_STATUS] = pEnv->status;
    __enable_irq();

    tx_buf[0] = MODBUS_SLAVE_ADDR;
    tx_buf[1] = 0x03U;
    tx_buf[2] = (uint8_t)(reg_cnt * 2U);

    for (i = 0U; i < reg_cnt; i++)
    {
        tx_buf[3U + i * 2U] =
            (uint8_t)(reg_arr[start_addr + i] >> 8U);
        tx_buf[4U + i * 2U] =
            (uint8_t)(reg_arr[start_addr + i] & 0x00FFU);
    }

    tx_data_len = 3U + reg_cnt * 2U;
    Modbus_SendFrame(tx_buf, tx_data_len);
}

void Modbus_RTU_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, ENABLE);

    /* TIM7 clock 84 MHz / 84 = 1 MHz; 1750 counts = 1.75 ms */
    TIM_TimeBaseStructure.TIM_Prescaler = 83U;
    TIM_TimeBaseStructure.TIM_Period = 1749U;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM7, &TIM_TimeBaseStructure);

    TIM_ClearITPendingBit(TIM7, TIM_IT_Update);
    TIM_ITConfig(TIM7, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM7, DISABLE);

    NVIC_InitStructure.NVIC_IRQChannel = TIM7_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2U;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3U;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    mb_rx_idx = 0U;
    mb_frame_ready = 0U;
    memset((void *)mb_rx_buf, 0, MB_RX_BUF_LEN);
    GPIO_ResetBits(GPIOA, GPIO_Pin_0);
}

void Modbus_RTU_Poll(EnvSensor_Typedef *pEnv)
{
    uint8_t frame_buf[MB_RX_BUF_LEN];
    uint16_t frame_len;
    uint16_t i;

    if (mb_frame_ready == 0U)
    {
        return;
    }

    __disable_irq();

    frame_len = mb_rx_idx;
    if (frame_len > MB_RX_BUF_LEN)
    {
        frame_len = MB_RX_BUF_LEN;
    }

    for (i = 0U; i < frame_len; i++)
    {
        frame_buf[i] = mb_rx_buf[i];
    }

    mb_rx_idx = 0U;
    mb_frame_ready = 0U;
    TIM_Cmd(TIM7, DISABLE);
    TIM_SetCounter(TIM7, 0U);
    TIM_ClearITPendingBit(TIM7, TIM_IT_Update);

    __enable_irq();

    Modbus_ParseFrame(frame_buf, frame_len, pEnv);
    memset((void *)mb_rx_buf, 0, MB_RX_BUF_LEN);
}

uint8_t Modbus_CRC_SelfTest(void)
{
    static const uint8_t test_data[6] =
    {
        0x01U, 0x03U, 0x00U, 0x00U, 0x00U, 0x03U
    };

    return (Modbus_CRC16(test_data, 6U) == 0xCB05U) ? 1U : 0U;
}
