#include "common.h"
#include "SD.h"

#define SPIBUSY SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) != RESET

#define WAIT_COUNT 200
#define WAIT_COUNT_HIGH 0xFFFF

SD_Info_t SD_Info;

//发送接收数据
u8 SD_RW(u8 dat)
{
	SPI_I2S_SendData(SPI2, dat);
	while (SPIBUSY)
		;
	return SPI_I2S_ReceiveData(SPI2) & 0xFF;
}

//发送空数据 返回接收数据
u8 SD_SendNOP()
{
	return SD_RW(0xFF);
}

//发送指令
u8 SD_SendData(u8 cmd, u32 arg, u8 crc)
{
	u8 tmp, wait = WAIT_COUNT;
	SD_SendNOP();
	SD_RW(cmd);
	SD_RW((arg >> 24) & 0xFF);
	SD_RW((arg >> 16) & 0xFF);
	SD_RW((arg >> 8) & 0xFF);
	SD_RW(arg & 0xFF);
	SD_RW(crc);
	do
	{
		tmp = SD_SendNOP();
	} while (tmp == 0xFF && wait--);
	return tmp;
}

//初始化卡 成功返回0
u8 SD_InitCard()
{
	u8 tmp, wait;
	SD_CS_CLR;
	Delay_ms(1);
	SD_CS_SET;
	Delay_ms(1);
	for (tmp = 0; tmp < 16; tmp++)
	{ //初始化时钟
		while (SPIBUSY)
			;
		SPI_I2S_SendData(SPI2, 0xFF);
	}
	Delay_ms(1);
	SD_CS_CLR;
	tmp = SD_SendData(0x40, 0x00000000, 0x95); //CMD0
	if (tmp != 0x01)
	{
		SD_CS_SET;
		return 1;
	}
	tmp = SD_SendData(0x48, 0x000001AA, 0x87); //CMD8
	if (tmp == 0x01)
	{
		//SD2.0
		SD_Info.SD_Type = SD_2;
		SD_SendNOP(); //接收完CMD8的4个字节
		SD_SendNOP();
		SD_SendNOP();
		SD_SendNOP();
		wait = WAIT_COUNT;
		do
		{
			Delay_ms(1);
			SD_SendData(0x77, 0x00000000, 0xFF);	   //CMD55
			tmp = SD_SendData(0x69, 0x40000000, 0xFF); //ACMD41
		} while (tmp != 0x00 && wait--);
		if (tmp == 0x00)
		{
			SD_SendData(0x7A, 0x00000000, 0xFF); //CMD58
			tmp = SD_SendNOP();
			SD_SendNOP();
			SD_SendNOP();
			SD_SendNOP();
			if (tmp & 0x40)
			{
				SD_Info.SD_Type = SD_HC;
			}
		}
		else
		{
			SD_CS_SET;
			return 1;
		}
	}
	else if (tmp == 0x05)
	{
		//SD1.0
		SD_Info.SD_Type = SD_1;
	}
	else
	{
		SD_CS_SET;
		return 1;
	}
	SD_CS_SET;

	//Delay_ms(2);
	return 0;
}

//开始读扇区 成功返回1
u8 SD_BlockStart(u32 sector, u8 mode)
{
	u8 tmp;
	u16 wait = WAIT_COUNT_HIGH;
	if (SD_Info.SD_Type != SD_HC)
	{ //非HC卡 扇区号转字节号
		sector <<= 9;
	}
	do
	{
		tmp = SD_SendData(mode, sector, 0xFF);
	} while (tmp && wait--);
	if (tmp == 0)
	{
		return 1;
	}
	return 0;
}

//等待接收起始令牌0xFE
u8 SD_WaitRead()
{
	u8 tmp;
	u16 wait = WAIT_COUNT_HIGH;
	do
	{
		tmp = SD_SendNOP();
	} while (tmp != 0xFE && wait--);
	if (wait)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

//读单扇区 成功返回1
u8 SD_ReadBlock(u8 *buf, u32 sector)
{
	u16 i;
	if (SD_BlockStart(sector, SD_CMD_SINGLE_READ) && SD_WaitRead())
	{
		for (i = 0; i < 512; i++)
		{
			*(buf + i) = SD_SendNOP();
		}
		//丢弃两个字节的CRC
		SD_SendNOP();
		SD_SendNOP();
		return 1;
	}
	return 0;
}

//读多扇区 成功返回1
u8 SD_ReadBlocks(u8 *buf, u32 sector, u32 count)
{
	u16 i;
	u32 j;
	u8 res = 1;
	if (SD_BlockStart(sector, SD_CMD_MULTI_READ))
	{
		for (j = 0; j < count; j++)
		{
			if (SD_WaitRead())
			{
				for (i = 0; i < 512; i++)
				{
					*(buf + (j * 512 + i)) = SD_SendNOP();
				}
				//丢弃两个字节的CRC
				SD_SendNOP();
				SD_SendNOP();
			}
			else
			{
				res = 0;
				break;
			}
		}
		SD_BlockStart(0, SD_CMD_STOP);
	}
	else
	{
		res = 0;
	}
	return res;
}
//写单扇区 成功返回1
u8 SD_WriteBlock(uc8 *buf, u32 sector)
{
	u16 i;
	if (SD_BlockStart(sector, SD_CMD_SINGLE_WRITE))
	{
		for (i = 0; i < 2; i++)
			SD_SendNOP();
		SD_RW(0xFE);
		for (i = 0; i < 512; i++)
		{
			SD_RW(*(buf + i));
		}
		//两个字节的CRC
		SD_SendNOP();
		SD_SendNOP();
		u8 tmp;
		tmp = SD_SendNOP();
		if ((tmp & 0x1F) != 0x05)
			return 0;//Error
		do
		{
			tmp = SD_SendNOP();
		} while (tmp != 0xFF);
		return 1;
	}
	return 0;
}
//写多扇区 成功返回1
u8 SD_WriteBlocks(uc8 *buf, u32 sector, u32 count)
{
	u8 tmp, res, err = 0;
	u16 i;
	u32 j;
	if (SD_BlockStart(sector, SD_CMD_MULTI_WRITE))
	{
		for (i = 0; i < 2; i++)
			SD_SendNOP();
		for (j = 0; j < count; j++)
		{
			SD_RW(0xFC);
			for (i = 0; i < 512; i++)
				SD_RW(*(buf + i));
			SD_SendNOP();
			SD_SendNOP();
			tmp = SD_SendNOP();
			if ((tmp & 0x1F) != 0x05)
			{
				err = 0;//Data not accepted
			}
			do
			{
				tmp = SD_SendNOP();
			} while (tmp != 0xFF);
			if (err)
				break;
		}
		SD_RW(0xFD);//Stop
		SD_SendNOP();
		do
		{
			tmp = SD_SendNOP();
		} while (tmp != 0xFF);
		res = 1;
	}
	else
	{
		res = 0;
	}
	return res;
}
