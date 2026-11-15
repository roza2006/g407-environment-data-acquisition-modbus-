/*Modbus RTU 通信说明*/
/*关于通信参数：从机地址	0x01
			   波特率	115200
			   数据位	8
			   停止位	1
               校验位	无
			   支持功能码	0x03 读取保持寄存器
			   CRC	Modbus CRC16，低字节先发送*/
/*寄存器表格如下：地址	内容
				0x0000	PM2.5	
				0x0001	PM10	
				0x0002	照度	
				0x0003	设备状态*/
/*一些初步实现的异常提示：
bit0	PM2.5 数据错误
bit1	PM10 数据错误
bit2	BH1750 数据异常
bit3	数据尚未完成第一次采集
bit4	传感器响应超时*/	
/*读取指令及结果示例：
01 03 00 00 00 04 44 09

01 03 08
PM25_H PM25_L
PM10_H PM10_L
ILL_H ILL_L
STATUS_H STATUS_L
CRC_L CRC_H*/

//另：对于CRC错误、从机地址错误、报文不完整的情况，从机之作静默，不予回复

#include "stm32f4xx.h"
#include "delay.h"
#include "I2C.h"
#include "BH1750.h"
#include "PM25.h"
#include "lcd.h"
#include "sensor_env.h"
#include "modbus_rtu.h"
#include "usart6.h"
#include "usart3.h"

unsigned char screen_refresh_flag = 0;//屏幕1秒刷新1次的标志位

int main(void)
{  

    char disbuf[64] = {0};//屏幕字符串缓存
    EnvSensor_Typedef env;//传感器结构体

    EnvSensor_InitData(&env);

    SysTick_Init();

	GPIO_InitTypeDef GPIO_InitStruct;
RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;//收发控制引脚
GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
GPIO_Init(GPIOA, &GPIO_InitStruct);
GPIO_ResetBits(GPIOA,GPIO_Pin_0); //默认拉低到接受模式，这里是考虑到这个主从模式，从机收到请求后才可能回复
	/*USART3 负责 Modbus 报文的串行数据收发，但是 RS485 芯片是半双工，需要一个 IO 引脚控制芯片切换接收和发送模式，所以额外使用 PA0 普通 GPIO，和 USART3 配合完成 RS485 通信。*/
	/*这里其实我没太懂，但是没这个跑不了*/
    I2C_GPIO_Configuration();
    BH1750_Init();
    UART6_Configuration(9600);
    setPM25Mode(0);

    UART3_Configuration();
    Modbus_RTU_Init();

    LCD_Init();
    LCD_Clear(WHITE);
    LCD_ShowString(20, 10, "SAMPLE MODBUS?RTU", BLACK, WHITE);

    while(1)
    {   
        Modbus_RTU_Poll(&env);//MODBUS轮询

        if(screen_refresh_flag == 1)
        {
            screen_refresh_flag = 0;
            EnvSensor_Read(&env);

            sprintf(disbuf,"PM2.5: %3d ug/m3", env.pm25);
            LCD_ShowString(20,40,(uint8_t*)disbuf,BLACK,WHITE);
            sprintf(disbuf,"PM10 : %3d ug/m3", env.pm10);
            LCD_ShowString(20,70,(uint8_t*)disbuf,BLACK,WHITE);
            sprintf(disbuf,"Illu : %5d Lux", env.illuminance);
            LCD_ShowString(20,100,(uint8_t*)disbuf,BLACK,WHITE);
        }
    }
}
