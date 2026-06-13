#ifndef __LASER_RX_H
#define __LASER_RX_H

#include "stm32f4xx_hal.h"

void LaserRx_Init(uint32_t sample_rate_hz, uint32_t bit_rate_hz);
void LaserRx_SetThreshold(float threshold);
float LaserRx_GetSignalLevel(void);
int LaserRx_ProcessWindow(const uint16_t *samples, uint16_t count, uint8_t *bit_out);
void LaserRx_Reset(void);

#endif
