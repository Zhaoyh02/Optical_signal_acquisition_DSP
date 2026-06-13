#ifndef __LASER_TX_H
#define __LASER_TX_H

#include "stm32f4xx_hal.h"

void LaserTx_Init(TIM_HandleTypeDef *htim, uint32_t channel);
void LaserTx_SetBitPeriod(uint16_t period_us);
void LaserTx_SetDutyCycle(uint8_t percent);
uint8_t LaserTx_GetDutyCycle(void);
void LaserTx_SendBit(uint8_t bit);
void LaserTx_SendByte(uint8_t byte);
void LaserTx_SendBuffer(const uint8_t *data, uint16_t len);
void LaserTx_CarrierOn(void);
void LaserTx_CarrierOff(void);

#endif
