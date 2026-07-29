#include "gray.h"
#include <Arduino.h>

const int8_t W[8] = {-7, -5, -3, -1, +1, +3, +5, +7};

volatile int16_t  g_er       = 0;
volatile uint8_t  g_gray     = 0;
volatile bool     g_lost     = false;
volatile bool     g_allBlack = false;

static const uint8_t ADDR_PIN[3] = {PIN_GRAY_AD0, PIN_GRAY_AD1, PIN_GRAY_AD2};

void Gray_Init() {
    pinMode(PIN_GRAY_OUT, INPUT);
    for (int i = 0; i < 3; i++) {
        pinMode(ADDR_PIN[i], OUTPUT);
        digitalWrite(ADDR_PIN[i], LOW);
    }
}

// 选择通道: 通过AD0/1/2设置地址
static void select_channel(uint8_t ch) {
    digitalWrite(ADDR_PIN[0], (ch & 1) ? HIGH : LOW);
    digitalWrite(ADDR_PIN[1], (ch & 2) ? HIGH : LOW);
    digitalWrite(ADDR_PIN[2], (ch & 4) ? HIGH : LOW);
    delayMicroseconds(50);  // 等待多路开关稳定
}

// 0=黑线(低电平), 1=白纸(高电平)
static uint8_t read_channel(uint8_t ch) {
    select_channel(ch);
    return (uint8_t)digitalRead(PIN_GRAY_OUT);
}

void Gray_Sample() {
    uint8_t bits = 0;
    uint8_t black_cnt = 0;
    int32_t sumW = 0;

    for (int i = 0; i < 8; i++) {
        uint8_t val = read_channel(i);  // 1=白, 0=黑
        if (val == 0) {  // 黑线
            bits |= (1 << (7 - i));  // 高位=左
            sumW += W[i];
            black_cnt++;
        }
    }

    g_gray = bits;

    // 全白 = 丢线
    if (black_cnt == 0) {
        g_lost = true;
        g_allBlack = false;
        g_er = 0;
        return;
    }

    // 全黑 = 转弯触发
    if (black_cnt == 8) {
        g_allBlack = true;
        g_lost = false;
        g_er = 0;
        return;
    }

    g_lost = false;
    g_allBlack = false;

    // 直接累加权重（匹配旧TI稳定版：w[i]直接加，不取反）
    int16_t erRaw = (int16_t)sumW;
    g_er = (int16_t)(g_er * 0.3f + erRaw * 0.7f);
}
