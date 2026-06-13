#ifndef __FRAME_PROTOCOL_H
#define __FRAME_PROTOCOL_H

#include "stm32f4xx_hal.h"

#define FRAME_PREAMBLE_LEN  4
#define FRAME_SYNC_LEN      2
#define FRAME_HDR_LEN       8
#define FRAME_CRC_LEN       2
#define FRAME_MAX_PAYLOAD   255
#define FRAME_MAX_TOTAL     (FRAME_HDR_LEN + FRAME_MAX_PAYLOAD + FRAME_CRC_LEN)

typedef enum {
    FRAME_RESULT_OK = 0,
    FRAME_RESULT_SYNC_LOST,
    FRAME_RESULT_BAD_LENGTH,
    FRAME_RESULT_CRC_ERROR,
    FRAME_RESULT_OVERRUN,
    FRAME_RESULT_INCOMPLETE
} FrameResult;

typedef enum {
    FRAME_RX_SEEK_PREAMBLE,
    FRAME_RX_SEEK_SYNC,
    FRAME_RX_READ_LENGTH,
    FRAME_RX_READ_PAYLOAD,
    FRAME_RX_READ_CRC,
    FRAME_RX_COMPLETE
} FrameRxState;

uint16_t Frame_Encode(const uint8_t *payload, uint16_t payload_len,
                      uint8_t *frame_out, uint16_t frame_buf_size);

void FrameRx_Reset(void);
FrameResult FrameRx_FeedBit(uint8_t bit);
FrameResult FrameRx_GetResult(uint8_t *payload_out, uint16_t *len_out);
FrameRxState FrameRx_GetState(void);
uint16_t FrameRx_GetFramesReceived(void);
uint16_t FrameRx_GetCRCErrors(void);
uint16_t CRC16_CCITT(const uint8_t *data, uint16_t len);

#endif
