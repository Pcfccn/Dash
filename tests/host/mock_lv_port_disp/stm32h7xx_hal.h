/* Host-side stand-in for the STM32H7 HAL, display test only: the GPIO, SPI,
 * DMA and NVIC pieces lv_port_disp.c uses. Implemented by
 * tests/host/test_lv_port_disp.c. Shadows tests/host/mock/stm32h7xx_hal.h. */
#ifndef MOCK_DISP_STM32H7XX_HAL_H
#define MOCK_DISP_STM32H7XX_HAL_H

#include <stdint.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 } HAL_StatusTypeDef;

/* GPIO */
typedef struct { int id; } GPIO_TypeDef;
extern GPIO_TypeDef mock_gpiob, mock_gpioc;
#define GPIOB (&mock_gpiob)
#define GPIOC (&mock_gpioc)
#define GPIO_PIN_0  0x0001u
#define GPIO_PIN_4  0x0010u
#define GPIO_PIN_5  0x0020u
#define GPIO_PIN_13 0x2000u
#define GPIO_PIN_14 0x4000u
#define GPIO_PIN_15 0x8000u
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
#define GPIO_MODE_AF_PP      0x2u
#define GPIO_NOPULL          0x0u
#define GPIO_SPEED_FREQ_HIGH 0x2u
#define GPIO_AF5_SPI2        0x5u
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState s);
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);

/* DMA */
typedef struct {
    uint32_t Request, Direction, PeriphInc, MemInc, PeriphDataAlignment;
    uint32_t MemDataAlignment, Mode, Priority, FIFOMode;
} DMA_InitTypeDef;
typedef struct { void *Instance; DMA_InitTypeDef Init; } DMA_HandleTypeDef;
#define DMA1_Stream0          ((void *)0x40020010u)
#define DMA_REQUEST_SPI2_TX   40u
#define DMA_MEMORY_TO_PERIPH  0x40u
#define DMA_PINC_DISABLE      0u
#define DMA_MINC_ENABLE       0x400u
#define DMA_PDATAALIGN_BYTE   0u
#define DMA_MDATAALIGN_BYTE   0u
#define DMA_NORMAL            0u
#define DMA_PRIORITY_LOW      0u
#define DMA_FIFOMODE_DISABLE  0u
#define __HAL_RCC_DMA1_CLK_ENABLE() ((void)0)
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef *h);
void              HAL_DMA_IRQHandler(DMA_HandleTypeDef *h);

/* SPI */
typedef struct {
    uint32_t Mode, Direction, DataSize, CLKPolarity, CLKPhase, NSS, FirstBit, BaudRatePrescaler;
} SPI_InitTypeDef;
typedef struct { SPI_InitTypeDef Init; DMA_HandleTypeDef *hdmatx; } SPI_HandleTypeDef;
#define SPI_MODE_MASTER          1u
#define SPI_DIRECTION_1LINE      2u
#define SPI_DATASIZE_8BIT        7u
#define SPI_POLARITY_LOW         0u
#define SPI_PHASE_1EDGE          0u
#define SPI_NSS_SOFT             1u
#define SPI_FIRSTBIT_MSB         0u
#define SPI_BAUDRATEPRESCALER_8  2u
#define __HAL_LINKDMA(h, field, dma) ((h)->field = &(dma))
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *h);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *h, const uint8_t *p, uint16_t n, uint32_t timeout);
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *h, const uint8_t *p, uint16_t n);
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *h);
void              HAL_SPI_IRQHandler(SPI_HandleTypeDef *h);

/* NVIC, time, cache */
typedef enum { DMA1_Stream0_IRQn = 11, SPI2_IRQn = 36 } IRQn_Type;
void     HAL_NVIC_SetPriority(IRQn_Type irq, uint32_t pre, uint32_t sub);
void     HAL_NVIC_EnableIRQ(IRQn_Type irq);
uint32_t HAL_GetTick(void);
void     HAL_Delay(uint32_t ms);
void     SCB_CleanDCache_by_Addr(uint32_t *addr, int32_t size);

#endif
