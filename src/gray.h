#ifndef _GRAY_H_
#define _GRAY_H_

#include <stdint.h>

// 数字灰度引脚 (复用型: OUT + AD0/1/2)
#define PIN_GRAY_OUT   6
#define PIN_GRAY_AD0   7
#define PIN_GRAY_AD1   15
#define PIN_GRAY_AD2   16

extern const int8_t W[8];
extern volatile int16_t  g_er;
extern volatile uint8_t  g_gray;
extern volatile bool     g_lost;
extern volatile bool     g_allBlack;

void Gray_Init();
void Gray_Sample();

#endif
