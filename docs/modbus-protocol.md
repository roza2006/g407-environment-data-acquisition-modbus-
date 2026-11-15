# Modbus RTU 协议说明 · Protocol Reference

> 本文档描述本节点对外提供的 Modbus RTU 从站接口。
> This document describes the Modbus RTU slave interface exposed by this node.

- 实现位置 Implementation: [`App/modbus_rtu.c`](../App/modbus_rtu.c)
- 数据来源 Data source: [`App/sensor_env.c`](../App/sensor_env.c)

---

## 1. 串口参数 · Serial Parameters

| 参数 Parameter | 值 Value |
| --- | --- |
| 从机地址 Slave address | `0x01`（编译期常量 `MODBUS_SLAVE_ADDR`） |
| 波特率 Baud rate | `115200` |
| 数据位 Data bits | `8` |
| 停止位 Stop bits | `1` |
| 校验 Parity | None |
| 物理层 Physical layer | RS485 半双工，2 线（A/B）+ 共地 |
| 收发方向 Direction | `PA0` 高 = 发送，低 = 接收 |

> 地址、波特率目前是编译期常量：改地址需修改 `App/modbus_rtu.h` 的 `MODBUS_SLAVE_ADDR` 并重新编译；改波特率需同时修改 `BSP/USART3.c` 中的 `USART_BaudRate`。

---

## 2. 帧结构 · Frame Format

Modbus RTU 帧之间以总线静默分隔，无起始/结束符。

```text
+---------+----------+------------------+--------+--------+
| 地址    | 功能码   | 数据域 Data      | CRC 低 | CRC 高 |
| 1 byte  | 1 byte   | N bytes          | 1 byte | 1 byte |
+---------+----------+------------------+--------+--------+
```

- CRC 为 Modbus CRC16，多项式 `0xA001`（`0x8005` 反射），初值 `0xFFFF`，**低字节先发送**。
- 多字节数值均为 **大端**（高字节在前），例如寄存器 `0x012C` 发送为 `01 2C`。

### 帧边界判定 Frame boundary

本实现不依赖固定长度，而是靠**静默时间**切帧：

1. USART3 收到一个字节 → `RXNE` 中断 → 存入 `mb_rx_buf`，重置并启动 `TIM7`；
2. 只要持续有字节，`TIM7` 不断被重置，不会溢出；
3. 总线静默 **1.75 ms**（`TIM7` 溢出）→ 置 `mb_frame_ready = 1`；
4. 主循环的 `Modbus_RTU_Poll()` 拷贝缓冲区、清标志、开始解析。

`TIM7` 计数：APB1 时钟 84 MHz ÷ 84 = 1 MHz，周期 1750 → 1.75 ms。

> Modbus 规范在 115200 bps 下要求 3.5 字符 ≈ 0.30 ms。本实现取 1.75 ms 是为了容忍 USB 转 485 适配器的分包延迟；代价是高速连续帧之间的最小间隔变大。若需要严格符合规范，可把 `App/modbus_rtu.c` 中的 `TIM_Prescaler` / `TIM_Period` 改为 300 µs。

---

## 3. 支持的功能码 · Supported Function Code

仅支持 `0x03`（Read Holding Registers），一次请求最多读 **4** 个寄存器。

| 功能码 FC | 名称 Name | 支持 Supported |
| --- | --- | --- |
| `0x03` | 读保持寄存器 Read Holding Registers | ✅ |
| `0x04` | 读输入寄存器 Read Input Registers | ❌ 返回异常 `0x01` |
| `0x06` / `0x10` | 写单个/多个寄存器 | ❌ 返回异常 `0x01` |

---

## 4. 寄存器映射 · Register Map

| 地址 Addr | 名称 Register | 单位 Unit | 范围 Range | 说明 Notes |
| --- | --- | --- | --- | --- |
| `0x0000` | PM2.5 | µg/m³ | 0–65535 | PMS5003「大气环境」换算值 |
| `0x0001` | PM10 | µg/m³ | 0–65535 | 同上 |
| `0x0002` | 照度 Illuminance | lx | 0–65535 | BH1750 原始值 × 10 ÷ 12，整数截断 |
| `0x0003` | 设备状态 Device Status | bitfield | — | 见下节 |

一次读取跨出 `0x0003` 会被判为**非法地址**（异常码 `0x02`），例如 `0x0003` 起读 2 个寄存器、或 `0x0000` 起读 5 个。

### 状态位 · Status Bits

| 位 Bit | 宏定义 Macro | 含义 Meaning |
| --- | --- | --- |
| `bit0` | `ENV_STATUS_PM25_ERROR` | PM2.5 数据错误 |
| `bit1` | `ENV_STATUS_PM10_ERROR` | PM10 数据错误 |
| `bit2` | `ENV_STATUS_BH1750_ERROR` | BH1750 数据异常（读到 `0xFFFF` 判定） |
| `bit3` | `ENV_STATUS_DATA_NOT_READY` | 上电后尚未完成第一次采集 |
| `bit4` | `ENV_STATUS_SENSOR_TIMEOUT` | 传感器响应超时 |

> 状态寄存器每次采集都会重新计算（`EnvSensor_Read()` 内清零后按实际结果置位），不是锁存报警。上电初始值为 `0x0008`，第一次采集成功后自动清除 `bit3`。
>
> 注意：`bit0` 与 `bit1` 是联动置位的——PMS 帧校验失败时两者同时置位并附带 `bit4`，因为一帧里同时包含 PM2.5 与 PM10。

---

## 5. 请求与应答 · Request / Response

### 5.1 读保持寄存器（`0x03`）

请求（固定 8 字节）：

```text
+------+------+----------+----------+----------+----------+------+------+
| ADDR | 0x03 | START_H  | START_L  | COUNT_H  | COUNT_L  | CL   | CH   |
+------+------+----------+----------+----------+----------+------+------+
```

应答（`5 + 2×COUNT` 字节）：

```text
+------+------+------------+------------------+------+------+
| ADDR | 0x03 | BYTE_COUNT | DATA ...         | CL   | CH   |
+------+------+------------+------------------+------+------+
```

`BYTE_COUNT = 2 × COUNT`，数据区按寄存器地址升序、每个寄存器 2 字节大端。

### 5.2 异常应答

```text
+------+-------------+------+------+------+
| ADDR | FC \| 0x80  | CODE | CL   | CH   |
+------+-------------+------+------+------+
```

| 异常码 Code | 名称 Name | 触发条件 When |
| --- | --- | --- |
| `0x01` | Illegal Function | 功能码不是 `0x03` |
| `0x02` | Illegal Address | 起始地址 ≥ 4，或 `起始地址 + 数量 > 4` |
| `0x03` | Illegal Data Value | 数量为 0，或应答长度超过 64 字节缓冲 |
| `0x04` | Slave Device Failure | 传入的传感器结构体指针为空（防御性判断） |

### 5.3 静默丢帧（不应答）

以下情况从站**完全不回包**，主站只会看到超时：

- CRC 校验失败
- 从机地址不匹配（包括广播地址 `0x00`——本实现不支持广播）
- 报文长度不是 8 字节（例如被截断、或带了多余字节）

这是符合规范的保守策略：总线上有其它设备时，避免本机对别人的报文抢答。

---

## 6. 示例帧 · Example Frames

以下 CRC 均已按 `0xA001` 实际计算，可直接对照抓包。

| 场景 Scenario | 帧 Frame |
| --- | --- |
| 读 4 个寄存器 Read all 4 registers | `01 03 00 00 00 04 44 09` |
| 正常应答（26 / 43 / 300 / 0） | `01 03 08 00 1A 00 2B 01 2C 00 00 CA 25` |
| 读单个寄存器（PM2.5） | `01 03 00 00 00 01 84 0A` |
| 读状态寄存器 | `01 03 00 03 00 01 74 0A` |
| 读 0x0002 起 2 个（合法） | `01 03 00 02 00 02 65 CB` |
| 读 0x0000 起 5 个 → 异常 `0x02` | 请求 `01 03 00 00 00 05 85 C9`，应答 `01 83 02 C0 F1` |
| 数量 0 → 异常 `0x03` | 请求 `01 03 00 00 00 00 45 CA`，应答 `01 83 03 01 31` |
| 功能码 0x04 → 异常 `0x01` | 请求 `01 04 00 00 00 01 ...`，应答 `01 84 01 82 C0` |
| 地址 `0x02` 的请求 → 静默 | `02 03 00 00 00 04 44 3A`（无应答） |

带故障状态的一次应答示例：PM2.5=`0x001A`、PM10=`0x002B`、照度=`0x012C`、状态=`0x0012`（PM 超时 + 传感器超时）：

```text
01 03 08 00 1A 00 2B 01 2C 00 12 <CRC>
```

---

## 7. CRC 计算 · CRC Calculation

```c
uint16_t Modbus_CRC16(const uint8_t *buf, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    while (len--) {
        crc ^= *buf++;
        for (uint8_t i = 0; i < 8; i++)
            crc = (crc & 1) ? ((crc >> 1) ^ 0xA001U) : (crc >> 1);
    }
    return crc;   /* 低字节先发送 */
}
```

以 `01 03 00 00 00 04` 为例，计算得 `0x0944`，发送顺序为 `44 09`。

Python 等价实现（`tools/modbus_poll.py` 内有同样逻辑，可 `--selftest` 自检）：

```python
def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc
```

---

## 8. 对接提示 · Integration Notes

- **寄存器是 16 位无符号整数**，组态软件里请按 `UInt16`、大端解析。
- PM2.5 / PM10 单位是 µg/m³，不是 `0.1 µg/m³`，无需再除以 10。
- 照度为整数 lx，分辨率约 1.2 lx；>65535 lx 时饱和。
- 轮询周期建议 ≥ 1 s：传感器本身 1 s 更新一次，更快读取只会拿到重复值。
- RS485 两端建议加 120 Ω 终端电阻；多节点时注意共地，避免共模电压超限。
