#ifndef _GRAY_H_
#define _GRAY_H_

#include <stdint.h>

extern const uint8_t ADC_PIN[8];
extern const int8_t  W[8];

extern volatile int16_t  g_er;
extern volatile uint8_t  g_gray;
extern volatile bool     g_lost;
extern volatile bool     g_allBlack;

void Gray_Init();
void Gray_Sample();

#endif
