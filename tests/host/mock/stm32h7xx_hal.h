/* Host-side stand-in for the STM32H7 HAL: just the FDCAN types, constants and
 * calls fdcan_obd.c uses. The functions are implemented by the test harness. */
#ifndef MOCK_STM32H7XX_HAL_H
#define MOCK_STM32H7XX_HAL_H

#include <stdint.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 } HAL_StatusTypeDef;

typedef struct { int dummy; } FDCAN_HandleTypeDef;

typedef struct {
    uint32_t Identifier, IdType, TxFrameType, DataLength, ErrorStateIndicator;
    uint32_t BitRateSwitch, FDFormat, TxEventFifoControl, MessageMarker;
} FDCAN_TxHeaderTypeDef;

typedef struct {
    uint32_t Identifier, IdType, RxFrameType, DataLength, ErrorStateIndicator;
    uint32_t BitRateSwitch, FDFormat, RxTimestamp, FilterIndex, IsFilterMatchingFrame;
} FDCAN_RxHeaderTypeDef;

typedef struct {
    uint32_t IdType, FilterIndex, FilterType, FilterConfig, FilterID1, FilterID2, RxBufferIndex;
} FDCAN_FilterTypeDef;

typedef struct {
    uint32_t LastErrorCode, DataLastErrorCode, Activity, ErrorPassive, Warning, BusOff;
} FDCAN_ProtocolStatusTypeDef;

#define FDCAN_STANDARD_ID       0x00000000U
#define FDCAN_EXTENDED_ID       0x40000000U
#define FDCAN_DATA_FRAME        0x00000000U
#define FDCAN_REMOTE_FRAME      0x20000000U
#define FDCAN_DLC_BYTES_8       0x00000008U
#define FDCAN_CLASSIC_CAN       0x00000000U
#define FDCAN_BRS_OFF           0x00000000U
#define FDCAN_ESI_ACTIVE        0x00000000U
#define FDCAN_NO_TX_EVENTS      0x00000000U
#define FDCAN_RX_FIFO0          0x00000040U
#define FDCAN_FILTER_RANGE      0x00000000U
#define FDCAN_FILTER_MASK       0x00000002U
#define FDCAN_FILTER_TO_RXFIFO0 0x00000001U
#define FDCAN_REJECT            0x00000002U
#define FDCAN_REJECT_REMOTE     0x00000001U
#define FDCAN_FLAG_RX_FIFO0_MESSAGE_LOST 0x00000008U

#define __HAL_FDCAN_GET_FLAG(h, f)   (0)
#define __HAL_FDCAN_CLEAR_FLAG(h, f) ((void)0)

uint32_t          HAL_GetTick(void);
HAL_StatusTypeDef HAL_FDCAN_ConfigFilter(FDCAN_HandleTypeDef *h, FDCAN_FilterTypeDef *f);
HAL_StatusTypeDef HAL_FDCAN_ConfigGlobalFilter(FDCAN_HandleTypeDef *h, uint32_t a, uint32_t b,
                                               uint32_t c, uint32_t d);
HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef *h);
HAL_StatusTypeDef HAL_FDCAN_Stop(FDCAN_HandleTypeDef *h);
HAL_StatusTypeDef HAL_FDCAN_AddMessageToTxFifoQ(FDCAN_HandleTypeDef *h,
                                                FDCAN_TxHeaderTypeDef *tx, uint8_t *data);
uint32_t          HAL_FDCAN_GetRxFifoFillLevel(FDCAN_HandleTypeDef *h, uint32_t fifo);
HAL_StatusTypeDef HAL_FDCAN_GetRxMessage(FDCAN_HandleTypeDef *h, uint32_t loc,
                                         FDCAN_RxHeaderTypeDef *rh, uint8_t *data);
HAL_StatusTypeDef HAL_FDCAN_GetProtocolStatus(FDCAN_HandleTypeDef *h,
                                              FDCAN_ProtocolStatusTypeDef *ps);

#endif
