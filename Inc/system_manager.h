#ifndef __SYSTEM_MANAGER_H
#define __SYSTEM_MANAGER_H

#include "stm32f4xx_hal.h"

typedef enum {
    SYS_STATE_INIT,
    SYS_STATE_IDLE,
    SYS_STATE_TX,
    SYS_STATE_RX_LISTENING,
    SYS_STATE_LOOPBACK,
    SYS_STATE_BER_TEST,
    SYS_STATE_ERROR
} SystemState;

void SysMgr_Init(TIM_HandleTypeDef *htim1, ADC_HandleTypeDef *hadc1,
                 UART_HandleTypeDef *huart1);
void SysMgr_Run(void);
void SysMgr_OnADC_DMAComplete(const uint16_t *buffer, uint16_t len);
void SysMgr_OnADC_DMAHalfComplete(const uint16_t *buffer, uint16_t len);
void SysMgr_OnUART_RxChar(uint8_t ch);

SystemState SysMgr_GetState(void);
uint16_t SysMgr_GetFramesTX(void);
uint16_t SysMgr_GetFramesRX(void);
uint16_t SysMgr_GetCRCErrors(void);

extern volatile uint8_t sys_oled_page;

#endif
