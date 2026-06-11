/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   This file includes a diskio driver skeleton to be completed by the user.
  ******************************************************************************
  */
 /* USER CODE END Header */

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/*
 * Warning: the user section 0 is no more in use (starting from CubeMx version 4.16.0)
 * To be suppressed in the future.
 * Kept to ensure backward compatibility with previous CubeMx versions when
 * migrating projects.
 * User code previously added there should be copied in the new user sections before
 * the section contents can be deleted.
 */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */

#include <string.h>
#include <stdio.h>
#include "ff_gen_drv.h"
#include "main.h"
#include "pins.h"

/* CS control */
#define SD_CS_LOW()  HAL_GPIO_WritePin(SPI_CS_PORT, SPI_CS_PIN, GPIO_PIN_RESET)
#define SD_CS_HIGH() HAL_GPIO_WritePin(SPI_CS_PORT, SPI_CS_PIN, GPIO_PIN_SET)

/* Card type */
#define CT_SDSC 0x01
#define CT_SDHC 0x02

extern SPI_HandleTypeDef hspi1;

static volatile DSTATUS Stat = STA_NOINIT;
static uint8_t CardType = 0;

/* ── SPI helpers ────────────────────────────────────────────── */

static void SPI_SetSpeed(uint32_t prescaler)
{
    while (hspi1.State == HAL_SPI_STATE_BUSY);
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR_Msk, prescaler);
    __HAL_SPI_ENABLE(&hspi1);
}

static uint8_t SPI_TxRx(uint8_t data)
{
    while (!(hspi1.Instance->SR & SPI_SR_TXE));
    *(volatile uint8_t *)&hspi1.Instance->DR = data;

    while (hspi1.Instance->SR & SPI_SR_BSY);  // wait until transaction complete
    while (!(hspi1.Instance->SR & SPI_SR_RXNE));
    return *(volatile uint8_t *)&hspi1.Instance->DR;
}

/* ── SD helpers ─────────────────────────────────────────────── */

static uint8_t SD_WaitReady(void)
{
    uint8_t res;
    uint32_t t = HAL_GetTick();
    do {
        res = SPI_TxRx(0xFF);
        if ((HAL_GetTick() - t) >= 2000)  // increased from 500
        {
            printf("SD_WaitReady: timeout\r\n");
            return 0x00;
        }
    } while (res != 0xFF);
    return res;
}

static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg)
{
    uint8_t crc = 0x01;
    if (cmd == 0) crc = 0x95;
    if (cmd == 8) crc = 0x87;

    if (cmd != 0)
	{
		uint8_t rdy = SD_WaitReady();
		if (rdy != 0xFF)
		{
			printf("SD_SendCmd: card not ready for cmd=%d\r\n", cmd);
			return 0xFF; // return error
		}
	}

    SPI_TxRx(0x40 | cmd);
    SPI_TxRx((arg >> 24) & 0xFF);
    SPI_TxRx((arg >> 16) & 0xFF);
    SPI_TxRx((arg >>  8) & 0xFF);
    SPI_TxRx((arg >>  0) & 0xFF);
    SPI_TxRx(crc);

    if (cmd == 12) SPI_TxRx(0xFF); // skip one byte for CMD12

    uint8_t res = 0xFF;
    for (int i = 0; i < 8; i++)
    {
        res = SPI_TxRx(0xFF);
        if (!(res & 0x80)) break;
    }
    return res;
}

static uint8_t SD_ReadBlock(uint8_t *buf, uint16_t len)
{
    uint8_t token;
    uint32_t t = HAL_GetTick();

    do {
        token = SPI_TxRx(0xFF);
        if ((HAL_GetTick() - t) >= 500)
        {
            printf("SD_ReadBlock: token timeout\r\n");
            return 0;
        }
    } while (token == 0xFF);

    if (token != 0xFE)
    {
        printf("SD_ReadBlock: bad token 0x%02X\r\n", token);
        return 0;
    }

    for (uint16_t i = 0; i < len; i++) buf[i] = SPI_TxRx(0xFF);

    SPI_TxRx(0xFF); // discard CRC
    SPI_TxRx(0xFF);

    return 1;
}

static uint8_t SD_WriteBlock(const uint8_t *buf, uint8_t token)
{
    if (SD_WaitReady() != 0xFF) return 0;

    SPI_TxRx(token);

    if (token == 0xFD) return 1; // stop tran, no data follows

    for (uint16_t i = 0; i < 512; i++) SPI_TxRx(buf[i]);

    SPI_TxRx(0xFF); // dummy CRC
    SPI_TxRx(0xFF);

    uint8_t res = SPI_TxRx(0xFF) & 0x1F;
    if (res != 0x05)
    {
        printf("SD_WriteBlock: rejected 0x%02X\r\n", res);
        return 0;
    }

    SD_WaitReady();
    return 1;
}

/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif /* _USE_IOCTL == 1 */

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if  _USE_WRITE
  USER_write,
#endif  /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize (
	BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
  /* USER CODE BEGIN INIT */
    printf("USER_initialize called\r\n");

    if (pdrv != 0) return STA_NOINIT;

    SPI_SetSpeed(SPI_BAUDRATEPRESCALER_128); // 32MHz/128 = 250kHz for init

    SD_CS_HIGH();
    for (int i = 0; i < 10; i++) SPI_TxRx(0xFF); // 80 dummy clocks

    SD_CS_LOW();

    // CMD0 - reset
    uint8_t res;
    for (int retry = 0; retry < 5; retry++)
    {
        res = SD_SendCmd(0, 0);
        if (res == 0x01) break;
        HAL_Delay(10);
    }
    if (res != 0x01)
    {
        SD_CS_HIGH();
        printf("SD: CMD0 failed (0x%02X)\r\n", res);
        return STA_NOINIT;
    }
    printf("SD: CMD0 OK\r\n");

    // CMD8 - check voltage / detect v2
    res = SD_SendCmd(8, 0x1AA);
    if (res == 0x01)
    {
        uint8_t r7[4];
        for (int i = 0; i < 4; i++) r7[i] = SPI_TxRx(0xFF);
        printf("SD: CMD8 R7=%02X %02X %02X %02X\r\n", r7[0], r7[1], r7[2], r7[3]);

        if (r7[2] == 0x01 && r7[3] == 0xAA)
        {
            // SD v2 - ACMD41 with HCS
            uint32_t t = HAL_GetTick();
            do {
                SD_SendCmd(55, 0);
                res = SD_SendCmd(41, 0x40000000);
            } while (res != 0x00 && (HAL_GetTick() - t) < 2000);

            if (res != 0x00)
            {
                SD_CS_HIGH();
                printf("SD: ACMD41 timeout\r\n");
                return STA_NOINIT;
            }

            // CMD58 - read OCR, check CCS for SDHC
            SD_SendCmd(58, 0);
            uint8_t ocr[4];
            for (int i = 0; i < 4; i++) ocr[i] = SPI_TxRx(0xFF);
            printf("SD: OCR=%02X %02X %02X %02X\r\n", ocr[0], ocr[1], ocr[2], ocr[3]);

            CardType = (ocr[0] & 0x40) ? CT_SDHC : CT_SDSC;
        }
        else
        {
            SD_CS_HIGH();
            printf("SD: CMD8 voltage mismatch\r\n");
            return STA_NOINIT;
        }
    }
    else
    {
        // SD v1 - ACMD41 without HCS
        uint32_t t = HAL_GetTick();
        do {
            SD_SendCmd(55, 0);
            res = SD_SendCmd(41, 0);
        } while (res != 0x00 && (HAL_GetTick() - t) < 2000);

        if (res != 0x00)
        {
            SD_CS_HIGH();
            printf("SD: v1 ACMD41 failed\r\n");
            return STA_NOINIT;
        }
        CardType = CT_SDSC;
    }

    SD_CS_HIGH();
    SPI_TxRx(0xFF);

    SPI_SetSpeed(SPI_BAUDRATEPRESCALER_4); // 32MHz/2 = 16MHz for normal operation

    // Let card settle at new speed
    SD_CS_HIGH();
    for (int i = 0; i < 10; i++) SPI_TxRx(0xFF);
    HAL_Delay(10);

    Stat = 0;
    printf("SD: Init OK! CardType=%s\r\n", CardType == CT_SDHC ? "SDHC" : "SDSC");
    return Stat;
  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (
	BYTE pdrv       /* Physical drive number to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */
    if (pdrv != 0) return STA_NOINIT;
    return Stat;
  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USER_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */
    if (pdrv != 0 || count == 0) return RES_PARERR;
    if (Stat & STA_NOINIT)       return RES_NOTRDY;

    if (CardType != CT_SDHC) sector *= 512; // SDSC byte addressing

    SD_CS_LOW();

    if (count == 1)
    {
        uint8_t cmd_res = SD_SendCmd(17, sector);
        if (cmd_res == 0x00 && SD_ReadBlock(buff, 512))
            count = 0;
    }
    else
    {
        if (SD_SendCmd(18, sector) == 0x00)
        {
            do {
                if (!SD_ReadBlock(buff, 512)) break;
                buff += 512;
            } while (--count);
            SD_SendCmd(12, 0);
        }
    }

    SD_CS_HIGH();
    SPI_TxRx(0xFF);

    if (count) printf("USER_read FAILED\r\n");
    return count ? RES_ERROR : RES_OK;
  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{
  /* USER CODE BEGIN WRITE */
    if (pdrv != 0 || count == 0) return RES_PARERR;
    if (Stat & STA_NOINIT)       return RES_NOTRDY;

    if (CardType != CT_SDHC) sector *= 512;

    SD_CS_LOW();

    if (count == 1)
    {
        if (SD_SendCmd(24, sector) == 0x00 && SD_WriteBlock(buff, 0xFE))
            count = 0;
    }
    else
    {
        SD_SendCmd(55, 0);
        SD_SendCmd(23, count); // ACMD23 pre-erase

        if (SD_SendCmd(25, sector) == 0x00)
        {
            do {
                if (!SD_WriteBlock(buff, 0xFC)) break;
                buff += 512;
            } while (--count);

            SD_WriteBlock(0, 0xFD); // stop tran token
        }
    }

    SD_CS_HIGH();
    SPI_TxRx(0xFF);

    if (count) printf("USER_write FAILED\r\n");
    return count ? RES_ERROR : RES_OK;
  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */
    if (pdrv != 0)         return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    DRESULT res = RES_ERROR;
    SD_CS_LOW();

    switch (cmd)
    {
        case CTRL_SYNC:
            if (SD_WaitReady() == 0xFF) res = RES_OK;
            break;

        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;
            res = RES_OK;
            break;

        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1;
            res = RES_OK;
            break;

        case GET_SECTOR_COUNT:
        {
            uint8_t csd[16];
            if (SD_SendCmd(9, 0) == 0x00 && SD_ReadBlock(csd, 16))
            {
                DWORD sectors = 0;
                if ((csd[0] >> 6) == 1) // CSD v2 (SDHC)
                {
                    uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16)
                                    | ((uint32_t)csd[8] << 8)
                                    | csd[9];
                    sectors = (c_size + 1) << 10;
                }
                else // CSD v1
                {
                    uint32_t c_size  = ((uint32_t)(csd[6] & 0x03) << 10)
                                     | ((uint32_t)csd[7] << 2)
                                     | ((csd[8] >> 6) & 0x03);
                    uint32_t c_mult  = 1 << ((((csd[9] & 0x03) << 1)
                                     | ((csd[10] >> 7) & 0x01)) + 2);
                    uint32_t blk_len = 1 << (csd[5] & 0x0F);
                    sectors = (c_size + 1) * c_mult * blk_len / 512;
                }
                *(DWORD*)buff = sectors;
                printf("SD: sector count=%lu\r\n", (unsigned long)sectors);
                res = RES_OK;
            }
            break;
        }

        default:
            res = RES_PARERR;
    }

    SD_CS_HIGH();
    SPI_TxRx(0xFF);

    return res;
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */

