/**
  * @brief  激光接收信号处理
  *         对 ADC 采样窗口计算 RMS 值，与门限比较判决比特 0/1
  *         使用 CMSIS-DSP 库的 arm_rms_f32 进行 RMS 计算
  */
#include "laser_rx.h"
#include "arm_math.h"

static float32_t rx_working_buf[512];     // 工作缓冲，保存浮点电压值
static float threshold = 0.5f;            // 判决门限(V)，RMS低于门限判为1（有光），高于判为0（无光）
static float signal_level = 0.0f;         // 当前信号 RMS 值（用于DDM上报）
static uint32_t samples_per_half_bit;
static uint32_t samples_per_bit;

void LaserRx_Init(uint32_t sample_rate_hz, uint32_t bit_rate_hz) {
    samples_per_bit = sample_rate_hz / bit_rate_hz;
    samples_per_half_bit = samples_per_bit / 2;
}

void LaserRx_SetThreshold(float t) {
    threshold = t;
}

float LaserRx_GetSignalLevel(void) {
    return signal_level;
}

// 对采样窗口进行 ADC → 电压转换、RMS计算、门限判决
// 返回 0 成功, -1 失败
int LaserRx_ProcessWindow(const uint16_t *samples, uint16_t count, uint8_t *bit_out) {
    if (count == 0 || count > 512) return -1;

    // ADC值 → 电压值: V = ADC * 3.3 / 4095
    for (uint16_t i = 0; i < count; i++) {
        rx_working_buf[i] = (float32_t)samples[i] * 3.3f / 4095.0f;
    }

    // 计算 RMS (有效值)
    float32_t rms;
    arm_rms_f32(rx_working_buf, count, &rms);
    signal_level = rms;

    // 门限比较: RMS < threshold → 有光信号 → bit=1; RMS >= threshold → 无光 → bit=0
    // 采用 < 操作是因为激光接收在收到光信号时输出低电平
    *bit_out = (rms < threshold) ? 1 : 0;
    return 0;
}

void LaserRx_Reset(void) {
    signal_level = 0.0f;
}
