#ifndef __APC_CONTROL_H
#define __APC_CONTROL_H

#include "stm32f4xx_hal.h"

typedef struct {
    float kp;
    float ki;
    float setpoint;
    float integral;
    float integral_max;
    float output_min;
    float output_max;
    uint8_t enabled;
} APC_Controller;

void APC_Init(APC_Controller *apc, float kp, float ki, float setpoint);
float APC_Update(APC_Controller *apc, float measured, float dt);
void APC_Enable(APC_Controller *apc);
void APC_Disable(APC_Controller *apc);
void APC_SetSetpoint(APC_Controller *apc, float setpoint);
void APC_Reset(APC_Controller *apc);

#endif
