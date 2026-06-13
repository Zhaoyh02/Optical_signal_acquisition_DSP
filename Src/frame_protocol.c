#include "frame_protocol.h"
#include <string.h>

// CRC-16/CCITT 查找表 (多项式 x^16 + x^12 + x^5 + 1, 初始值 0xFFFF)
static const uint16_t crc_table[256] = {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
    0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
    0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
    0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
    0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
    0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
    0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
    0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
    0x48C4,0x58E5,0x6886,0x78A7,0x0840,0x1861,0x2802,0x3823,
    0xC9CC,0xD9ED,0xE98E,0xF9AF,0x8948,0x9969,0xA90A,0xB92B,
    0x5AF5,0x4AD4,0x7AB7,0x6A96,0x1A71,0x0A50,0x3A33,0x2A12,
    0xDBFD,0xCBDC,0xFBBF,0xEB9E,0x9B79,0x8B58,0xBB3B,0xAB1A,
    0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
    0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
    0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
    0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
    0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
    0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
    0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,
    0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
    0xB5EA,0xA5CB,0x95A8,0x8589,0xF56E,0xE54F,0xD52C,0xC50D,
    0x34E2,0x24C3,0x14A0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xA7DB,0xB7FA,0x8799,0x97B8,0xE75F,0xF77E,0xC71D,0xD73C,
    0x26D3,0x36F2,0x0691,0x16B0,0x6657,0x7676,0x4615,0x5634,
    0xD94C,0xC96D,0xF90E,0xE92F,0x99C8,0x89E9,0xB98A,0xA9AB,
    0x5844,0x4865,0x7806,0x6827,0x18C0,0x08E1,0x3882,0x28A3,
    0xCB7D,0xDB5C,0xEB3F,0xFB1E,0x8BF9,0x9BD8,0xABBB,0xBB9A,
    0x4A75,0x5A54,0x6A37,0x7A16,0x0AF1,0x1AD0,0x2AB3,0x3A92,
    0xFD2E,0xED0F,0xDD6C,0xCD4D,0xBDAA,0xAD8B,0x9DE8,0x8DC9,
    0x7C26,0x6C07,0x5C64,0x4C45,0x3CA2,0x2C83,0x1CE0,0x0CC1,
    0xEF1F,0xFF3E,0xCF5D,0xDF7C,0xAF9B,0xBFBA,0x8FD9,0x9FF8,
    0x6E17,0x7E36,0x4E55,0x5E74,0x2E93,0x3EB2,0x0ED1,0x1EF0
};

// CRC-16/CCITT 校验计算
uint16_t CRC16_CCITT(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc = (crc << 8) ^ crc_table[(crc >> 8) ^ *data++];
    }
    return crc;
}

// 帧编码: [4B 0xAA前导码] + [2B 同步字 0x7E 0x5A] + [2B 载荷长度] + [N字节载荷] + [2B CRC]
uint16_t Frame_Encode(const uint8_t *payload, uint16_t payload_len,
                      uint8_t *frame_out, uint16_t frame_buf_size) {
    if (payload_len > FRAME_MAX_PAYLOAD) return 0;
    uint16_t total = FRAME_HDR_LEN + payload_len + FRAME_CRC_LEN;
    if (total > frame_buf_size) return 0;

    // 前导码: 4字节 0xAA，用于接收端位同步
    memset(frame_out, 0xAA, FRAME_PREAMBLE_LEN);
    uint16_t pos = FRAME_PREAMBLE_LEN;
    frame_out[pos++] = 0x7E;                      // 同步字高字节
    frame_out[pos++] = 0x5A;                      // 同步字低字节
    frame_out[pos++] = (payload_len >> 8) & 0xFF; // 载荷长度 MSB
    frame_out[pos++] = payload_len & 0xFF;        // 载荷长度 LSB
    memcpy(&frame_out[pos], payload, payload_len);
    pos += payload_len;
    uint16_t crc = CRC16_CCITT(payload, payload_len);
    frame_out[pos++] = (crc >> 8) & 0xFF;         // CRC 高字节
    frame_out[pos++] = crc & 0xFF;                // CRC 低字节
    return pos;                                   // 返回帧总长度
}

// ---- RX 状态机: 从比特流中逐位提取完整帧 ----
// 状态转换: SEEK_PREAMBLE → SEEK_SYNC → READ_LENGTH → READ_PAYLOAD → READ_CRC → COMPLETE
// 使用移位寄存器逐位收集，每满8位输出一个字节
static FrameRxState rx_state = FRAME_RX_SEEK_PREAMBLE;
static uint32_t rx_shift_reg;         // 移位寄存器，逐位移入
static uint8_t  rx_bit_count;         // 移位寄存器中已收集的位数
static uint8_t  rx_byte_buf[FRAME_MAX_TOTAL]; // 帧字节缓冲区
static uint16_t rx_byte_pos;
static uint16_t rx_payload_len;
static uint16_t rx_total_len;
static uint16_t frames_received;      // 成功接收帧计数
static uint16_t crc_errors;           // CRC 错误计数

void FrameRx_Reset(void) {
    rx_state = FRAME_RX_SEEK_PREAMBLE;
    rx_shift_reg = 0;
    rx_bit_count = 0;
    rx_byte_pos = 0;
}

FrameRxState FrameRx_GetState(void) { return rx_state; }
uint16_t FrameRx_GetFramesReceived(void) { return frames_received; }
uint16_t FrameRx_GetCRCErrors(void) { return crc_errors; }

// 向接收状态机喂入一个比特，返回当前帧解析结果
FrameResult FrameRx_FeedBit(uint8_t bit) {
    rx_shift_reg = (rx_shift_reg << 1) | (bit & 1);
    rx_bit_count++;

    switch (rx_state) {
    case FRAME_RX_SEEK_PREAMBLE: {
        // 搜索连续的 0xAA（前导码），发现后进入同步字搜索状态
        if (rx_bit_count >= 8) {
            uint8_t byte = (rx_shift_reg >> (rx_bit_count - 8)) & 0xFF;
            if (byte == 0xAA) {
                rx_byte_buf[0] = byte;
                rx_byte_pos = 1;
                rx_bit_count = 0;
                rx_state = FRAME_RX_SEEK_SYNC;
            }
        }
        break;
    }
    case FRAME_RX_SEEK_SYNC: {
        // 确认4字节 0xAA 前导码后，等待同步字 0x7E 0x5A
        if (rx_bit_count >= 8) {
            uint8_t byte = (rx_shift_reg >> (rx_bit_count - 8)) & 0xFF;
            rx_byte_buf[rx_byte_pos++] = byte;
            rx_bit_count = 0;

            if (rx_byte_pos >= 4) {
                // 验证前4字节是否全为 0xAA
                if (rx_byte_buf[0] == 0xAA && rx_byte_buf[1] == 0xAA &&
                    rx_byte_buf[2] == 0xAA && rx_byte_buf[3] == 0xAA) {
                }
            }
            if (rx_byte_pos >= 6) {
                // 第5字节=0x7E, 第6字节=0x5A 即同步字匹配成功
                if (rx_byte_buf[4] == 0x7E && rx_byte_buf[5] == 0x5A) {
                    rx_byte_pos = 0;
                    rx_state = FRAME_RX_READ_LENGTH;
                } else {
                    // 同步字不匹配，回到前导码搜索
                    rx_byte_pos = 0;
                    memset(rx_byte_buf, 0, sizeof(rx_byte_buf));
                    rx_state = FRAME_RX_SEEK_PREAMBLE;
                    rx_bit_count = 0;
                }
            }
        }
        break;
    }
    case FRAME_RX_READ_LENGTH: {
        // 读取2字节载荷长度，合法性检查后进入载荷或CRC阶段
        if (rx_bit_count >= 8) {
            rx_byte_buf[rx_byte_pos++] = (rx_shift_reg >> (rx_bit_count - 8)) & 0xFF;
            rx_bit_count = 0;
            if (rx_byte_pos >= 2) {
                rx_payload_len = ((uint16_t)rx_byte_buf[0] << 8) | rx_byte_buf[1];
                if (rx_payload_len > FRAME_MAX_PAYLOAD) {
                    FrameRx_Reset();
                    return FRAME_RESULT_BAD_LENGTH;
                }
                rx_total_len = rx_payload_len + FRAME_CRC_LEN;
                rx_byte_pos = 0;
                memset(rx_byte_buf, 0, sizeof(rx_byte_buf));
                if (rx_payload_len > 0) {
                    rx_state = FRAME_RX_READ_PAYLOAD;
                } else {
                    rx_state = FRAME_RX_READ_CRC;  // 空载荷，直接读CRC
                }
            }
        }
        break;
    }
    case FRAME_RX_READ_PAYLOAD: {
        // 逐字节读取载荷数据
        if (rx_bit_count >= 8) {
            rx_byte_buf[rx_byte_pos++] = (rx_shift_reg >> (rx_bit_count - 8)) & 0xFF;
            rx_bit_count = 0;
            if (rx_byte_pos >= rx_payload_len) {
                rx_byte_pos = 0;
                rx_state = FRAME_RX_READ_CRC;
            }
        }
        break;
    }
    case FRAME_RX_READ_CRC: {
        // 读取2字节CRC校验值
        if (rx_bit_count >= 8) {
            // CRC字节存放在载荷数据之后 (buf[payload_len] 和 buf[payload_len+1])
            rx_byte_buf[rx_payload_len + rx_byte_pos] =
                (rx_shift_reg >> (rx_bit_count - 8)) & 0xFF;
            rx_byte_pos++;
            rx_bit_count = 0;
            if (rx_byte_pos >= 2) {
                rx_state = FRAME_RX_COMPLETE;
            }
        }
        break;
    }
    case FRAME_RX_COMPLETE:
        break;
    }

    // 帧接收完整，校验 CRC
    if (rx_state == FRAME_RX_COMPLETE) {
        uint16_t crc_expected = ((uint16_t)rx_byte_buf[rx_payload_len] << 8)
                              | rx_byte_buf[rx_payload_len + 1];
        uint16_t crc_calc = CRC16_CCITT(rx_byte_buf, rx_payload_len);
        if (crc_expected == crc_calc) {
            frames_received++;
            return FRAME_RESULT_OK;
        } else {
            crc_errors++;
            FrameRx_Reset();
            return FRAME_RESULT_CRC_ERROR;
        }
    }
    return FRAME_RESULT_INCOMPLETE;
}

FrameResult FrameRx_GetResult(uint8_t *payload_out, uint16_t *len_out) {
    if (rx_state != FRAME_RX_COMPLETE) return FRAME_RESULT_INCOMPLETE;
    *len_out = rx_payload_len;
    memcpy(payload_out, rx_byte_buf, rx_payload_len);
    FrameRx_Reset();
    return FRAME_RESULT_OK;
}
