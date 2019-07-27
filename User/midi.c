#include "stm32f10x_tim.h"
#include "stm32f10x_rcc.h"
#include "midi.h"
#include "ff.h"
#include "debug.h"
#include <math.h>
#include "notes.c"

FIL file;
MIDI_HeadTypeDef MIDI_Head;
MIDI_TrackTypeDef MIDI_Track;
u32 Speed = 0; //每个节拍的时长(us)
UINT br;

//转换32位数据字节序
u32 convert32(u32 data)
{
    u32 tmp = (data & 0x000000FF) << 24;
    tmp |= (data & 0x0000FF00) << 8;
    tmp |= (data & 0x00FF0000) >> 8;
    tmp |= (data & 0xFF000000) >> 24;
    return tmp;
}
//转换16位数据字节序
u16 convert16(u16 data)
{
    u16 tmp;
    tmp = (data & 0x00FF) << 8;
    tmp |= (data & 0xFF00) >> 8;
    return tmp;
}
//读取一个动态长度
u32 getDynamicLength()
{
    u32 tmp = 0;
    u8 data, i;
    for (i = 0; i < 4; i++)
    {
        tmp <<= 8;
        f_read(&file, &data, 1, &br);
        tmp |= data & 0x7F;
        if (!(data & 0x80))
        {
            i = 4; //break
        }
    }
    if (data & 0x80)
    {
        printf("Data out of range.\r\n");
    }
    return tmp;
}
void MIDI_DelayTick(u16 tick)
{
    printf("Delay %d tick\r\n", tick);
    if (tick > 0 && Speed > 0)
    {
        TIM1->CNT = 0;
        TIM1->CR1 |= TIM_CR1_CEN;
        while (tick--)
        {
            while (!(TIM1->SR & TIM_SR_UIF))
            {
            }
            TIM1->SR &= ~TIM_SR_UIF;
        }
        TIM1->CR1 &= ~TIM_CR1_CEN;
    }
}
//分析一个音轨
void MIDI_AnalyzeTrack()
{
    if (MIDI_Track.TrackName != 0x6B72544D)
    {
        printf("Found invalid track.\r\n");
        return;
    }
    while (1)
    {
        Delay_ms(5);
        u32 time = getDynamicLength();
        MIDI_DelayTick(time);
        u8 data, currentCode;
        f_read(&file, &data, 1, &br);
        // printf("%02X %lX %ld \r\n", data, file.fptr - 1, time);
        // if (file.fptr == 0x3FF)
        // {//for debug breakpoint
        //     file.fptr = file.fptr;
        // }
        if (data == 0xFF)
        {
            //元事件
            currentCode = data;
            u8 type, a, b, i;
            u32 length;
            f_read(&file, &type, 1, &br);
            length = getDynamicLength();
            //分析元事件类型
            switch (type)
            {
            case 0x03: //音轨名称
                file.fptr += length;
                break;
            case 0x2F: //音轨结束
                return;
            case 0x51: //指定速度
                Speed = 0;
                for (i = 0; i < length; i++)
                {
                    Speed <<= 8;
                    f_read(&file, &data, 1, &br);
                    Speed |= data;
                }
                printf("%ldus per beat.\r\n", Speed);
                u16 tick2Time = Speed * 2 / MIDI_Head.Time; //半个tick的时间
                TIM1->ARR = tick2Time;                      //设置定时器1的溢出时间就是1个Tick
                printf("2Tick Time %dus\r\n", tick2Time);
                break;
            case 0x58: //节拍
                f_read(&file, &a, 1, &br);
                f_read(&file, &b, 1, &br);
                file.fptr += 2;
                b = pow(2, b);
                printf("Beat: %d/%d\r\n", a, b);
                break;
            default:
                file.fptr += length;
                printf("Unknow meta %02X, Len %ld\r\n", type, length);
                break;
            }
        }
        else if (data <= 0x7F)
        {
            file.fptr -= 1;
            if (currentCode <= 0xBF)
            {
                file.fptr += 2;
            }
            else if (currentCode <= 0xDF)
            {
                file.fptr += 1;
            }
            else if (currentCode <= 0xEF)
            {
                file.fptr += 2;
            }
            else
            {
                printf("Invalid current code %02X\r\n", currentCode);
            }
        }
        else if (data <= 0x8F)
        {
            //松开音符
            currentCode = data;
            u8 note, velocity;
            f_read(&file, &note, 1, &br);
            f_read(&file, &velocity, 1, &br);
            TIM2->CR1 &= ~TIM_CR1_CEN;
            TIM2->CNT = 0;
        }
        else if (data <= 0x9F)
        {
            //按下音符
            currentCode = data;
            // printf("Down, delayTime=%ld, fptr=%lX\r\n", time, file.fptr);
            u8 note, velocity;
            f_read(&file, &note, 1, &br);
            f_read(&file, &velocity, 1, &br);
            note -= 23;
            u16 psc = 1000000 / notes[note];
            printf("Next play %d, psc=%d\r\n", note, psc);
            TIM2->PSC = psc;
            TIM2->CR1 |= TIM_CR1_CEN;
        }
        else if (data <= 0xAF)
        {
            //触后音符
            currentCode = data;
            u8 note, velocity;
            f_read(&file, &note, 1, &br);
            f_read(&file, &velocity, 1, &br);
        }
        else if (data <= 0xBF)
        {
            //控制器变化
            currentCode = data;
            u8 code, arg;
            f_read(&file, &code, 1, &br);
            f_read(&file, &arg, 1, &br);
        }
        else if (data <= 0xCF)
        {
            //改变乐器
            currentCode = data;
            file.fptr += 1;
        }
        else if (data <= 0xDF)
        {
            //通道触动压力
            currentCode = data;
            file.fptr += 1;
        }
        else if (data <= 0xEF)
        {
            //弯音轮变换
            currentCode = data;
            u8 pitchL, pitchH;
            f_read(&file, &pitchL, 1, &br);
            f_read(&file, &pitchH, 1, &br);
        }
        else
        {
            printf("Unknow code %02X\r\n", data);
        }
        if (file.fptr >= file.obj.objsize)
        {
            printf("File end.\r\n");
            break;
        }
    }
}
u8 MID_Init()
{
    u8 tmp;
    //定时器1做Tick延时计时器
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 36 - 1; //2MHz
    TIM_TimeBaseInitStructure.TIM_Period = 0;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);
    //定时器2做蜂鸣器频率发生器
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Prescaler = 0;
    TIM_TimeBaseInitStructure.TIM_Period = 72 - 1;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
	TIM_OCInitStructure.TIM_Pulse = 36;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_Low;
	TIM_OC2Init(TIM2, &TIM_OCInitStructure);
    //打开文件
    tmp = f_open(&file, "0:/music.mid", FA_READ);
    if (tmp != FR_OK)
    {
        return 1;
    }
    //读MIDI头
    f_read(&file, &MIDI_Head, sizeof(MIDI_HeadTypeDef), &br);
    MIDI_Head.Length = convert32(MIDI_Head.Length);
    MIDI_Head.Format = convert16(MIDI_Head.Format);
    MIDI_Head.TrackCount = convert16(MIDI_Head.TrackCount);
    MIDI_Head.Time = convert16(MIDI_Head.Time);
    if (MIDI_Head.ChunkName == 0x6468544D && MIDI_Head.Length == 6)
    {
        printf("Track count %d\r\n", MIDI_Head.TrackCount);
        printf("%dticks per beat.\r\n", MIDI_Head.Time);
        return 0;
    }
    return 1;
}
u8 MIDI_Play()
{
    u8 track;
    for (track = 0; track < MIDI_Head.TrackCount; track++)
    {
        //解析每个音轨  按顺序解析 当成是一个  就算同时解析了也不能同时播放
        f_read(&file, &MIDI_Track.TrackName, 4, &br);
        f_read(&file, &MIDI_Track.Length, 4, &br);
        MIDI_Track.Length = convert32(MIDI_Track.Length);
        MIDI_Track.StartPos = file.fptr;
        // printf("Track length %ld\r\n", MIDI_Track.Length);
        MIDI_AnalyzeTrack();
    }
    return 0;
}