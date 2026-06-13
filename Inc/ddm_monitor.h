#ifndef __DDM_MONITOR_H
#define __DDM_MONITOR_H

#include "stm32f4xx_hal.h"

typedef struct {
    int16_t  temperature;
    uint16_t vcc;
    uint16_t tx_bias;
    uint16_t tx_power;
    uint16_t rx_power;
} DDM_Params;

void DDM_Init(ADC_HandleTypeDef *hadc);
void DDM_Update(void);
void DDM_GetParams(DDM_Params *out);
float DDM_GetTemperatureCelsius(void);
float DDM_GetVCC(void);

#endif
