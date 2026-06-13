/**
  * @brief  系统管理器: 主状态机、任务调度、OLED显示
  *         负责初始化所有子系统, 在主循环中轮询 CMIS 命令、TX触发、RX解帧、
  *         DDM更新、APC更新、OLED刷新
  *
  *         系统状态: INIT → IDLE → TX/RX_LISTENING/LOOPBACK/BER_TEST
  */
#include "system_manager.h"
#include "laser_tx.h"
#include "laser_rx.h"
#include "frame_protocol.h"
#include "cmis_protocol.h"
#include "OLED.h"
#include <stdio.h>
#include <string.h>

static TIM_HandleTypeDef  *sys_htim1;
static UART_HandleTypeDef *sys_huart1;

static SystemState sys_state = SYS_STATE_INIT;

// TX 相关
static uint8_t  tx_frame_buf[FRAME_MAX_TOTAL];// 编码后完整帧数据 (载荷+CRC+封装)
static uint16_t tx_frame_len;// 编码后完整帧长度
static uint8_t  tx_active;           // 1=正在发送

// RX 相关
static uint8_t  rx_payload_buf[FRAME_MAX_PAYLOAD];// 解码后载荷数据
static uint16_t rx_payload_len;// 解码后载荷长度
static uint8_t  rx_frame_ready;      // 1=已解析到完整帧, 待主循环处理

// 统计计数
static uint16_t frames_tx;// 发送帧计数
static uint16_t frames_rx;// 接收帧计数
static uint16_t crc_errors;// CRC错误计数

// 各任务的节拍计数 (用于非阻塞定时)
static uint32_t last_ddm_tick;// DDM更新节拍
static uint32_t last_apc_tick;// APC更新节拍
static uint32_t last_oled_tick;// OLED刷新节拍

static uint32_t samples_per_bit = 100;  // 50000Hz / 100bps = 500 samples/bit

// 当前 OLED 显示页面: 0=主页面, 1=DDM监控, 2=信号质量
volatile uint8_t sys_oled_page = 2;

// ===== OLED 三页显示 =====
// 第0页: 系统主页面, 显示版本号、状态、收发帧计数、CRC错误数
static void OLED_ShowPage0(void) {
    OLED_Clear();
    OLED_ShowString(0, 0,  "OOK XCVR v2.0", OLED_8X16);
    char line2[22];
    const char *state_str = "INIT";
    switch (sys_state) {
        case SYS_STATE_IDLE:         state_str = "IDLE"; break;
        case SYS_STATE_TX:           state_str = "TX  "; break;
        case SYS_STATE_RX_LISTENING: state_str = "RX  "; break;
        case SYS_STATE_LOOPBACK:     state_str = "LOOP"; break;
        case SYS_STATE_BER_TEST:     state_str = "BERT"; break;
        default: break;
    }
    snprintf(line2, sizeof(line2), "State: %s", state_str);
    OLED_ShowString(0, 16, line2, OLED_8X16);
    char line3[22];
    snprintf(line3, sizeof(line3), "TX:%u RX:%u", frames_tx, frames_rx);
    OLED_ShowString(0, 32, line3, OLED_8X16);
    char line4[22];
    snprintf(line4, sizeof(line4), "CRC:%u", crc_errors);
    OLED_ShowString(0, 48, line4, OLED_8X16);
    OLED_Update();
}

// 第1页: DDM 诊断页面, 从CMIS寄存器读取温度/电压/TX功率/RX功率并显示
static void OLED_ShowPage1(void) {
    OLED_Clear();
    OLED_ShowString(0, 0,  "DDM Monitor", OLED_8X16);
    int16_t t_raw = (int16_t)(((uint16_t)CMIS_RegRead(0x00, 0x80) << 8)
                              | CMIS_RegRead(0x00, 0x81));
    float temp = t_raw / 256.0f;
    uint16_t v_raw = ((uint16_t)CMIS_RegRead(0x00, 0x82) << 8)
                    | CMIS_RegRead(0x00, 0x83);
    float vcc = v_raw * 100e-6f;
    uint16_t tp_raw = ((uint16_t)CMIS_RegRead(0x00, 0x86) << 8)
                     | CMIS_RegRead(0x00, 0x87);
    float tx_pwr = tp_raw * 0.1e-3f;
    uint16_t rp_raw = ((uint16_t)CMIS_RegRead(0x00, 0x88) << 8)
                     | CMIS_RegRead(0x00, 0x89);
    float rx_pwr = rp_raw * 0.1e-3f;
    char line[22];
    snprintf(line, sizeof(line), "T:%5.1fC V:%4.2fV", temp, vcc);
    OLED_ShowString(0, 16, line, OLED_8X16);
    snprintf(line, sizeof(line), "TX:%5.2fmW", tx_pwr);
    OLED_ShowString(0, 32, line, OLED_8X16);
    snprintf(line, sizeof(line), "RX:%5.2fmW", rx_pwr);
    OLED_ShowString(0, 48, line, OLED_8X16);
    OLED_Update();
}

// 第2页: 信号质量页面, 显示RMS电压、信号/噪声判定、比特率
static void OLED_ShowPage2(void) {
    OLED_Clear();
    OLED_ShowString(0, 0,  "Signal Quality", OLED_8X16);

    float rms = LaserRx_GetSignalLevel();

    // 将 float 拆成整数+小数部分分别打印, 避免 MCU 浮点 printf 陷阱
    int rms_int = (int)rms;
    int rms_dec = (int)((rms - rms_int) * 100);
    char line[22];
    snprintf(line, sizeof(line), "RMS: %d.%02dV", rms_int, rms_dec);// 显示 RMS 电压
    
    OLED_ShowString(0, 16, line, OLED_8X16);
    snprintf(line, sizeof(line), "Thr: %s", rms < 2.0f ? "SIGNAL" : "NOISE ");// 显示信号/噪声判定
    OLED_ShowString(0, 32, line, OLED_8X16);
    snprintf(line, sizeof(line), "Bits/s: %lu", (unsigned long)(50000 / samples_per_bit));// 显示比特率
    OLED_ShowString(0, 48, line, OLED_8X16);
    OLED_Update();
}

/* ---- Public API ---- */
// 系统初始化: 配置激光收发、CMIS协议、帧解码器、OLED
void SysMgr_Init(TIM_HandleTypeDef *htim1, ADC_HandleTypeDef *hadc1,
                 UART_HandleTypeDef *huart1) {
    sys_htim1 = htim1;
    sys_huart1 = huart1;
    (void)hadc1;  // ADC 由 DMA 中断驱动，此处仅保存引用

    // 初始化激光发射: TIM1_CH1 PWM, 100bps (10ms/bit)
    LaserTx_Init(sys_htim1, TIM_CHANNEL_1);
    LaserTx_SetBitPeriod(10000);   // 10ms/bit = 100bps (LDR 响应时间 ~10-20ms)
    LaserTx_SetDutyCycle(50);
    HAL_TIM_PWM_Start(sys_htim1, TIM_CHANNEL_1);
    LaserTx_CarrierOff();          // 初始关闭激光

    // 初始化激光接收: 50kHz 采样率, 100bps 比特率
    LaserRx_Init(50000, 100);
    LaserRx_SetThreshold(2.0f);    // RMS 判决门限 2.0V, 需根据实际光路调整
    samples_per_bit = 50000 / 100;

    CMIS_Init(sys_huart1);
    FrameRx_Reset();

    tx_active = 0;
    rx_frame_ready = 0;
    frames_tx = 0;
    frames_rx = 0;
    crc_errors = 0;

    OLED_Init();
    OLED_Clear();

    last_ddm_tick = HAL_GetTick();
    last_apc_tick = HAL_GetTick();
    last_oled_tick = HAL_GetTick();

    // CMIS 配置: RX使能 (bit1=1), 进入回环模式
    CMIS_RegWrite(0x01, 0x90, 0x02);// page1 reg0x90: bit0=TX使能, bit1=RX使能,
    //addr0x90 bit1=1启用RX,  data=0x02
    sys_state = SYS_STATE_LOOPBACK;
}

// 主循环: 在 while(1) 中每轮调用
void SysMgr_Run(void) {
    CMIS_Process();   // CMIS 维护 (实际由中断驱动)

    // ---- CMIS 配置变更处理 ----
    // Page 0x01 Reg 0x90: bit0=TX使能, bit1=RX使能
    uint8_t laser_cfg = CMIS_RegRead(CMIS_PAGE_CONFIG, 0x90);//
    uint8_t tx_enable = laser_cfg & 0x01;
    uint8_t rx_enable = (laser_cfg >> 1) & 0x01;
    static uint8_t prev_tx_enable = 0;// 上一轮的 TX 使能状态, 用于边沿检测

    // TX 使能/关闭: 仅在边沿变化时操作
    if (tx_enable && !prev_tx_enable) {
        LaserTx_CarrierOn();
    } else if (!tx_enable && prev_tx_enable) {
        LaserTx_CarrierOff();
    }
    prev_tx_enable = tx_enable;

    // RX 使能/关闭: 切换到监听状态
    if (rx_enable && sys_state == SYS_STATE_IDLE) {
        sys_state = SYS_STATE_RX_LISTENING;
        FrameRx_Reset();
    }
    if (!rx_enable && sys_state == SYS_STATE_RX_LISTENING) {
        sys_state = SYS_STATE_IDLE;
    }

    // ---- TX 发送触发 ----
    // Page 0x01 Reg 0xA1: 写 1 触发一次发送, Reg 0xA0 = 载荷长度, Reg 0xA2+ = 载荷数据
    if (CMIS_RegRead(CMIS_PAGE_CONFIG, 0xA1) == 0x01) {// 触发发送
        CMIS_RegWrite(CMIS_PAGE_CONFIG, 0xA1, 0x00); // 清除触发位
        uint16_t plen = CMIS_RegRead(CMIS_PAGE_CONFIG, 0xA0);// 载荷长度
        if (plen > 0 && plen <= FRAME_MAX_PAYLOAD) {// 载荷长度合法
            uint8_t payload[FRAME_MAX_PAYLOAD];// 从 CMIS 寄存器读取载荷数据
            for (uint16_t i = 0; i < plen; i++) {// 从 CMIS 寄存器读取载荷数据
                payload[i] = CMIS_RegRead(CMIS_PAGE_CONFIG, (uint8_t)(0xA2 + i));
            }
            // 编码成完整帧 (添加 CRC 和封装)
            uint16_t total = Frame_Encode(payload, plen, tx_frame_buf, sizeof(tx_frame_buf));
            
            if (total > 0) {// 编码成功且有数据需要发送
                tx_frame_len = total;// 记录编码后帧长度
                tx_active = 1;// 标记正在发送, 由 DMA 回调中实际触发发送
                SystemState prev = sys_state;// 记录当前状态
                sys_state = SYS_STATE_TX;// 切换到 TX 状态, 在 DMA 回调中发送完成后切回
                LaserTx_SendBuffer(tx_frame_buf, tx_frame_len);// 触发发送
                tx_active = 0;// 发送完成, 立即切回之前的状态 (通常是监听状态)
                frames_tx++;// 发送计数加一
                LaserTx_CarrierOff();// 发送完成后立即关闭激光, 避免空闲时的误触发
                sys_state = prev;// 切回之前的状态, 继续监听或保持空闲
            }
        }
    }

    // ---- RX 帧处理 ----
    // 由 DMA 中断中 FrameRx_FeedBit 触发, 在此处取出完整帧并回写 CMIS
    if (rx_frame_ready) {
        rx_frame_ready = 0;
        uint16_t rlen;
        FrameResult res = FrameRx_GetResult(rx_payload_buf, &rlen);// 从帧解码器获取载荷数据和长度
        if (res == FRAME_RESULT_OK) {     // 成功解析到完整帧
            rx_payload_len = rlen;// 记录载荷长度
            frames_rx++;// 接收计数加一
            CMIS_RegWrite(CMIS_PAGE_CONFIG, 0xA0, (uint8_t)rlen);// 将载荷长度写回 CMIS 寄存器
            for (uint16_t i = 0; i < rlen; i++) {
                CMIS_RegWrite(CMIS_PAGE_CONFIG, (uint8_t)(0xA2 + i), rx_payload_buf[i]);
            }
            uint8_t status = CMIS_RegRead(CMIS_PAGE_STATUS, 0x8E);// 读取当前状态寄存器
            status |= 0x10;// 设置 bit4=1 表示有新帧到达, 需要主机处理
            CMIS_RegWrite(CMIS_PAGE_STATUS, 0x8E, status);// 写回状态寄存器, 触发主机中断
        } else if (res == FRAME_RESULT_CRC_ERROR) {// CRC 错误
            crc_errors++;
        }
    }

    uint32_t now = HAL_GetTick();// 获取当前系统时间 (ms)

    // ---- DDM 定时更新 (500ms) ----
    // 将温度/电压/TX功率/RX功率写入 CMIS 寄存器映射表中
    if (now - last_ddm_tick >= 500) {// 每 500ms 更新一次 DDM 数据
        last_ddm_tick = now;// 更新 DDM 数据: 温度、电压、TX功率、RX功率
        int16_t temp_raw = (int16_t)(25.0f * 256.0f);
        CMIS_RegWrite(0x00, 0x80, (uint8_t)((temp_raw >> 8) & 0xFF));// Page0 Reg0x80-81: 温度, 16-bit signed, 1/256度C
        CMIS_RegWrite(0x00, 0x81, (uint8_t)(temp_raw & 0xFF));// Page0 Reg0x80-81: 温度, 16-bit signed, 1/256度C
        uint16_t vcc_raw = 33000;// 电压 3.3V 对应的原始值, 16-bit unsigned, 100uV/bit
        CMIS_RegWrite(0x00, 0x82, (uint8_t)((vcc_raw >> 8) & 0xFF));// Page0 Reg0x82-83: 电压, 16-bit unsigned, 100uV/bit
        CMIS_RegWrite(0x00, 0x83, (uint8_t)(vcc_raw & 0xFF));//
        uint8_t duty = LaserTx_GetDutyCycle();// 获取当前激光占空比 (0-100)
        uint16_t tx_pwr = (uint16_t)(duty * 20);// 假设占空比 100% 对应 2000mW, 则每 1% 对应 20mW, 16-bit unsigned, 0.1mW/bit
        CMIS_RegWrite(0x00, 0x86, (uint8_t)((tx_pwr >> 8) & 0xFF));
        CMIS_RegWrite(0x00, 0x87, (uint8_t)(tx_pwr & 0xFF));
        float sig = LaserRx_GetSignalLevel();// 获取当前接收信号电平 (RMS), 0-3.3V
        uint16_t rx_pwr = (uint16_t)(sig / 3.3f * 65535.0f);// 将 0-3.3V 映射到 0-65535, 16-bit unsigned, 0.1mW/bit (假设 3.3V 对应 6553.5mW)
        CMIS_RegWrite(0x00, 0x88, (uint8_t)((rx_pwr >> 8) & 0xFF));
        CMIS_RegWrite(0x00, 0x89, (uint8_t)(rx_pwr & 0xFF));
    }

    // ---- APC 定时更新 (100ms) ----
    // 预留 PI 控制器更新位置
    if (now - last_apc_tick >= 100) {
        last_apc_tick = now;// 每 100ms 更新一次 APC 控制器 (当前未实现, 仅占位)
    }

    // ---- OLED 定时刷新 (500ms) ----
    // 根据当前选择的页面 (由 CMIS 远程切换) 刷新显示
    if (now - last_oled_tick >= 500) {// 每 500ms 刷新一次 OLED 显示
        last_oled_tick = now;
        switch (sys_oled_page) {
            case 0: OLED_ShowPage0(); break;
            case 1: OLED_ShowPage1(); break;
            case 2: OLED_ShowPage2(); break;
            default: sys_oled_page = 0; OLED_ShowPage0(); break;
        }
    }
}

// DMA 传输完成回调: 对采样数据按比特窗口切分, 逐窗口判决并喂入帧状态机
void SysMgr_OnADC_DMAComplete(const uint16_t *buffer, uint16_t len) {
    // 仅在接收相关状态下处理
    if (sys_state != SYS_STATE_RX_LISTENING && sys_state != SYS_STATE_LOOPBACK && sys_state != SYS_STATE_TX) return;

    uint16_t pos = 0;// 当前处理位置
    while (pos + samples_per_bit <= len) {// 按比特窗口切分, 逐窗口判决并喂入帧状态机
        uint8_t bit;
        if (LaserRx_ProcessWindow(&buffer[pos], (uint16_t)samples_per_bit, &bit) == 0) {// 处理一个比特窗口, 获取判决结果
            FrameResult fr = FrameRx_FeedBit(bit);// 将判决结果喂入帧状态机, 获取当前帧解析状态
            if (fr == FRAME_RESULT_OK) {// 成功解析到完整帧, 设置标志位由主循环处理
                rx_frame_ready = 1;
            } else if (fr == FRAME_RESULT_CRC_ERROR) {
                crc_errors++;
            }
        }
        pos += samples_per_bit;// 移动到下一个比特窗口
    }
}

void SysMgr_OnADC_DMAHalfComplete(const uint16_t *buffer, uint16_t len) {
    SysMgr_OnADC_DMAComplete(buffer, len);// 半传输完成时同样处理, 以实现更快的响应
}

void SysMgr_OnUART_RxChar(uint8_t ch) {// UART 接收中断回调, 处理 CMIS 协议数据
    CMIS_OnRxChar(ch);
}

SystemState SysMgr_GetState(void) { return sys_state; }// 获取当前系统状态
uint16_t SysMgr_GetFramesTX(void) { return frames_tx; }// 获取发送帧计数
uint16_t SysMgr_GetFramesRX(void) { return frames_rx; }// 获取接收帧计数
uint16_t SysMgr_GetCRCErrors(void) { return crc_errors; }// 获取 CRC 错误计数
