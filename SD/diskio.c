/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"		/* Obtains integer types */
#include "diskio.h" /* Declarations of disk functions */
#include "SD.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"

/* Definitions of physical drive number for each drive */
// #define DEV_RAM		0	/* Example: Map Ramdisk to physical drive 0 */
// #define DEV_MMC		1	/* Example: Map MMC/SD card to physical drive 1 */
// #define DEV_USB		2	/* Example: Map USB MSD to physical drive 2 */

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status(
	BYTE pdrv /* Physical drive nmuber to identify the drive */
)
{
	if (pdrv == DEV_MMC)
	{
		if (SD_Info.SD_Type == UNINITIALIZED)
		{
			return STA_NODISK;
		}
		else
		{
			return RES_OK;
		}
	}
	return STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize(
	BYTE pdrv /* Physical drive nmuber to identify the drive */
)
{
	if (pdrv == DEV_MMC)
	{
		GPIO_InitTypeDef GPIO_InitStructure;
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
		GPIO_Init(GPIOB, &GPIO_InitStructure);
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
		GPIO_Init(GPIOB, &GPIO_InitStructure);
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_Init(GPIOB, &GPIO_InitStructure);
		//初始化SPI2为低速模式
		SPI_InitTypeDef SPI_InitStructure;
		SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
		SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
		SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
		SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;   //时钟悬空低
		SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge; //第一个时钟沿
		SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
		SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256; // 72MHz/256=281.25KHz SD卡初始化时需要低速
		SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
		SPI_Init(SPI2, &SPI_InitStructure);
		SPI_Cmd(SPI2, ENABLE);
		//初始化SD卡
		if (!SD_InitCard())
		{
			//成功
			SPI_Cmd(SPI2, DISABLE);
			SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
			SPI_Init(SPI2, &SPI_InitStructure);
			SPI_Cmd(SPI2, ENABLE);
			return RES_OK;
		}
		else
		{
			//失败 一般为没有卡
			SPI_Cmd(SPI2, DISABLE);
			return STA_NODISK;
		}
	}
	return STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read(
	BYTE pdrv,	/* Physical drive nmuber to identify the drive */
	BYTE *buff,   /* Data buffer to store read data */
	DWORD sector, /* Start sector in LBA */
	UINT count	/* Number of sectors to read */
)
{
	if (count == 0)
	{
		return RES_PARERR;
	}
	if (pdrv == DEV_MMC)
	{
		if (disk_status(pdrv) != RES_OK)
			return RES_NOTRDY;
		u8 result;
		SD_CS_CLR;
		if (count == 1)
			result = SD_ReadBlock(buff, sector) ? RES_OK : RES_ERROR;
		else
			result = SD_ReadBlocks(buff, sector, count) ? RES_OK : RES_ERROR;
		SD_CS_SET;
		return result;
	}
	return RES_PARERR;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write(
	BYTE pdrv,		  /* Physical drive nmuber to identify the drive */
	const BYTE *buff, /* Data to be written */
	DWORD sector,	 /* Start sector in LBA */
	UINT count		  /* Number of sectors to write */
)
{
	if (pdrv == DEV_MMC && count > 0)
	{
		if (disk_status(pdrv) != RES_OK)
			return RES_NOTRDY;
		u8 result;
		SD_CS_CLR;
		if (count == 1)
		{
			result = SD_WriteBlock(buff, sector) ? RES_OK : RES_ERROR;
		}
		else
		{
			result = SD_WriteBlocks(buff, sector, count) ? RES_OK : RES_ERROR;
		}
		SD_CS_SET;
		return result;
	}
	return RES_PARERR;
}

#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl(
	BYTE pdrv, /* Physical drive nmuber (0..) */
	BYTE cmd,  /* Control code */
	void *buff /* Buffer to send/receive control data */
)
{
	return RES_PARERR;
}
