#include "common.h"
#include "debug.h"
#include "midi.h"
#include "diskio.h"
#include "ff.h"

int main (void) {

	FATFS fs;
	DEBUG_Init();
	DEBUG_SendHex('a');
	printf("start\r\n");
	printf("init%02X\r\n", disk_initialize(DEV_MMC));
	printf("mount%02X\r\n", f_mount(&fs, "0:", 1));
	if (MID_Init())
	{
		printf("File open error.\r\n");
		while (1)
		{
		}
	}
	MIDI_Play();
  	printf("Play done.\r\n");

	while(1)
	{
	}
}
