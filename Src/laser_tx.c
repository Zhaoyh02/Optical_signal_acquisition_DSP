/**
  * @brief  激光发送驱动
  *         通过 TIM1 PWM 控制激光二极管亮灭 (OOK 调制: 有光=1, 无光=0)
  *         使用 DWT (Data Watchpoint Trace) 单元实现微秒级精确延时
  */
#include "laser_tx.h"
#include "stm32f4xx_hal_rcc.h"

static TIM_HandleTypeDef *tx_htim;
static uint32_t tx_channel;
static uint16_t bit_period_us = 2000;     // 每比特持续时长 (us), 默认 2000us = 500bps
static uint8_t  duty_percent = 50;        // PWM占空比 (%), 0=完全关闭, 最大90%保护激光器

static uint32_t dwt_initialized = 0;

// DWT 初始化: 使能 Cortex-M4 的 DWT 周期计数器，用于精确延时
static void DWT_Init(void) {
    if (!dwt_initialized) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        dwt_initialized = 1;
    }
}

// DWT 微秒级延时: 基于 CPU 主频计算延时周期数
static void DWT_DelayUS(uint32_t us) {
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (HAL_RCC_GetSysClockFreq() / 1000000);
    while ((DWT->CYCCNT - start) < ticks) { }
}

void LaserTx_Init(TIM_HandleTypeDef *htim, uint32_t channel) {
    tx_htim = htim;
    tx_channel = channel;
    DWT_Init();
}

void LaserTx_SetBitPeriod(uint16_t period_us) {
    bit_period_us = period_us;
}

void LaserTx_SetDutyCycle(uint8_t percent) {
    if (percent > 90) percent = 90;   // 限制最大占空比，保护激光二极管
    duty_percent = percent;
}

uint8_t LaserTx_GetDutyCycle(void) {
    return duty_percent;
}

// 发送单个比特: bit=1 时设置 PWM 占空比(duty%), bit=0 时关闭 PWM
void LaserTx_SendBit(uint8_t bit) {
    uint32_t ccr = (bit && duty_percent > 0) ? duty_percent : 0;
    __HAL_TIM_SET_COMPARE(tx_htim, tx_channel, ccr);
    DWT_DelayUS(bit_period_us);
}

// 发送一个字节，MSB 先发
void LaserTx_SendByte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        LaserTx_SendBit((byte >> i) & 0x01);
    }
}

// 连续发送缓冲区
void LaserTx_SendBuffer(const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        LaserTx_SendByte(data[i]);
    }
}

// 打开载波 (保持发光，不调制)
void LaserTx_CarrierOn(void) {
    __HAL_TIM_SET_COMPARE(tx_htim, tx_channel, duty_percent);
}

// 关闭载波
void LaserTx_CarrierOff(void) {
    __HAL_TIM_SET_COMPARE(tx_htim, tx_channel, 0);
}
