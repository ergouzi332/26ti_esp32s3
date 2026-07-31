#include "stepper.h"
#include <Arduino.h>
#include "esp32-hal-timer.h"
#include "web.h"


// ====================================================================
// 硬件定时器脉冲发生器
// --------------------------------------------------------------------
// 之前用任务循环每 20ms 只发 2 个脉冲(≈100pps)，电机几乎转不动。
// 现在由硬件定时器 ISR 以 STEP_HZ 频率自动发脉冲，直到到达目标位置。
// 驱动板(ZDT X42S)内部自带编码器闭环，角度精确、不丢步、能锁轴，
// ESP32 只需把它当"角度伺服"用：给目标脉冲数即可，无需读编码器。
// ====================================================================

static hw_timer_t *s_timer = NULL;

static volatile int32_t s_steps  = 0;    // 当前脉冲数（带方向）
static volatile int32_t s_target = 0;    // 目标脉冲数
static volatile bool    s_en     = false; // 使能标志（同时控制 EN 引脚）

// 直写 GPIO 寄存器：比 digitalWrite 快且 ISR 内安全（仅适用 GPIO0-31）
#define GPIO_BIT(pin)  (1u << (pin))
static inline void pin_high(uint8_t pin) { GPIO.out_w1ts = GPIO_BIT(pin); }
static inline void pin_low (uint8_t pin) { GPIO.out_w1tc = GPIO_BIT(pin); }

void IRAM_ATTR Stepper_TimerISR() {
    if (!s_en) return;

    int32_t cur = s_steps;
    int32_t tgt = s_target;
    if (cur == tgt) return;

    // 先定方向再发脉冲
    if (tgt > cur) { pin_high(PIN_DIR); s_steps = cur + 1; }
    else           { pin_low (PIN_DIR); s_steps = cur - 1; }

    pin_high(PIN_STP);
    delayMicroseconds(10);      // 脉冲高电平宽度(驱动板要求 >2.5us)
    pin_low (PIN_STP);
}

void Stepper_Init() {
    pinMode(PIN_STP, OUTPUT);
    pinMode(PIN_DIR, OUTPUT);
    pinMode(PIN_EN,  OUTPUT);
    pinMode(PIN_COM, OUTPUT);

    pin_low(PIN_STP);
    pin_low(PIN_DIR);
    digitalWrite(PIN_EN, HIGH);    // 默认失能（低电平使能）
    digitalWrite(PIN_COM, HIGH);   // 公共端电平按实际接线确定

    s_steps  = 0;
    s_target = 0;
    s_en     = false;

    // 定时器：80MHz / 80 = 1MHz 计数，每 1e6/STEP_HZ 微秒一次中断
    s_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(s_timer, &Stepper_TimerISR, true);
    timerAlarmWrite(s_timer, 1000000UL / STEP_HZ, true);
    timerAlarmEnable(s_timer);
    timerStart(s_timer);

    Web_Logf("[STEPPER] Init OK: %d pps, STP=%d DIR=%d EN=%d\n",
                  (int)STEP_HZ, PIN_STP, PIN_DIR, PIN_EN);
}

void Stepper_Enable(bool en) {
    s_en = en;
    digitalWrite(PIN_EN, en ? LOW : HIGH);   // 低电平使能
}

int32_t Stepper_GetTarget() {
    return (int32_t)s_target;
}

int32_t Stepper_GetSteps() {
    return (int32_t)s_steps;
}

void Stepper_SetTarget(int32_t t) {
    if (t < STEP_MIN) t = STEP_MIN;
    if (t > STEP_MAX) t = STEP_MAX;
    s_target = t;
}

void Stepper_Update() {
    // 硬件定时器模式无需轮询，保留空函数兼容旧调用
}

bool Stepper_AtTarget() {
    return s_steps == s_target;
}