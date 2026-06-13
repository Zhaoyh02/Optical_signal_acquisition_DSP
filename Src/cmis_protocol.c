/**
  * @brief  CMIS (Common Management Interface Specification) 协议实现
  *         模拟 SFP/SFP+ 光模块的管理接口，通过 UART 接收主机命令，读写寄存器映射表
  *         命令格式: R<page><addr> 读寄存器, W<page><addr><data> 写寄存器（均为 hex）
  *         响应格式: +<value> 表示成功, -01 表示失败
  */
#include "cmis_protocol.h"
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *cmis_huart;

#define RX_BUF_SIZE 32
static uint8_t rx_buf[RX_BUF_SIZE];
static uint8_t rx_idx = 0;
uint8_t cmis_uart_rx_char;           // 由 UART 中断回调写入的单字节

// 寄存器映射表: 256 页 x 256 地址 = 512 字节 (page << 8 | addr)
#define REG_MAP_SIZE 0x200
static uint8_t reg_map[REG_MAP_SIZE];

// 通过 UART 发送字符串响应给主机
static void CMIS_SendResponse(const char *resp) {
    HAL_UART_Transmit(cmis_huart, (uint8_t *)resp, strlen(resp), 100);
}

// 解析并执行收到的命令（以 \n 结尾的一行）
static void CMIS_HandleCommand(void) {
    rx_buf[rx_idx] = '\0';
    char *cmd = (char *)rx_buf;

    char line[64];
    if (cmd[0] == 'R' || cmd[0] == 'r') {          // 读命令: R + 2位page + 2位addr
        unsigned int page, addr;
        if (sscanf(cmd + 1, "%2x%2x", &page, &addr) == 2) {
            uint16_t reg_idx = (page << 8) | addr;
            if (reg_idx < REG_MAP_SIZE) {
                uint8_t val = CMIS_RegRead((uint8_t)page, (uint8_t)addr);
                snprintf(line, sizeof(line), "+%02X\r\n", val);
                CMIS_SendResponse(line);
                rx_idx = 0; return;
            }
        }
        CMIS_SendResponse("-01\r\n");
    } else if (cmd[0] == 'W' || cmd[0] == 'w') {   // 写命令: W + 2位page + 2位addr + 2位data
        unsigned int page, addr, data;
        if (sscanf(cmd + 1, "%2x%2x%2x", &page, &addr, &data) == 3) {
            uint16_t reg_idx = (page << 8) | addr;
            if (reg_idx < REG_MAP_SIZE) {
                CMIS_RegWrite((uint8_t)page, (uint8_t)addr, (uint8_t)data);
                CMIS_SendResponse("+\r\n");
                rx_idx = 0; return;
            }
        }
        CMIS_SendResponse("-01\r\n");
    } else {
        CMIS_SendResponse("-01\r\n");
    }
    rx_idx = 0;
}

// 初始化 CMIS 协议引擎，绑定 UART，清零寄存器表，写入模块标识
void CMIS_Init(UART_HandleTypeDef *huart) {
    cmis_huart = huart;
    memset(reg_map, 0, sizeof(reg_map));

    // 模块 Vendor: 写入 "OTRX" 标识 (Page 0x00, Addr 0x03-0x06)
    reg_map[(0x00 << 8) | 0x00] = 0x01;
    reg_map[(0x00 << 8) | 0x03] = 'O';
    reg_map[(0x00 << 8) | 0x04] = 'T';
    reg_map[(0x00 << 8) | 0x05] = 'R';
    reg_map[(0x00 << 8) | 0x06] = 'X';

    rx_idx = 0;
    // 启动 UART 中断接收，每收到一个字节触发回调
    HAL_UART_Receive_IT(cmis_huart, &cmis_uart_rx_char, 1);
}

// UART 接收字符回调: 以 \n 为命令分隔符，\r\n 兼容处理
void CMIS_OnRxChar(uint8_t ch) {
    if (ch == '\r' || ch == '\n') {
        if (ch == '\n' && rx_idx > 0) {
            CMIS_HandleCommand();
        }
    } else {
        if (rx_idx < RX_BUF_SIZE - 1) {
            rx_buf[rx_idx++] = ch;
        }
    }
}

// 主循环调用，UART 收发自中断驱动，暂无轮询任务
void CMIS_Process(void) {
}

// 读寄存器: page + addr 合成 11位地址 (0x000 ~ 0x1FF)
uint8_t CMIS_RegRead(uint8_t page, uint8_t addr) {
    uint16_t idx = ((uint16_t)page << 8) | addr;
    if (idx < REG_MAP_SIZE) return reg_map[idx];
    return 0x00;
}

// 写寄存器
void CMIS_RegWrite(uint8_t page, uint8_t addr, uint8_t data) {
    uint16_t idx = ((uint16_t)page << 8) | addr;
    if (idx < REG_MAP_SIZE) {
        reg_map[idx] = data;
    }
}
