#ifndef __DEBUG_H
#define __DEBUG_H

#include "common.h"
#include <stdio.h>

void DEBUG_Init();
void DEBUG_SendHex(u8 hex);
int _write(int fd, char *ptr, int len);

#endif