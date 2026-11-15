# 已知问题与路线 · Known Issues & Roadmap

> 本页如实记录当前代码的不足。**本轮整理只调整目录、补工程与文档，没有改动任何源码行为**，因此下列问题依旧存在。
> This page honestly lists current shortcomings. **This cleanup round only reorganised files and added the project/docs — no source behaviour was changed**, so the issues below still apply.

---

## 1. 会影响功能的 BUG · Functional Bugs

### 1.1 BH1750 读取了 3 个字节，且最后一个字节回 ACK（高）

`Driver/BH1750.c:32`：

```c
for (i = 0; i < 3; i++)
{
    ptr[i] = I2C_ReceiveByte();
    if (i == 3)            /* 永远不会成立 */
    { I2C_SendACK(1); }    /* 最后一个字节应回 NACK */
    else
    { I2C_SendACK(0); }
}
```

BH1750 一次只返回 2 字节。当前实现会多读 1 个字节，并在读最后一个有效字节后仍回 ACK，不符合 I2C 规范（主机在最后一个字节应回 NACK 再发 STOP）。

实际影响：多数情况下第三个字节读到总线噪声，`data_buf[0..1]` 仍正确，所以功能"看起来正常"；但在总线电容大、上拉弱时可能读到错位数据，并把 SDA 拉死在低电平。

**建议改法**（未改动）：

```c
for (i = 0; i < 2; i++)
{
    ptr[i] = I2C_ReceiveByte();
    I2C_SendACK(i == 1 ? 1 : 0);
}
```

### 1.2 USART6 接收中断没有边界检查（中）

`User/stm32f4xx_it.c:47`：

```c
Uart6ReceiveBuf[Uart6RXlenth] = USART_ReceiveData(USART6);
Uart6RXlenth++;
```

`Uart6ReceiveBuf` 只有 300 字节。PMS 若因干扰持续输出（或波特率错误导致乱码连续流入），会写越界踩坏相邻内存（`Uart6Time`、`Uart6ReceiveFinish` 等），表现为偶发死机或采集永久失败。

**建议**：在写入前判断 `if (Uart6RXlenth < sizeof(Uart6ReceiveBuf))`，并在溢出时丢弃本帧。

### 1.3 状态位 `bit0` / `bit1` 永远同时置位（低）

`App/sensor_env.c:34`：PMS 帧无效时同时置 `PM25_ERROR`、`PM10_ERROR`、`SENSOR_TIMEOUT`。因为一帧里同时含两个值，无法区分是哪一个错——这是设计上的取舍，但会让「按位定位故障」失去意义。若想区分，需要保留上一帧有效值与时间戳。

### 1.4 应答缓冲长度检查是死代码（低）

`App/modbus_rtu.c:114` 的 `(3 + cnt*2 + 2) > 64` 永远不成立：前面 `start_addr + reg_cnt <= 4` 的检查已经把 `reg_cnt` 限制到 4。留着无害，但会误导读者，可删除或改为 `MODBUS_REG_COUNT` 的静态断言。

---

## 2. 健壮性与规范差距 · Robustness & Spec Gaps

| 项目 Item | 现状 Current | 影响 Impact |
| --- | --- | --- |
| 帧静默阈值 | 固定 1.75 ms（`App/modbus_rtu.c:157`），规范要求 3.5 字符 ≈ 0.30 ms @115200 | 更宽容，但限制高速连续帧；对严格主站可能表现为响应偏慢 |
| 广播地址 `0x00` | 不支持（地址不匹配即静默） | 当前无写功能，暂不影响；将来加 `0x06/0x10` 时需支持 |
| 功能码 | 只有 `0x03` | 无法在线读设备信息（`0x11`）、改地址/阈值 |
| 长度校验 | 仅接受正好 8 字节 | 部分主站会附加调试字节或分包，导致丢帧 |
| 应答延迟 | 阻塞发送 ≈0.7 ms，采集窗口内最长 ≈100 ms | 主站超时建议 ≥200 ms |
| 看门狗 | 未启用 IWDG | 程序跑飞无法自恢复 |
| CRC 自检 | `Modbus_CRC_SelfTest()` 已实现但从未被调用 | 无法在启动时自证 CRC 正确 |
| 传感器超时重试 | PMS 单次 100 ms 等待，失败即置错误位 | 无重试、无滑动平均，读数会抖动 |

---

## 3. 工程与可维护性 · Engineering Hygiene

### 3.1 源码编码混杂（中）

以下文件是 **GBK** 编码（中文注释），其余为 UTF-8：

```text
BSP/delay.c  Display/font.h  Display/lcd.c  Display/lcd.h
Driver/BH1750.c  Driver/BH1750.h  Driver/I2C.c  Driver/I2C.h
Driver/LED.c  Driver/LED.h
```

后果：GitHub 网页上这些文件的中文注释显示为乱码；VS Code 需手动指定编码。

**建议**（本轮未做，避免产生巨大的"伪改动" diff）：统一转 UTF-8（Keil 的 `Edit → Configuration → Encoding` 需同步设为 UTF-8），或加 `.gitattributes` 的 `working-tree-encoding`。

### 3.2 文件名大小写不统一（低）

`BSP/USART3.c` / `USART3.h` 与 `usart6.c` / `usart6_var.c` 混用大小写，`#include "usart3.h"` 与文件名 `USART3.h` 不一致。Windows 与 Keil 大小写不敏感，因此可编译；但迁移到 Linux/GCC 会直接找不到头文件。

### 3.3 注释与文案待清理（低）

- `App/main.c:65`：`/*这里其实我没太懂，但是没这个跑不了*/`
- `App/main.c:76`：开机界面 `"SAMPLE MODBUS?RTU"`（`?` 应为空格或 `-`）
- `User/stm32f4xx_it.c:42`：`void USART6_IRQHandler(void)/*没用，不用管*/`——实际上它是 PMS 收数的关键中断
- `Driver/LED.h:2`：`#define _GIPO_H_` 拼写错误（应为 `_GPIO_H_`），且 `#ifndef` 守卫与宏名不一致

### 3.4 未使用的代码（低）

- `Driver/LED.c/.h`：应用未调用
- `Modbus_CRC_SelfTest()`：从未调用
- `Libraries/STM32F4xx_StdPeriph_Driver` 整个库都编进工程（34 个 `.c`），实际只用到 `rcc/gpio/usart/tim/misc`，其余增加编译时间，链接器会丢弃未引用段，故不影响 Flash 占用

### 3.5 字库以头文件形式存在（低）

`Display/Hzk16song.h` 是 1.6 MB 的 `const` 数组声明，导致 RO-data 274 KB、IDE 打开慢。可改为独立 `.c` 或在外部 Flash 中存放。

### 3.6 仓库元信息（需在 GitHub 上手动改）

仓库名 `g407-environment-data-acquisition-modbus-` 与描述里的 **"STM32G407"** 都不准确：芯片实际是 **STM32F407**（G4 是另一个系列）；名字结尾还多了一个 `-`。

建议（不改历史，只改 GitHub 设置）：

- Description：`STM32F407 environmental sensing node: PM2.5 / PM10 / illuminance, Modbus RTU slave over RS485, 4.3" TFT display.`
- 可选：Rename 为 `stm32f407-modbus-env-node`（GitHub 会自动重定向旧链接）

---

## 4. 建议的修复顺序 · Suggested Fix Order

1. **1.1 BH1750 读 2 字节 + NACK**（10 行内改动，收益最大）
2. **1.2 USART6 越界防护**（3 行）
3. **3.1 编码统一为 UTF-8**（一次性，注意 Keil 设置）
4. 3.3 注释/文案清理、3.2 文件名统一小写
5. 加 IWDG、CRC 启动自检
6. 扩展 `0x04`/`0x06`/`0x10` 与阈值持久化
7. 补主机侧 pytest（CRC、帧解析、异常码）

---

## 5. 路线图 · Roadmap

- [ ] 修复 1.1 / 1.2，并在 `main()` 中调用 `Modbus_CRC_SelfTest()`
- [ ] 支持 `0x04` 输入寄存器、`0x06`/`0x10` 写配置寄存器
- [ ] 传感器数据滑动平均（8 点）+ 通信失联指示（LED/状态位）
- [ ] IWDG 看门狗，超时复位
- [ ] 源码编码统一、文件名统一小写、跨平台 GCC 构建（CMake）
- [ ] 主机侧自动化测试与 CI（GitHub Actions 仅跑 Python 侧测试与静态检查）
