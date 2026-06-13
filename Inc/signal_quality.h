#ifndef __SIGNAL_QUALITY_H
#define __SIGNAL_QUALITY_H

#include "stm32f4xx_hal.h"

typedef struct {
    float snr_db;
    float signal_rms;
    float noise_rms;
    uint32_t ber_errors;
    uint32_t ber_total;
    float ber;
    uint8_t ber_test_active;
} SignalQuality;

void SigQual_Init(SignalQuality *sq);
void SigQual_UpdateSNR(SignalQuality *sq, float signal_rms, float noise_rms);
void SigQual_RecordBit(SignalQuality *sq, uint8_t expected, uint8_t actual);
void SigQual_StartBERTest(SignalQuality *sq);
void SigQual_StopBERTest(SignalQuality *sq);
void SigQual_GetMetrics(const SignalQuality *sq, float *snr_db, float *ber);

#endif
