# 硬件与接线 · Hardware & Wiring

> 本文档汇总引脚分配、外设配置与接线注意事项，均以源码为准。
> Pin assignment, peripheral setup and wiring notes, as implemented in code.

---

## 1. 主控 · MCU

| 项目 Item | 参数 Value |
| --- | --- |
| 型号 Device | STM32F407ZGT6（LQFP144） |
| 内核 Core | ARM Cortex-M4F @ 168 MHz（`SystemCoreClock` 由 `system_stm32f4xx.c` 配置） |
| Flash / RAM | 1 MB / 192 KB + 64 KB CCM |
| 外部晶振 HSE | 8 MHz（工程默认） |
| 调试口 Debug | SWD（PA13/PA14） |
| 工程器件 Device | `STM32F407ZGTx` |

---

## 2. 引脚分配 · Pin Map

### 2.1 RS485 / Modbus（USART3）

| 引脚 Pin | 功能 Function | 配置 Config | 说明 Notes |
| --- | --- | --- | --- |
| `PC10` | USART3_TX | AF7，推挽，上拉 | 接 485 收发器 `DI` |
| `PC11` | USART3_RX | AF7，上拉 | 接 485 收发器 `RO` |
| `PA0` | RS485 方向 | 推挽输出，默认低 | 接 `DE` + `RE`（两者并联）：高 = 发送，低 = 接收 |

```c
/* BSP/USART3.c */
RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_USART3);
GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_USART3);
USART_InitStructure.USART_BaudRate = 115200U;
```

> `DE` 与 `RE` 通常并联接到 `PA0`：`DE=1` 发送、`RE=0` 使能接收，正好同一电平。如果模块把 `DE`、`RE` 分开引出，需要外加反相或改用两个 IO。
>
> 发送前 `Modbus_SendFrame()` 拉高 `PA0`，`UART3_SendBytes()` 阻塞发送完并等到 `TC` 置位后才拉低，因此不会截断最后一个字节——但也意味着发送期间主循环被阻塞（8 字节 @115200 ≈ 0.7 ms）。

### 2.2 颗粒物传感器 PMS5003（USART6）

| 引脚 Pin | 功能 Function | 配置 Config | 说明 Notes |
| --- | --- | --- | --- |
| `PC6` | USART6_TX | AF8，推挽 | 接传感器 `RX` |
| `PC7` | USART6_RX | AF8，上拉 | 接传感器 `TX` |
| — | 电源 Power | 5 V | PMS5003 需要 5 V 供电，逻辑电平 3.3 V 兼容 |

- 波特率 **9600**，8N1（`UART6_Configuration(9600)`）。
- 传感器引脚丝印因厂家而异，常见为 `VCC/GND/TX/RX/SET/RESET`，务必**交叉连接**（MCU TX → 传感器 RX）。
- 上电后 `setPM25Mode(0)` 发送 `42 4D E1 00 00 00 <CRC>` 切到**被动（问答）模式**，之后每次 `PM25Collection()` 发 `42 4D E2 00 00 01 71` 请求一帧，32 字节应答中取 `buf[12..13]` 为 PM2.5、`buf[14..15]` 为 PM10。
- 若传感器停留在主动模式，`PM25Collection()` 会收到长度/帧头不符的报文，状态位会置 `bit0|bit1|bit4`。

### 2.3 光照传感器 BH1750（软件 I2C）

| 引脚 Pin | 功能 Function | 配置 Config | 说明 Notes |
| --- | --- | --- | --- |
| `PF13` | SCL | 推挽输出 | 需外接 4.7 kΩ 上拉至 3.3 V |
| `PF14` | SDA | 输出/输入动态切换 | 同上 |
| — | 地址 Address | `0x46`（写）/ `0x47`（读） | 即 7 位地址 `0x23`，`ADDR` 脚接地 |

```c
/* Driver/BH1750.h */
#define BH1750_SlaveAddress 0x46
```

- 初始化：`0x01`（上电）→ 延时 100 ms → `0x10`（连续 H-Resolution 模式，1 lx 分辨率，约 120 ms 一次转换）。
- 读数换算：`lux = raw * 10 / 12`（整数运算，等价于 `raw / 1.2`）。
- 注意：该驱动为**位翻转软件 I2C**，`SCL/SDA` 配置为推挽输出，靠外部上拉与方向切换模拟开漏；总线上若没有上拉电阻会读不到数据。

### 2.4 显示屏（FSMC 并口）

| 信号 Signal | 引脚 Pin | 说明 Notes |
| --- | --- | --- |
| `D0–D3` | `PD14, PD15, PD0, PD1` | FSMC 数据线 |
| `D4–D7` | `PD8, PD9, PD10, PE15` | 同上 |
| `D8–D15` | `PE7 – PE14` | 同上 |
| `NE4` (CS) | `PG12` | FSMC Bank1 NOR/SRAM4 |
| `A11` (RS/DC) | `PG1` | 0 = 命令，1 = 数据 |
| `BL` 背光 | `PA9` | 推挽输出，高电平点亮 |
| `WR / RD` | `PD5 / PD4` | FSMC 自动驱动 |

- 访问基地址：`0x6C000000 | 0x00000FFE`，即 Bank1 + NE4 + A11 偏移。
- 分辨率 **480×272**，16 位 RGB565。
- 时序：读 `ADDSET=15 / DATAST=60`，写 `ADDSET=9 / DATAST=8`（HCLK 168 MHz）。
- 屏幕型号为 SSD1963 类控制器（`LCD_Init()` 中写 `0xE0` 起始的初始化序列）。换屏需要改初始化表。

### 2.5 板载 LED（可选）

`PF0–PF7` 对应 LED1–LED8，低电平点亮（`Driver/LED.h`）。当前应用未使用，保留作调试指示。

---

## 3. 接线示意 · Wiring

```text
                    +3.3V
                      │
        ┌─────────────┴─────────────┐
        │        STM32F407          │
        │                           │
   PC10 ├──► DI ──┐                 │
   PC11 ◄── RO ──┤  MAX3485         │
    PA0 ├──► DE+RE┘                 │
        │        A ──────┬──────────┼──── 上位机 485 A
        │        B ──────┴──────────┼──── 上位机 485 B
        │         GND ──────────────┼──── 共地（必须）
        │                           │
    PC6 ├──► PMS5003 RX             │
    PC7 ◄── PMS5003 TX             │
        │                           │
  PF13  ├──► BH1750 SCL（4.7k 上拉）│
  PF14  ├──◄► BH1750 SDA（4.7k 上拉）│
        └───────────────────────────┘
```

要点：

1. **共地**：RS485 A/B 是差分信号，但两端必须共地，否则共模电压可能超出收发器范围。
2. **终端电阻**：长线（>10 m）或高波特率时，在总线**两端**各并 120 Ω；短距离实验可不加。
3. **A/B 极性**：不同厂家丝印 A/B 定义可能相反，通信不通时先对调 A/B 试一次。
4. **PMS 电源**：PMS5003 峰值电流约 100 mA，建议独立 5 V 供电，不要从调试器取电。
5. **USB 转 485**：常见 CH340/CP2102 模块为半双工自适应，不需要额外控制线；注意其驱动在 Windows 上的 COM 口号。

---

## 4. 上电表现 · Power-on Behaviour

| 阶段 Phase | 现象 Behaviour |
| --- | --- |
| 上电 0–200 ms | 屏幕背光点亮，清屏，显示标题 `SAMPLE MODBUS?RTU` |
| ≈200 ms | PMS5003 进入被动模式（最多重试 5 次，每次间隔 500 ms） |
| 之后每秒 | 读 BH1750 → 请求一次 PMS → 刷新三行读数 |
| 任意时刻 | 收到合法 `0x03` 请求立即应答（不等待采集周期） |

若屏幕一直黑屏：先查背光 `PA9` 与 FSMC 接线；若屏幕有字但读数为 0：查传感器供电与 `bit3` 是否仍为 1。
