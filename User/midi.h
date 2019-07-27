#ifndef __MIDI_H
#define __MIDI_H
#include "common.h"

typedef struct
{
    u32 ChunkName;  //一定是MThd 0x6468544D
    u32 Length;     //一般是6
    u16 Format;     /*一般是1
                        00 00单音轨
                        00 01多音轨，且同步。这是最常见的
                        00 02多音轨，但不同步*/
    u16 TrackCount; //实际音轨数加上一个全局的音轨
    u16 Time;       /*类型1：定义一个四分音符的tick数，tick是MIDI中的最小时间单位
                        类型2：定义每秒中SMTPE帧的数量及每个SMTPE帧的tick*/
} __attribute__((packed)) MIDI_HeadTypeDef;
typedef struct
{
    u32 TrackName;//一定是MTrk
    u32 Length;//音轨数据长度
    u32 StartPos;//音轨数据起始位置
} __attribute__((packed)) MIDI_TrackTypeDef;

u8 MID_Init();
u8 MIDI_Play();

#endif