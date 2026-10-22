/* spi_nor.c — W25Q64 on SPI1, see spi_nor.h. */
#include "spi_nor.h"
#include "main.h"

#define NOR_CS_PORT  GPIOD
#define NOR_CS_PIN   GPIO_PIN_6
#define NOR_TIMEOUT_MS 10u           /* a command and up to 256 bytes at 18.75 MHz: ~0.1 ms */

/* W25Qxx commands */
#define CMD_WREN     0x06u
#define CMD_RDSR1    0x05u
#define CMD_READ     0x03u
#define CMD_PP       0x02u
#define CMD_SE4K     0x20u
#define CMD_JEDEC    0x9Fu
#define CMD_RELPD    0xABu           /* release from power-down          */
#define CMD_RSTEN    0x66u
#define CMD_RST      0x99u
#define SR1_WIP      0x01u

static SPI_HandleTypeDef s_spi;
static uint32_t          s_jedec;
static bool              s_ok;

static void cs_low(void)  { HAL_GPIO_WritePin(NOR_CS_PORT, NOR_CS_PIN, GPIO_PIN_RESET); }
static void cs_high(void) { HAL_GPIO_WritePin(NOR_CS_PORT, NOR_CS_PIN, GPIO_PIN_SET); }

static bool tx(const uint8_t *p, uint16_t n)
{
    if (HAL_SPI_Transmit(&s_spi, (uint8_t *)p, n, NOR_TIMEOUT_MS) == HAL_OK) return true;
    HAL_SPI_Abort(&s_spi);
    return false;
}

static bool rx(uint8_t *p, uint16_t n)
{
    if (HAL_SPI_Receive(&s_spi, p, n, NOR_TIMEOUT_MS) == HAL_OK) return true;
    HAL_SPI_Abort(&s_spi);
    return false;
}

/* One CS-framed command: header bytes out, then optional data out or in. */
static bool cmd(const uint8_t *hdr, uint16_t nh, const uint8_t *out, uint8_t *in, uint16_t nd)
{
    cs_low();
    bool ok = tx(hdr, nh);
    if (ok && nd) ok = out ? tx(out, nd) : rx(in, nd);
    cs_high();
    return ok;
}

static bool cmd1(uint8_t c) { return cmd(&c, 1, NULL, NULL, 0); }

static void spi1_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();          /* kernel clock: SPI123 = PLL1Q 150 MHz (spi.c) */

    GPIO_InitTypeDef gi = { 0 };
    gi.Pin       = GPIO_PIN_3 | GPIO_PIN_4;          /* SCK, MISO */
    gi.Mode      = GPIO_MODE_AF_PP;
    gi.Pull      = GPIO_NOPULL;
    gi.Speed     = GPIO_SPEED_FREQ_HIGH;
    gi.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOB, &gi);
    gi.Pin       = GPIO_PIN_7;                       /* MOSI */
    HAL_GPIO_Init(GPIOD, &gi);

    cs_high();
    gi.Pin       = NOR_CS_PIN;
    gi.Mode      = GPIO_MODE_OUTPUT_PP;
    gi.Pull      = GPIO_PULLUP;
    gi.Alternate = 0;
    HAL_GPIO_Init(NOR_CS_PORT, &gi);

    s_spi.Instance               = SPI1;
    s_spi.Init.Mode              = SPI_MODE_MASTER;
    s_spi.Init.Direction         = SPI_DIRECTION_2LINES;
    s_spi.Init.DataSize          = SPI_DATASIZE_8BIT;
    s_spi.Init.CLKPolarity       = SPI_POLARITY_LOW;    /* mode 0 */
    s_spi.Init.CLKPhase          = SPI_PHASE_1EDGE;
    s_spi.Init.NSS               = SPI_NSS_SOFT;
    s_spi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;   /* 18.75 MHz, chip max 40+ */
    s_spi.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    s_spi.Init.TIMode            = SPI_TIMODE_DISABLE;
    s_spi.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    s_spi.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
    s_spi.Init.FifoThreshold     = SPI_FIFO_THRESHOLD_01DATA;
    s_spi.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;  /* SCK stays low between frames */
    (void)HAL_SPI_Init(&s_spi);           /* HAL_SPI_MspInit (spi.c) only handles SPI2 */
}

bool nor_init(void)
{
    spi1_init();
    /* Wake it up (a previous image may have left it in power-down) and reset
     * it, so a program interrupted by a reset of ours is not still running. */
    (void)cmd1(CMD_RELPD);
    HAL_Delay(1);
    (void)cmd1(CMD_RSTEN);
    (void)cmd1(CMD_RST);
    HAL_Delay(1);

    uint8_t c = CMD_JEDEC, id[3] = { 0 };
    s_ok = cmd(&c, 1, NULL, id, 3);
    s_jedec = s_ok ? ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2] : 0u;
    /* Winbond (0xEF), 25-series; capacity code 0x17 = 8 MB, larger also fits. */
    s_ok = s_ok && id[0] == 0xEFu && id[2] >= 0x17u && id[2] <= 0x20u;
    if (!s_ok && s_jedec == 0xFFFFFFu) s_jedec = 0u;   /* nothing on the bus */
    return s_ok;
}

uint32_t nor_jedec(void) { return s_jedec; }

static void addr_hdr(uint8_t *h, uint8_t c, uint32_t a)
{
    h[0] = c; h[1] = (uint8_t)(a >> 16); h[2] = (uint8_t)(a >> 8); h[3] = (uint8_t)a;
}

bool nor_read(uint32_t addr, void *buf, uint32_t len)
{
    if (!s_ok || addr + len > NOR_SIZE) return false;
    uint8_t *p = buf;
    while (len) {                         /* HAL sizes are 16-bit */
        uint16_t n = (len > 4096u) ? 4096u : (uint16_t)len;
        uint8_t h[4];
        addr_hdr(h, CMD_READ, addr);
        if (!cmd(h, 4, NULL, p, n)) return false;
        addr += n; p += n; len -= n;
    }
    return true;
}

bool nor_busy(void)
{
    if (!s_ok) return false;
    uint8_t c = CMD_RDSR1, sr = 0xFFu;
    if (!cmd(&c, 1, NULL, &sr, 1)) return true;   /* unknown: treat as busy */
    return (sr & SR1_WIP) != 0u;
}

bool nor_program(uint32_t addr, const void *buf, uint32_t len)
{
    if (!s_ok || len == 0u || len > NOR_PAGE || addr + len > NOR_SIZE) return false;
    if ((addr / NOR_PAGE) != ((addr + len - 1u) / NOR_PAGE)) return false;
    if (nor_busy() || !cmd1(CMD_WREN)) return false;
    uint8_t h[4];
    addr_hdr(h, CMD_PP, addr);
    return cmd(h, 4, buf, NULL, (uint16_t)len);
}

bool nor_erase4k(uint32_t addr)
{
    if (!s_ok || addr >= NOR_SIZE || (addr % NOR_SECTOR) != 0u) return false;
    if (nor_busy() || !cmd1(CMD_WREN)) return false;
    uint8_t h[4];
    addr_hdr(h, CMD_SE4K, addr);
    return cmd(h, 4, NULL, NULL, 0);
}
