# 光通信OOK收发系统 — 使用手册

> 版本: v2.0 | 平台: STM32F407VET6 | 制率: 500bps OOK

---

## 一、项目简介

本项目是一个基于 STM32F407 的**可见光OOK（开关键控）通信收发系统**。通过激光二极管发送调制光信号，光电传感器接收并解调，实现点对点的低速光通信。

**核心功能：**
- OOK 调制光通信（500bps）
- 帧协议：前导码同步 + CRC16 校验
- CMIS 风格寄存器管理接口（兼容 SFF-8636 概念）
- DDM 数字诊断监控（温度、VCC、TX功率、RX功率）
- APC 自动功率控制（PI控制器）
- OLED 三页实时数据显示
- Python 上位机测试工具

---

## 二、硬件清单

| 序号 | 器件 | 型号/参数 | 数量 | 备注 |
|------|------|-----------|------|------|
| 1 | 主控板 | STM32F407VET6 最小系统板 | 1 | 或同芯片开发板 |
| 2 | 激光二极管 | 650nm 红光，5mW，工作电压3-5V | 1 | 需要驱动电路 |
| 3 | 光电传感器 | 光敏电阻(LDR) 或 光电二极管(PD) | 1 | 需要分压/放大电路 |
| 4 | OLED屏幕 | 0.96寸 SSD1306，I2C接口 | 1 | 128×64 像素 |
| 5 | ST-Link/V2 | STM32 调试/烧录器 | 1 | 或 J-Link |
| 6 | USB转串口 | CH340/CP2102 | 1 | 与上位机CMIS通信 |
| 7 | MOS管 | AO3400 (N-MOS) 或 S8050 (NPN) | 1 | 激光驱动开关 |
| 8 | 电阻 | 10kΩ, 100Ω, 1kΩ | 若干 | 分压/限流 |
| 9 | 面包板 + 杜邦线 | — | 若干 | 搭建电路 |

---

## 三、引脚接线总表

```
STM32F407VET6 引脚分配
┌─────────────────────────────────────────────┐
│                                             │
│  PA0  ──── ADC1_IN0  ──→ 光电传感器信号输入  │
│  PE9  ──── TIM1_CH1  ──→ 激光驱动(MOS管栅极) │
│  PA9  ──── USART1_TX ──→ USB转串口 RX        │
│  PA10 ──── USART1_RX ──→ USB转串口 TX        │
│  PB8  ──── OLED SCL  ──→ OLED SCL引脚        │
│  PB9  ──── OLED SDA  ──→ OLED SDA引脚        │
│  PA13 ──── SWDIO     ──→ ST-Link SWDIO       │
│  PA14 ──── SWCLK     ──→ ST-Link SWCLK       │
│  3.3V ──── VCC       ──→ OLED VCC            │
│  GND  ──── GND       ──→ 所有器件共地         │
│                                             │
└─────────────────────────────────────────────┘
```

### 详细接线

| STM32引脚 | 连接目标 | 线色建议 |
|-----------|----------|----------|
| PA0 | 光电传感器分压输出（中间节点） | 黄色 |
| PE9 | MOS管栅极（经100Ω电阻） | 橙色 |
| PA9 | USB转串口模块 RX | 绿色 |
| PA10 | USB转串口模块 TX | 蓝色 |
| PB8 | OLED SCL | 紫色 |
| PB9 | OLED SDA | 灰色 |
| 3.3V | OLED VCC, 光电传感器 VCC | 红色 |
| 5V | 激光二极管正极（经限流电阻） | 红色 |
| GND | 所有器件 GND（共地） | 黑色 |
| PA13 | ST-Link SWDIO | — |
| PA14 | ST-Link SWCLK | — |

---

## 四、电路搭建

### 4.1 激光驱动电路（PWM调制）

STM32的PE9输出3.3V PWM信号，通过N-MOS管驱动激光二极管：

```
        5V
         │
        ┌┴┐
        │ │ 限流电阻 47~100Ω（根据激光规格调整）
        └┬┘
         │
         ├──→ 激光二极管正极
         │
        ┌┴┐ 激光二极管
        └┬┘
         │
         D (漏极)
  PE9 ──[100Ω]── G (栅极)  AO3400 N-MOS
         │        S (源极)
         │         │
         ├─────────┴──→ GND
       10kΩ (下拉，确保无信号时关断)
         │
        GND
```

**要点：**
- MOS管的栅极串联 100Ω 电阻防止振荡
- 栅极对地接 10kΩ 下拉电阻，确保ST M32未初始化时激光关闭
- 激光二极管的限流电阻根据激光规格计算：`R = (5V - Vf_laser) / I_laser`
- 例如：红色激光 Vf≈2.0V，额定电流20mA → R = (5-2)/0.02 = 150Ω

### 4.2 光电传感器接收电路

使用光敏电阻（LDR）与固定电阻构成分压电路：

```
        3.3V
         │
        ┌┴┐
        │ │ 光敏电阻 (LDR)
        └┬┘          亮阻~1kΩ, 暗阻~100kΩ+
         │
         ├──→ PA0 (ADC采样点)
         │
        ┌┴┐
        │ │ 固定电阻 10kΩ
        └┬┘
         │
        GND
```

**要点：**
- 有光照时 LDR 阻值降低 → PA0 电压升高 → ADC读数大
- 无光照时 LDR 阻值增大 → PA0 电压降低 → ADC读数小
- 10kΩ 分压电阻在亮/暗时分别产生约 `3.3×10/(1+10)≈3.0V` 和 `3.3×10/(100+10)≈0.3V` 的电压差，足够RMS检测分辨

### 4.3 OLED连接

```
STM32        OLED (SSD1306)
PB8  ──────── SCL
PB9  ──────── SDA
3.3V ──────── VCC
GND  ──────── GND
```

使用软件模拟 I2C，不需要STM32硬件I2C外设。

### 4.4 USB转串口连接

```
STM32        USB转串口模块
PA9  ──────── RX  (交叉连接)
PA10 ──────── TX  (交叉连接)
GND  ──────── GND
```

**要点：TX接RX、RX接TX（交叉），必须共地。**

---

## 五、软件环境搭建

### 5.1 必要工具

| 工具 | 版本要求 | 用途 |
|------|----------|------|
| STM32CubeCLT | ≥1.21.0 | GCC工具链 + ST-Link驱动 |
| CMake | ≥3.22 | 构建系统 |
| Ninja | 任意版本 | 构建后端 |
| Python | ≥3.9 | 运行上位机测试脚本 |
| pyserial | `pip install pyserial` | Python串口库 |

### 5.2 安装pyserial

```bash
pip install pyserial
```

---

## 六、编译与烧录

### 6.1 编译

```bash
cd Optical_signal_acquisition_DSP

# 配置CMake（Debug版本）
cmake --preset Debug

# 编译
cmake --build build/Debug
```

编译成功后，ELF文件位于 `build/Debug/Optical_signal_acquisition_DSP.elf`。

**资源占用参考：**
```
RAM:    8.9 KB  / 128 KB  (6.8%)
Flash:  30.7 KB / 512 KB  (5.9%)
```

### 6.2 烧录

使用 STM32CubeProgrammer 或 ST-Link CLI：

```bash
# 方法1：STM32CubeProgrammer CLI
STM32_Programmer_CLI -c port=SWD -w build/Debug/Optical_signal_acquisition_DSP.elf -v

# 方法2：使用ST-Link GDB
arm-none-eabi-gdb -ex "target extended-remote :61234" \
                  -ex "load build/Debug/Optical_signal_acquisition_DSP.elf" \
                  -ex "monitor reset" \
                  -ex "quit"
```

烧录后按复位键或重新上电，OLED 应显示 "OOK XCVR v2.0"。

---

## 七、OLED 界面说明

系统启动后，OLED每500ms刷新一次，共3个页面。可通过修改变量 `sys_oled_page` 切换页面（未来可通过按键或CMIS命令控制）。

### 第1页：系统状态（Page 0）

```
┌─────────────────────────────┐
│ OOK XCVR v2.0              │  ← 项目名称
│ State: IDLE                │  ← 当前状态
│ TX:0 RX:0                  │  ← 发送/接收帧计数
│ CRC:0                      │  ← CRC错误计数
└─────────────────────────────┘
```

状态说明：
- **IDLE** — 空闲，等待命令
- **TX** — 正在发送数据帧
- **RX** — 正在监听接收
- **LOOP** — 自发自收测试模式
- **BERT** — 误码率测试模式

### 第2页：DDM监控（Page 1）

```
┌─────────────────────────────┐
│ DDM Monitor                 │
│ T: +25.0C V:  3.30V        │  ← 温度 / 电源电压
│ TX:  1.00mW                 │  ← 发射光功率(估算)
│ RX:  0.50mW                 │  ← 接收光功率(估算)
└─────────────────────────────┘
```

> 注意：温度和VCC当前使用标称值。如需真实读数，需启用ADC内部通道读取。

### 第3页：信号质量（Page 2）

```
┌─────────────────────────────┐
│ Signal Quality              │
│ RMS:  2.85V                 │  ← 接收信号RMS电平
│ Thr: SIGNAL                 │  ← 信号/噪声指示
│ Bits/s: 500                 │  ← 比特率
└─────────────────────────────┘
```

---

## 八、CMIS 寄存器协议

### 8.1 协议格式

通过 USART1（115200, 8N1）以文本命令读写寄存器。

**读寄存器：**
```
发送: R<page_hex><addr_hex>\r\n
响应: +<data_hex>\r\n

示例: R0080\r\n       → 读取温度MSB
响应: +19\r\n         → 返回 0x19
```

**写寄存器：**
```
发送: W<page_hex><addr_hex><data_hex>\r\n
响应: +\r\n

示例: W019001\r\n     → 使能TX+RX
响应: +\r\n
```

**错误响应：**
```
-01  命令格式错误或地址无效
```

### 8.2 寄存器映射

#### Page 0x00 — 状态与监控（只读）

| 地址 | 名称 | 格式 | 说明 |
|------|------|------|------|
| 0x00 | 模块ID | u8 | 固定值 0x01（OOK收发器） |
| 0x03-0x06 | 部件号 | ASCII | 'O' 'T' 'R' 'X' |
| 0x80-0x81 | 温度 | s16, 1/256°C | 当前 0x1900 = 25°C |
| 0x82-0x83 | VCC | u16, 100μV | 当前 33000 ≈ 3.30V |
| 0x84-0x85 | TX偏置电流 | u16, 2μA | 预留 |
| 0x86-0x87 | TX发射功率 | u16, 0.1μW | 由占空比估算 |
| 0x88-0x89 | RX接收功率 | u16, 0.1μW | 由RMS信号电平估算 |
| 0x8E | 状态标志 | u8 | bit3=FRAME_RCVD |

**状态寄存器 (0x8E) 位定义：**

| 位 | 名称 | 说明 |
|----|------|------|
| bit0 | TX_ACTIVE | 正在发送 |
| bit1 | RX_ACTIVE | 正在接收 |
| bit2 | APC_ENABLED | APC已使能 |
| bit3 | LINK_UP | 链路已建立 |
| bit4 | FRAME_RCVD | 收到有效帧 |
| bit7 | ERROR | 错误状态 |

#### Page 0x01 — 配置与控制（读写）

| 地址 | 名称 | 默认值 | 说明 |
|------|------|--------|------|
| 0x80 | APC使能 | 0x00 | 0=关闭, 1=开启 |
| 0x81 | APC目标功率 | 0x80 | 0.1dBm单位 |
| 0x90 | 激光使能 | 0x00 | bit0=TX, bit1=RX |
| 0x91-0x92 | 比特率设置 | 0x01F4 | 500bps, big-endian |
| 0x93 | 检测阈值 | — | RX检测阈值 |
| 0xA0 | TX载荷长度 | 0x00 | 0-255字节 |
| 0xA1 | TX触发 | 0x00 | 写0x01触发发送 |
| 0xA2-... | TX载荷数据 | — | 从0xA2开始存放载荷 |

---

## 九、Python 上位机使用

### 9.1 基本用法

```bash
cd Python

# 读取模块信息
python optical_transceiver_test.py COM3

# DDM持续监控（30秒）
python optical_transceiver_test.py COM3 --monitor

# Loopback自发自收测试（需将激光对准光电传感器）
python optical_transceiver_test.py COM3 --loopback

# 发送自定义消息
python optical_transceiver_test.py COM3 --send "Hello World"

# 发送并启用接收（用于loopback验证）
python optical_transceiver_test.py COM3 --send "Test" --rx
```

### 9.2 预期输出示例

**读取模块信息：**
```
Module ID: 0x01
Temperature: 25.0 C
VCC: 3.300 V
```

**DDM监控：**
```
  Time  Temp(C)  VCC(V)  TX(mW)   RX(mW)   Status
   0.0    25.00   3.300  1.0000   0.5000    0x00
   1.0    25.00   3.300  1.0000   0.5000    0x00
   2.0    25.00   3.300  1.0000   0.5000    0x10
```

**Loopback测试：**
```
Enabling RX...
Enabling TX laser...
Sending: Hello Optical World!
Waiting for loopback frame...
Received: Hello Optical World!
Match: True
```

### 9.3 用串口助手手动测试

你也可以用任意串口工具（Putty、SSCOM、MobaXterm等）手动发送CMIS命令：

```
配置: 115200, 8N1, 无流控, 末尾加\r\n

R0000       → 读模块ID（应返回 +01）
R0080       → 读温度MSB
R0081       → 读温度LSB
W019005     → 使能TX+RX
W01A005     → 设置载荷长度=5
W01A248     → 载荷[0]='H' (0x48)
W01A265     → 载荷[1]='e' (0x65)
W01A26C     → 载荷[2]='l' (0x6C)
W01A36C     → 载荷[3]='l' (0x6C)
W01A46F     → 载荷[4]='o' (0x6F)
W01A101     → 触发发送！
R008E       → 轮询状态，bit4=1表示收到帧
```

---

## 十、快速测试步骤

### Step 1：硬件检查
- [ ] 所有器件按接线表正确连接
- [ ] 万用表测3.3V和5V供电正常
- [ ] ST-Link能识别芯片（STM32CubeProgrammer 显示 Device connected）

### Step 2：编译烧录
```bash
cmake --preset Debug
cmake --build build/Debug
# 烧录 ELF 文件
```
- [ ] OLED 显示 "OOK XCVR v2.0"

### Step 3：串口通信验证
```bash
python optical_transceiver_test.py COM3
```
- [ ] 返回 Module ID: 0x01

### Step 4：DDM监控测试
```bash
python optical_transceiver_test.py COM3 --monitor
```
- [ ] 每1秒输出一行温度/VCC/功率数据

### Step 5：Loopback通信测试
1. 将激光二极管对准光电传感器（距离1-5cm）
2. 运行：
```bash
python optical_transceiver_test.py COM3 --loopback
```
- [ ] 输出 `Match: True`
- [ ] OLED 显示 TX:1 RX:1

#### 6. 打开激光
python optical_transceiver_test.py COM3 --laser on

### 7. 关闭激光
python optical_transceiver_test.py COM3 --laser off
---

## 十一、故障排查

| 现象 | 可能原因 | 解决方法 |
|------|----------|----------|
| OLED无显示 | PB8/PB9接线错误或接触不良 | 检查接线；用逻辑分析仪看I2C信号 |
| 串口无响应 | PA9/PA10接反或波特率不匹配 | TX接RX、RX接TX（交叉）；确认115200bps |
| Loopback收不到帧 | 激光未对准传感器、分压电阻不合适 | 调暗环境光；用示波器看PA0是否有波形变化 |
| 激光不亮 | MOS管驱动电路问题 | 用示波器测量PE9：bit '1'应有10kHz PWM；测MOS漏极电压 |
| LED呼吸无反应 | 电路问题或PWM输出异常 | 用示波器测量PE9引脚确认有PWM波形 |
| 编译失败 "arm_math.h not found" | DSP库路径未配置 | 检查 `DSP/Include` 目录存在 |
| 烧录失败 "No target connected" | ST-Link驱动或接线问题 | 检查SWDIO/SWCLK/GND三线；重装ST-Link驱动 |
| CRC错误持续增加 | 光路干扰大、阈值不合适 | 调整 `LaserRx_SetThreshold` 参数（当前0.3V） |
| ST-Link错误138 | Windows驱动冲突 | 换USB口、重装驱动或重启电脑 |

---

## 十二、进阶开发

### 12.1 调整比特率

修改 `system_manager.c` 中 `SysMgr_Init()` 的参数：

```c
LaserTx_SetBitPeriod(2000);    // 改为 1000 = 1000bps
LaserRx_Init(50000, 500);      // 第二个参数改为 1000
```

### 12.2 调整RX检测阈值

```c
LaserRx_SetThreshold(0.3f);    // 根据实际信号电平调整
                                // 高了会漏检，低了会误检
```

### 12.3 启用真实温度/VCC读取

编辑 `ddm_monitor.c` 的 `DDM_Update()`，启用内部ADC通道IN16（温度传感器）和IN17（Vrefint）的读数（需暂停ADC DMA、切换通道、读取、恢复DMA）。

### 12.4 启用APC闭环控制

编辑 `system_manager.c` 的 APC tick 部分，将占位代码替换为实际的PI控制器调用：

```c
static APC_Controller apc;
// 在 SysMgr_Init 中: APC_Init(&apc, 0.5f, 0.1f, target);
// 在 APC tick 中: float duty = APC_Update(&apc, measured, 0.1f);
//                LaserTx_SetDutyCycle((uint8_t)duty);
```

### 12.5 如何实现通过激光器发送”Hello World”，又是如何在接收端解析数据的？

以回环模式（LOOPBACK，激光对准自家接收端）为例，完整追踪一次”Hello World”的发送与接收过程。

#### 12.5.1 整体数据流

```
上位机串口 → CMIS命令写寄存器 → System Manager检测触发
    → Frame_Encode 组帧 → LaserTx OOK调制 → 激光闪烁
    → 光电传感器接收 → ADC 50kHz采样 → DMA双缓冲传输
    → LaserRx RMS窗口判决 → FrameRx 5状态机解帧 → CRC校验
    → 载荷写回CMIS寄存器 → 上位机串口读取结果
```

**关键参数：**
- 比特率：100bps（10ms/bit），在 `SysMgr_Init()` 中设置
- ADC 采样率：50kHz（20μs/点），由 TIM2 触发
- 每比特采样点数：50000 / 100 = **500 点/bit**
- PWM 载波频率：84MHz / (167+1) / (99+1) = 5kHz，占空比 50%

#### 12.5.2 发送端 — 第一步：通过 CMIS 加载数据

用户在串口终端（115200, 8N1）依次发送以下命令：

```
W01A00B          ① 写 Page 0x01, Reg 0xA0 = 0x0B （载荷长度 = 11 字节）
W01A248          ② 写 Page 0x01, Reg 0xA2 = 'H' = 0x48
W01A365          ③ 写             Reg 0xA3 = 'e' = 0x65
W01A46C          ④ 写             Reg 0xA4 = 'l' = 0x6C
W01A56C          ⑤ 写             Reg 0xA5 = 'l' = 0x6C
W01A66F          ⑥ 写             Reg 0xA6 = 'o' = 0x6F
W01A720          ⑦ 写             Reg 0xA7 = ' ' = 0x20
W01A857          ⑧ 写             Reg 0xA8 = 'W' = 0x57
W01A96F          ⑨ 写             Reg 0xA9 = 'o' = 0x6F
W01AA72          ⑩ 写             Reg 0xAA = 'r' = 0x72
W01AB6C          ⑪ 写             Reg 0xAB = 'l' = 0x6C
W01AC64          ⑫ 写             Reg 0xAC = 'd' = 0x64
W01A101          ⑬ 写 Page 0x01, Reg 0xA1 = 0x01（触发发送！）
```

每条命令的处理路径：UART RX 中断 → `CMIS_OnRxChar()` 逐字符收集至换行符 → `sscanf` 解析 hex → `CMIS_RegWrite()` 写入 512 字节的 `reg_map` 数组。

#### 12.5.3 发送端 — 第二步：System Manager 检测触发

`SysMgr_Run()` 每轮主循环检查 `CMIS_RegRead(0x01, 0xA1)`。当读到 `0x01` 时，执行发送流程：

```c
// system_manager.c SysMgr_Run() 中的 TX 触发逻辑
if (CMIS_RegRead(CMIS_PAGE_CONFIG, 0xA1) == 0x01) {
    CMIS_RegWrite(CMIS_PAGE_CONFIG, 0xA1, 0x00);  // 清除触发标志

    uint16_t plen = CMIS_RegRead(CMIS_PAGE_CONFIG, 0xA0);  // plen = 11
    uint8_t payload[FRAME_MAX_PAYLOAD];
    for (uint16_t i = 0; i < plen; i++) {
        payload[i] = CMIS_RegRead(CMIS_PAGE_CONFIG, 0xA2 + i);
    }
    // payload = “Hello World”

    uint16_t total = Frame_Encode(payload, plen, tx_frame_buf, sizeof(tx_frame_buf));
    LaserTx_SendBuffer(tx_frame_buf, total);  // 阻塞发送整帧
}
```

#### 12.5.4 发送端 — 第三步：Frame_Encode 组帧

`Frame_Encode()` 将 11 字节载荷封装为带同步头和校验的完整帧（共 **21 字节 = 168 bits**）：

```
字节偏移  内容                    说明
────────────────────────────────────────────────────
 0~3    0xAA 0xAA 0xAA 0xAA    前导码（4字节，共32个交替bit）
                                 用于接收端从比特流中定位帧边界
 4~5    0x7E 0x5A              同步字（标志”帧头在这里”）
 6~7    0x00 0x0B              载荷长度 = 11（大端序）
 8~18   48 65 6C 6C 6F 20      载荷：”Hello World”
        57 6F 72 6C 64
19~20   CRC_H CRC_L            CRC-16/CCITT（多项式 x^16+x^12+x^5+1）
```

帧结构示意图：

```
┌──────┬──────┬──────┬──────┬──────────┬──────────┬────────┬──────┐
│0xAA  │0xAA  │0xAA  │0xAA  │0x7E 0x5A │Len=0x000B│”Hello  │CRC16 │
│      │      │      │      │          │          │ World” │      │
│4字节 │      │      │      │2字节同步字│2字节长度  │11字节   │2字节 │
│前导码│      │      │      │          │          │载荷     │校验  │
└──────┴──────┴──────┴──────┴──────────┴──────────┴────────┴──────┘
```

#### 12.5.5 发送端 — 第四步：OOK 调制（激光闪烁）

`LaserTx_SendBuffer()` 逐字节调用 `LaserTx_SendByte()`，后者逐位（MSB 优先）调用 `LaserTx_SendBit()`：

```c
// laser_tx.c
void LaserTx_SendBit(uint8_t bit) {
    // bit=1 → TIM1 CCR = 50 → PWM 5kHz 方波 → 激光点亮
    // bit=0 → TIM1 CCR = 0  → 输出恒低电平     → 激光熄灭
    uint32_t ccr = (bit && duty_percent > 0) ? duty_percent : 0;
    __HAL_TIM_SET_COMPARE(tx_htim, tx_channel, ccr);

    // 保持状态 10000us（10ms，对应 100bps）
    DWT_DelayUS(bit_period_us);  // bit_period_us = 10000
}
```

以字母 **'H' = 0x48 = 0b01001000** 为例，它的 8 个 bit 被依次发送（MSB 优先）：

```
bit7=0     bit6=1     bit5=0     bit4=0     bit3=1     bit2=0     bit1=0     bit0=0
 激光灭     激光亮      激光灭     激光灭      激光亮     激光灭     激光灭     激光灭
  10ms      10ms       10ms       10ms       10ms       10ms       10ms       10ms

时间轴:
0ms    10ms    20ms    30ms    40ms    50ms    60ms    70ms    80ms
───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────
 OFF   │  ON   │ OFF   │ OFF   │  ON   │ OFF   │ OFF   │ OFF
(bit7) │(bit6) │(bit5) │(bit4) │(bit3) │(bit2) │(bit1) │(bit0)
───────┴───────┴───────┴───────┴───────┴───────┴───────┴───────
```

**OOK（On-Off Keying）调制**本质：用激光的”亮/灭”表示数字信号的”1/0”，是最简单的数字调制方式。整帧 168 bits × 10ms = **1.68 秒**发完。

#### 12.5.6 接收端 — 第五步：ADC 采样

TIM2 以 50kHz 频率触发 ADC（`84MHz / (83+1) / (19+1) = 50kHz`），每 20μs 采集一次光电传感器输出电压。DMA 将采样值循环写入 1000 点的双缓冲区。

```
ADC 采样参数:
  采样率: 50kHz（每20μs一个点）
  分辨率: 12bit（0~4095）
  参考电压: 3.3V
  缓冲区: adc_buffer[1000]，前500点和后500点交替使用（DMA半传输/完成中断）

每比特采集: 50000Hz / 100bps = 500 个 ADC 采样点
```

DMA 半传输完成 → 处理 `adc_buffer[0..499]` → DMA 完成传输 → 处理 `adc_buffer[500..999]` → 循环往复，实现**无间断**数据流。

#### 12.5.7 接收端 — 第六步：RMS 窗口判决

`SysMgr_OnADC_DMAComplete()` 以 `samples_per_bit = 500` 为滑动窗口，每 500 个采样点调用一次 `LaserRx_ProcessWindow()`：

```c
// laser_rx.c — LaserRx_ProcessWindow 核心逻辑
// ① ADC值 → 电压值: V = ADC × 3.3 / 4095
for (uint16_t i = 0; i < count; i++) {
    rx_working_buf[i] = (float32_t)samples[i] * 3.3f / 4095.0f;
}

// ② 计算500个采样点的RMS有效值（调用CMSIS-DSP库）
float32_t rms;
arm_rms_f32(rx_working_buf, count, &rms);  // 硬件加速浮点运算

// ③ 门限判决
//    有光 → 光电管输出电压低 → RMS < 2.0V → 判为 bit=1
//    无光 → 光电管输出电压高 → RMS ≥ 2.0V → 判为 bit=0
*bit_out = (rms < threshold) ? 1 : 0;
```

**为什么用 `<` 而不是 `>`？** 这是由光敏电阻的分压电路特性决定的：

```
有光照 → LDR阻值降低(~1kΩ) → PA0电压 ≈ 3.3×10/(1+10) ≈ 3.0V (高位)
无光照 → LDR阻值升高(~100kΩ) → PA0电压 ≈ 3.3×10/(100+10) ≈ 0.3V (低位)

但在本电路中，激光亮时LDR阻值低 → PA0电压高 → ADC读数大
激光灭时LDR阻值高 → PA0电压低 → ADC读数小
```

> **注意：** 如果使用的是光电二极管（PD）而非光敏电阻，极性可能相反，需根据实际电路调整 `LaserRx_SetThreshold()` 的门限值和 `<` / `>` 方向。

#### 12.5.8 接收端 — 第七步：5 状态帧解调机

`FrameRx_FeedBit()` 将逐个到来的 bit 喂入一个**移位寄存器 + 5 状态机**：

```c
// frame_protocol.c — 移位寄存器逐位收集
rx_shift_reg = (rx_shift_reg << 1) | (bit & 1);
rx_bit_count++;
// 每收集满 8 个 bit，取出一个完整字节
if (rx_bit_count >= 8) {
    uint8_t byte = (rx_shift_reg >> (rx_bit_count - 8)) & 0xFF;
    // 交给状态机处理...
}
```

**状态机 5 个状态的转换逻辑：**

```
                     ┌─────────────────────────────────┐
                     │                                 │
          ┌──────────┴──────────┐                      │
          │  SEEK_PREAMBLE      │ ← 同步字不匹配时回退  │
          │  寻找 0xAA         │                      │
          └──────────┬──────────┘                      │
                     │ 发现 0xAA                        │
          ┌──────────┴──────────┐                      │
          │  SEEK_SYNC          │                      │
          │  等待 0x7E 0x5A     │──── 不匹配 ──────────┘
          └──────────┬──────────┘
                     │ 确认前导码+同步字
          ┌──────────┴──────────┐
          │  READ_LENGTH        │
          │  读取2字节载荷长度   │──── 长度非法 → BAD_LENGTH
          └──────────┬──────────┘
                     │ 长度合法
          ┌──────────┴──────────┐
          │  READ_PAYLOAD       │  逐字节收集载荷
          └──────────┬──────────┘
                     │ 收满 plen 字节
          ┌──────────┴──────────┐
          │  READ_CRC           │  读取2字节CRC校验值
          └──────────┬──────────┘
                     │
          ┌──────────┴──────────┐
          │  COMPLETE           │  校验 CRC-16/CCITT
          └─────────────────────┘
               │            │
          CRC匹配         CRC不匹配
          FRAME_RESULT_OK  FRAME_RESULT_CRC_ERROR
```

#### 12.5.9 接收端 — 第八步：CRC 校验

状态机到达 `COMPLETE` 后，立即进行 CRC-16/CCITT 校验：

```c
// frame_protocol.c — CRC 校验逻辑
uint16_t crc_expected = (rx_byte_buf[payload_len] << 8)   // 帧中携带的 CRC
                      | rx_byte_buf[payload_len + 1];
uint16_t crc_calc = CRC16_CCITT(rx_byte_buf, payload_len); // 重新计算 CRC

if (crc_expected == crc_calc) {
    return FRAME_RESULT_OK;    // 校验通过，载荷 = “Hello World”
} else {
    return FRAME_RESULT_CRC_ERROR;  // 传输中发生了比特错误
}
```

CRC-16/CCITT 使用查表法实现（256 项的预计算表 `crc_table`），输入是载荷数据，输出 2 字节校验码。任何单比特或多比特错误都有极高概率被检测出来。

#### 12.5.10 接收端 — 第九步：结果回传 CMIS

校验通过后，`SysMgr_Run()` 将解码结果回填到 CMIS 寄存器：

```c
// system_manager.c — 收到完整帧时
FrameRx_GetResult(rx_payload_buf, &rlen);          // 取出 “Hello World”, rlen=11
CMIS_RegWrite(CMIS_PAGE_CONFIG, 0xA0, (uint8_t)rlen);  // Reg 0xA0 = 0x0B
for (uint16_t i = 0; i < rlen; i++) {
    CMIS_RegWrite(CMIS_PAGE_CONFIG, 0xA2 + i, rx_payload_buf[i]);
}
// 设置 FRAME_RCVD 标志 (bit4)
uint8_t status = CMIS_RegRead(CMIS_PAGE_STATUS, 0x8E);
status |= 0x10;
CMIS_RegWrite(CMIS_PAGE_STATUS, 0x8E, status);
```

此时上位机查询：

```
R01A0  → +0B           （载荷长度=11）
R01A2  → +48  = 'H'
R01A3  → +65  = 'e'
R01A4  → +6C  = 'l'
...                    （逐字节读出 “Hello World”）
```

#### 12.5.11 完整时间线

```
T=0.0s   上位机发送 W01A00B ~ W01AC64（12条写命令）
T=0.1s   上位机发送 W01A101（触发发送）
T=0.1s   SysMgr_Run 检测到触发
          Frame_Encode 组帧（21字节）
T=0.1s   激光开始闪烁，MSB先发

          前导码 (32 bits)  ─── 0.32s ───
          同步字 (16 bits)  ─── 0.16s ───
          长度   (16 bits)  ─── 0.16s ───
          载荷   (88 bits)  ─── 0.88s ───  ← “Hello World” 在这里
          CRC    (16 bits)  ─── 0.16s ───

T=1.78s  激光发送完毕，LaserTx_CarrierOff

          同时（回环模式），接收端 ADC 持续采样
          每 500 个采样点（= 10ms）判决一个 bit
          判决结果逐位送入 FrameRx 状态机

T≈1.8s   CRC 校验通过
          FrameRx_GetResult 取出载荷
          载荷回写 CMIS 寄存器
          rx_frame_ready 置 1，frames_rx 计数 +1

T=1.9s   上位机轮询 R008E 检测到 bit4=1
          上位机 R01A0 读到长度 0x0B
          上位机 R01A2~R01AC 逐字节读出 “Hello World”
          → 通信成功！
```

#### 12.5.12 Python 一键发送

以上手动步骤可用 Python 脚本一行完成：

```bash
python optical_transceiver_test.py COM3 --send “Hello World” --rx
```

脚本内部自动执行了：设置长度 → 逐字节写载荷 → 触发发送 → 等待接收 → 读取并打印结果。

---

## 附录：引脚速查卡

```
         STM32F407VET6 (LQFP100)
        ┌─────────────────────────┐
        │                         │
  PA0 ──┤ 23  ADC1_IN0  光电输入  │
  PA9 ──┤ 42  USART1_TX  串口TX   │
 PA10 ──┤ 43  USART1_RX  串口RX   │
 PA13 ──┤ 46  SWDIO      ST-Link  │
 PA14 ──┤ 49  SWCLK      ST-Link  │
  PB8 ──┤ 95  OLED_SCL   软件I2C  │
  PB9 ──┤ 96  OLED_SDA   软件I2C  │
  PE9 ──┤ 4   TIM1_CH1   激光PWM  │
        │                         │
        │  3.3V ── OLED/传感器VCC │
        │  GND  ── 所有器件共地   │
        └─────────────────────────┘
```
