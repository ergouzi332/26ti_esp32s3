#include "gray.h"
#include <Arduino.h>

const uint8_t ADC_PIN[8] = {6, 7, 8, 3, 9, 10, 1, 2};
const int8_t  W[8]       = {-7, -5, -3, -1, +1, +3, +5, +7};

volatile int16_t  g_er       = 0;
volatile uint8_t  g_gray     = 0;
volatile bool     g_lost     = false;
volatile bool     g_allBlack = false;

#define FILTER_ALPHA 0.7f

void Gray_Init() {
    analogReadResolution(12);
}

void Gray_Sample() {
    uint16_t raw[8];
    uint16_t hi = 0, lo = 4095;
    for (int i = 0; i < 8; i++) {
        raw[i] = analogRead(ADC_PIN[i]);
        if (raw[i] > hi) hi = raw[i];
        if (raw[i] < lo) lo = raw[i];
    }
    uint16_t span = hi - lo;
    uint8_t mask  = 0;

    if (span < 300) {
        if (hi > 3500) {
            g_lost = true; g_allBlack = false; g_er = 0; g_gray = 0xFF;
            return;
        } else if (lo < 500) {
            g_allBlack = true; g_lost = false; g_er = 0; g_gray = 0x00;
            return;
        } else {
            g_gray = 0; return;
        }
    }
    g_lost = false; g_allBlack = false;

    uint16_t thr = (hi + lo) / 2;
    int32_t sumW = 0, sumV = 0;
    for (int i = 0; i < 8; i++) {
        if (raw[i] < thr) {
            uint16_t deep = thr - raw[i];
            sumW += W[i] * deep;
            sumV += deep;
            mask |= (1 << (7 - i));
        }
    }
    if (sumV > 0) {
        int16_t erRaw = (int16_t)(sumW / sumV);
        g_er = (int16_t)(g_er * (1.0f - FILTER_ALPHA) + erRaw * FILTER_ALPHA);
    } else {
        g_er = 0;
    }
    g_gray = mask;
}
