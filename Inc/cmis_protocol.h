#ifndef __CMIS_PROTOCOL_H
#define __CMIS_PROTOCOL_H

#include "stm32f4xx_hal.h"

extern uint8_t cmis_uart_rx_char;

void CMIS_Init(UART_HandleTypeDef *huart);
void CMIS_Process(void);
void CMIS_OnRxChar(uint8_t ch);

uint8_t CMIS_RegRead(uint8_t page, uint8_t addr);
void    CMIS_RegWrite(uint8_t page, uint8_t addr, uint8_t data);

#define CMIS_PAGE_STATUS    0x00// 0x00-0x7F 状态寄存器, 只读
#define CMIS_PAGE_CONFIG    0x01// 0x00-0x7F 配置寄存器, 可读写

#endif
