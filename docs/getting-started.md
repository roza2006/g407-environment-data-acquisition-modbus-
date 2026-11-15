# 快速开始 · Getting Started

> 从零到读出数据的完整流程。
> From a fresh clone to the first register read.

---

## 1. 环境准备 · Prerequisites

| 工具 Tool | 版本 Version | 用途 Purpose |
| --- | --- | --- |
| Keil MDK-ARM | 5.x（实测 5.38） | 编译、下载 |
| ARM Compiler 5 | 5.06 update 5 | 本工程使用 AC5（`--c99`），**不是** AC6 |
| ST-Link / J-Link | 任意 | 烧录与调试 |
| Python | 3.8+ | 上位机联调脚本 |
| pyserial | 3.5+ | `pip install pyserial` |

> 仓库内已包含 CMSIS、ST 标准外设库与 Keil 工程，**无需**再安装 STM32 固件包（DFP）。
> 如果 Keil 提示找不到 AC5：`Manage Project Items` → `Folders/Extensions` 里把编译器切到 `Use default compiler version 5`，或在 Keil 官网单独安装 ARM Compiler 5。

---

## 2. 编译 · Build

### 命令行

```powershell
# 仓库根目录执行
"C:\Keil_v5\UV4\UV4.exe" -b "Project\G407_EnvNode.uvprojx" -j0 -o "Project\build.log"
Get-Content Project\build.log
```

期望输出：

```text
Program Size: Code=19500  RO-data=274128  RW-data=76  ZI-data=3452
".\Obj\G407_EnvNode.axf" - 0 Error(s), 0 Warning(s).
```

### 图形界面

1. 打开 `Project/G407_EnvNode.uvprojx`
2. 确认目标为 `G407_EnvNode`，器件 `STM32F407ZGTx`
3. `F7` 编译，`Project\Obj\G407_EnvNode.hex` 即为产物

> 编译过一次后建议先跑 `Project/keilkill.bat`（或手动删 `Project/Obj`、`Project/List`）再重新编译，避免残留中间文件。这两个目录已在 `.gitignore` 中忽略。

---

## 3. 下载 · Flash

- **Keil 内下载**：`Options for Target → Debug` 选择你的调试器（ST-Link Debugger / J-LINK），`Flash Download` 勾选 `Reset and Run`，然后 `F8`。
- **命令行**：

```powershell
# ST-Link
& "C:\Keil_v5\ARM\STLink\ST-LINK_CLI.exe" -c SWD -P "Project\Obj\G407_EnvNode.hex" -V -Rst
```

下载后按 `docs/hardware.md` 接好线，屏幕上应出现三行读数。

---

## 4. 上位机联调 · Host Bring-up

### 4.1 用现成工具

- **Modbus Poll**（Windows）：`Connection → Connect` 选 COM 口，`115200 / 8 / None / 1`，Slave ID `1`，Function `03`，Address `0`，Quantity `4`，Scan Rate 1000 ms。
- **QModMaster**（跨平台）：Mode `RTU`，同理配置。

### 4.2 用仓库自带脚本

```bash
pip install pyserial
python tools/modbus_poll.py --port COM5 --interval 1
```

输出示例：

```text
#1  2026-11-15 22:41:03  PM2.5=26 µg/m³  PM10=43 µg/m³  Lux=300  Status=0x0000
```

其他用法：

```bash
python tools/modbus_poll.py --selftest          # 无硬件，校验 CRC 与示例帧
python tools/modbus_poll.py --port COM5 -n 10   # 采 10 次后退出
python tools/modbus_poll.py --port COM5 --raw   # 打印原始收发字节
```

### 4.3 用串口助手手动验证

发送十六进制：

```text
01 03 00 00 00 04 44 09
```

应收到：

```text
01 03 08 00 1A 00 2B 01 2C 00 00 CA 25
```

（数值随环境变化，但长度恒为 13 字节。）

---

## 5. 排错 · Troubleshooting

| 现象 Symptom | 可能原因 Cause | 处理 Action |
| --- | --- | --- |
| 完全无应答，串口助手超时 | A/B 接反、未共地、485 模块方向脚接错 | 对调 A/B；确认 `PA0` 接 `DE+RE`；万用表量 A-B 差分电压（空闲时应 >200 mV） |
| 有乱码回包 | 波特率不匹配 | 确认 115200 8N1，无校验 |
| 收到 `01 83 02` | 读的地址越界 | 只读 `0x0000–0x0003`，且数量不超过 4 |
| 收到 `01 83 01` | 功能码不是 0x03 | 用 FC03，不要用 FC04 |
| 无应答但 CRC 正确 | 从机地址不是 `0x01`，或长度不是 8 字节 | 检查报文；本机不支持广播 `0x00` |
| 状态 `bit3` 一直为 1 | 第一次采集未完成 | 等一个采集周期（约 1 s）；若一直不变，说明 PMS/BH1750 都没读到 |
| 状态 `bit0/bit1/bit4` | PMS 未进入被动模式或未接 | 检查 `PC6/PC7` 交叉连接与 5 V 供电；复位后重新上电让 `setPM25Mode` 生效 |
| 状态 `bit2` | BH1750 返回 `0xFFFF` | 检查 `PF13/PF14` 上拉电阻与地址脚 |
| 屏幕白屏/花屏 | FSMC 接线或初始化时序不匹配 | 检查 `PG12`(CS)、`PG1`(RS)、数据线；确认屏幕控制器兼容 `LCD_Init()` |
| 编译报 `stm32f4xx.h` 找不到 | 头文件路径丢失 | 确认 `Options → C/C++ → Include Paths` 含 `Libraries/CMSIS/Include`、`Libraries/CMSIS/Device/ST/STM32F4xx/Include`、`Libraries/STM32F4xx_StdPeriph_Driver/inc` 与 `App/BSP/Driver/Display/User` |
| 编译报 `core_cm4.h` / `__CC_ARM` 相关 | 用了 AC6 | 切回 ARM Compiler 5 |

---

## 6. 目录与工程文件的对应关系 · Project ↔ Source Mapping

| Keil 分组 Group | 目录 Folder | 内容 Contents |
| --- | --- | --- |
| `CMSIS` | `Libraries/CMSIS` | 启动文件、`system_stm32f4xx.c` |
| `StdPeriph` | `Libraries/STM32F4xx_StdPeriph_Driver` | ST 标准外设库（GPIO/USART/TIM/RCC…） |
| `App` | `App` | `main.c`、`modbus_rtu`、`sensor_env` |
| `BSP` | `BSP` | USART3/USART6、`usart6_var`、`delay` |
| `Driver` | `Driver` | BH1750、软件 I2C、PMS5003、LED |
| `Display` | `Display` | `lcd.c` 与字库 |
| `User` | `User` | `stm32f4xx_it.c`、`stm32f4xx_conf.h` |

新增源文件时，把它放进上表对应目录，并在 Keil 的对应分组里 `Add Existing Files`，同时在 `Include Paths` 中补上目录（路径均为相对 `Project/` 的 `..\Xxx`）。
