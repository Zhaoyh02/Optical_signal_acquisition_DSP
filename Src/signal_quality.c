/**
  * @brief  信号质量评估
  *         SNR (信噪比): SNR_dB = 20 * log10(signal_rms / noise_rms)
  *         BER (误码率): 在回环测试中统计错误比特比例
  */
#include "signal_quality.h"
#include <math.h>
#include <string.h>

void SigQual_Init(SignalQuality *sq) {
    memset(sq, 0, sizeof(*sq));
}

// 更新 SNR: 当 noise 极小时 (<0.001), 设定 SNR=99dB 表示极好
void SigQual_UpdateSNR(SignalQuality *sq, float signal_rms, float noise_rms) {
    sq->signal_rms = signal_rms;
    sq->noise_rms = noise_rms;
    if (noise_rms > 0.001f) {
        sq->snr_db = 20.0f * log10f(signal_rms / noise_rms);
    } else {
        sq->snr_db = 99.0f;   // 噪声可忽略时赋予上限值，避免除零
    }
}

// 记录一次比特判定结果: expected 为发送值, actual 为接收值
// 仅在 BER 测试启动时记录
void SigQual_RecordBit(SignalQuality *sq, uint8_t expected, uint8_t actual) {
    if (!sq->ber_test_active) return;
    sq->ber_total++;
    if (expected != actual) sq->ber_errors++;
    sq->ber = (sq->ber_total > 0)
              ? (float)sq->ber_errors / (float)sq->ber_total : 0.0f;
}

void SigQual_StartBERTest(SignalQuality *sq) {
    sq->ber_test_active = 1;
    sq->ber_errors = 0;
    sq->ber_total = 0;
    sq->ber = 0.0f;
}

void SigQual_StopBERTest(SignalQuality *sq) {
    sq->ber_test_active = 0;
}

void SigQual_GetMetrics(const SignalQuality *sq, float *snr_db, float *ber) {
    if (snr_db) *snr_db = sq->snr_db;
    if (ber) *ber = sq->ber;
}
