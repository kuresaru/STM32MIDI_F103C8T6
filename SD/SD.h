#include "stm32f10x.h"

#define SD_CS_SET GPIOB->BSRR = 1 << 12;
#define SD_CS_CLR GPIOB->BRR = 1 << 12;

#define SD_CMD_SINGLE_READ			0x51
#define SD_CMD_MULTI_READ			0x52
#define SD_CMD_STOP					0x4C
#define SD_CMD_SINGLE_WRITE			0x58
#define SD_CMD_MULTI_WRITE			0x59

typedef struct {
	enum SD_Type {
		UNINITIALIZED=0, SD_1, SD_2, SD_HC
	} SD_Type;
} SD_Info_t;

extern SD_Info_t SD_Info;

u8 SD_InitCard(void);
u8 SD_SendNOP(void);
u8 SD_ReadBlock(u8 *buf, u32 sector);
u8 SD_ReadBlocks(u8 *buf, u32 sector, u32 count);
u8 SD_WriteBlock(uc8 *buf, u32 sector);
u8 SD_WriteBlocks(uc8 *buf, u32 sector, u32 count);
