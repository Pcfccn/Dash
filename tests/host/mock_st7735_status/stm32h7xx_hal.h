/* Host-side stand-in for the STM32H7 HAL, ST7735 test only: the GPIO and SPI
 * pieces st7735_status.c uses. Implemented by tests/host/test_st7735_status.c.
 * Shadows tests/host/mock/stm32h7xx_hal.h. */
#ifndef MOCK_ST_STM32H7XX_HAL_H
#define MOCK_ST_STM32H7XX_HAL_H

#include <stdint.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2, HAL_TIMEOUT = 3 } HAL_StatusTypeDef;

/* GPIO */
typedef struct { int id; } GPIO_TypeDef;
extern GPIO_TypeDef mock_gpioc, mock_gpioe;
#define GPIOC (&mock_gpioc)
#define GPIOE (&mock_gpioe)
#define GPIO_PIN_0  0x0001u
#define GPIO_PIN_4  0x0010u
#define GPIO_PIN_5  0x0020u
#define GPIO_PIN_10 0x0400u
#define GPIO_PIN_11 0x0800u
#define GPIO_PIN_12 0x1000u
#define GPIO_PIN_13 0x2000u
#define GPIO_PIN_14 0x4000u
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;
typedef struct { uint32_t Pin, Mode, Pull, Speed, Alternate; } GPIO_InitTypeDef;
#define GPIO_MODE_OUTPUT_PP  0x1u
#define GPIO_MODE_AF_PP      0x2u
#define GPIO_NOPULL          0x0u
#define GPIO_SPEED_FREQ_HIGH 0x2u
#define GPIO_AF5_SPI4        0x5u
#define __HAL_RCC_GPIOE_CLK_ENABLE() ((void)0)
#define __HAL_RCC_SPI4_CLK_ENABLE()  ((void)0)
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState s);
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);

/* SPI */
typedef struct {
    uint32_t Mode, Direction, DataSize, CLKPolarity, CLKPhase, NSS;
    uint32_t BaudRatePrescaler, FirstBit, TIMode, CRCCalculation, NSSPMode;
} SPI_InitTypeDef;
typedef struct { void *Instance; SPI_InitTypeDef Init; } SPI_HandleTypeDef;
#define SPI4 ((void *)0x40013400u)
#define SPI_MODE_MASTER            1u
#define SPI_DIRECTION_1LINE        2u
#define SPI_DATASIZE_8BIT          7u
#define SPI_POLARITY_LOW           0u
#define SPI_PHASE_1EDGE            0u
#define SPI_NSS_SOFT               1u
#define SPI_BAUDRATEPRESCALER_16   3u
#define SPI_FIRSTBIT_MSB           0u
#define SPI_TIMODE_DISABLE         0u
#define SPI_CRCCALCULATION_DISABLE 0u
#define SPI_NSS_PULSE_DISABLE      0u
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *h);
HAL_StatusTypeDef HAL_SPI_DeInit(SPI_HandleTypeDef *h);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *h, const uint8_t *p, uint16_t n, uint32_t timeout);
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *h);

uint32_t HAL_GetTick(void);

#endif
