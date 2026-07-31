#include <Arduino.h>
#include "oled.h"
#include "uart.h"
#include "k230.h"
#include "stepper.h"
#include "ball.h"
#include "web.h"

volatile uint8_t  g_stop     = 1;
volatile uint8_t  g_timerRun = 0;
volatile uint8_t  g_ballCmd  = 0;
volatile uint16_t g_runMs    = 0;
unsigned long     _t0       = 0;
volatile int16_t  g_ballX   = K230_LOST;
volatile uint8_t  g_ampCmd  = 0;
volatile uint8_t  g_webCmd  = 0;
volatile uint8_t  g_lastCmd = 0;

TaskHandle_t thTi    = NULL;
TaskHandle_t thK230  = NULL;
TaskHandle_t thBall  = NULL;
TaskHandle_t thOled  = NULL;

void taskTi(void *pv) {
    TickType_t last = xTaskGetTickCount();
    while (1) {
        uint8_t cmd = 0;

        if (g_webCmd) {
            cmd = g_webCmd;
            g_webCmd = 0;
        } else if (Serial2.available()) {
            cmd = (uint8_t)Serial2.read();
        } else if (Serial.available()) {
            uint8_t c = (uint8_t)Serial.read();
            if (c == '1')      cmd = 0x01;
            else if (c == '2') cmd = 0x02;
            else if (c == '4') cmd = 0x04;
#if STEPPER_TEST_MODE
            else if (c >= '3' && c <= '8') g_ampCmd = c;
#endif
        }

        if (cmd == 0x01) {
            _t0 = millis();
            g_timerRun = 1;
            g_stop = 1;
            Web_Logf("[TI] LINE START");
        } else if (cmd == 0x04) {
            g_stop = 0;
            g_ballCmd = 1;
            Web_Logf("[TI] BALL START (Q3)");
        } else if (cmd == 0x05) {
            _t0 = millis();
            g_timerRun = 1;
            g_stop = 0;
            g_ballCmd = 2;
            Web_Logf("[TI] Q4 START");
        } else if (cmd == 0x06) {
            g_stop = 1;
            g_timerRun = 0;
            Web_Logf("[TI] Q4 DONE (dist) t=%ums", (unsigned)(millis() - _t0));
        } else if (cmd == 0x02) {
            g_stop = 1;
            g_timerRun = 0;
            Web_Logf("[TI] LINE DONE");
        } else if (cmd == 0x03) {
            g_stop = 1;
            g_timerRun = 0;
            Web_Logf("[TI] STOP");
        }
        if (cmd) g_lastCmd = cmd;
        if (g_timerRun) {
            g_runMs = (uint16_t)(millis() - _t0);
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(10));
    }
}

void taskK230(void *pv) {
    K230_Init(115200);
    while (1) {
        K230_Process();
        g_ballX = K230_GetX();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void taskBall(void *pv) {
    TickType_t last = xTaskGetTickCount();

#if STEPPER_TEST_MODE

    const uint32_t AUTO_INTERVAL_MS = 5000;
    const int32_t RECOVER_STEPS = 300;
    const uint32_t STUCK_PIX = 330;
    const uint32_t STUCK_MS = 2000;
    int32_t g_amp = 150;
    Stepper_Enable(true);
    Serial.println("[STEP] AUTO TEST: 3=100 4=150 5=200 6=250 7=300 8=400 steps | toggle every 5s");
    int32_t curTarget = 0;
    uint32_t lastSwitch = millis();
    uint32_t stuckT0 = 0;
    bool recovering = false;
    uint32_t recoverT0 = 0;
    int32_t recoverTgt = 0;
    int16_t lastBallX = 0;
    uint8_t idleCnt = 0;
    const int32_t AMP_TABLE[] = {100, 150, 200, 250, 300, 400};
    const uint8_t AMP_TABLE_LEN = 6;
    uint8_t ampIdx = 1;
    uint8_t toggleCnt = 0;
    while (1) {
        if (g_ampCmd) {
            uint8_t a = g_ampCmd;
            g_ampCmd = 0;
            g_amp = (a == 8) ? 400 : (int32_t)(100 + (a - 3) * 50);
            ampIdx = (a == 8) ? 5 : (uint8_t)(a - 3);
            Serial.printf("[STEP] amp -> %d\n", (int)g_amp);
        }

        bool nearEnd = (g_ballX == K230_LOST) ||
                       ((int32_t)g_ballX - X_CENTER > (int32_t)STUCK_PIX) ||
                       (X_CENTER - (int32_t)g_ballX > (int32_t)STUCK_PIX);
        bool idleStuck = (idleCnt >= 6);
        bool ballCentered = (g_ballX != K230_LOST) &&
                            (abs((int)g_ballX - X_CENTER) <= 100);

        if (!recovering) {
            if (nearEnd || idleStuck) {
                if (stuckT0 == 0) stuckT0 = millis();
                if (idleStuck || millis() - stuckT0 >= STUCK_MS) {
                    recovering = true;
                    recoverT0 = millis();
                    if (g_ballX == K230_LOST) {
                        recoverTgt = (curTarget <= 0) ? RECOVER_STEPS : -RECOVER_STEPS;
                    } else if (g_ballX > X_CENTER) {
                        recoverTgt = -RECOVER_STEPS;
                    } else {
                        recoverTgt = RECOVER_STEPS;
                    }
                    curTarget = recoverTgt;
                    Serial.printf("[STEP] STUCK -> recover %d\n", (int)recoverTgt);
                }
            } else {
                stuckT0 = 0;
            }

            if (millis() - lastSwitch >= AUTO_INTERVAL_MS) {
                lastSwitch = millis();

                toggleCnt++;
                if (toggleCnt >= 6) {
                    toggleCnt = 0;
                    ampIdx = (uint8_t)((ampIdx + 1) % AMP_TABLE_LEN);
                    g_amp = AMP_TABLE[ampIdx];
                    Serial.printf("[STEP] amp auto -> %d\n", (int)g_amp);
                }
                curTarget = (curTarget <= 0) ? g_amp : -g_amp;
                Stepper_SetTarget(curTarget);
                Serial.printf("[STEP] auto target=%d\n", (int)curTarget);
            }
        } else {

            uint32_t t = millis() - recoverT0;
            Stepper_SetTarget(((t / 1500) % 2 == 0) ? recoverTgt : 0);
            if (ballCentered) {
                recovering = false;
                stuckT0 = 0;
                idleCnt = 0;
                curTarget = 0;
                Serial.println("[STEP] ball back -> resume");
            }
        }
        Stepper_Update();
        static uint32_t lastPrint = 0;
        if (millis() - lastPrint >= 500) {
            lastPrint = millis();

            if (g_ballX != K230_LOST &&
                abs((int)g_ballX - (int)lastBallX) <= 10 &&
                abs((int)g_ballX - X_CENTER) > 100) {
                idleCnt++;
            } else {
                idleCnt = 0;
            }
            lastBallX = g_ballX;
            if (g_ballX == K230_LOST) {
                Serial.printf("[STEP] pos=%d target=%d ball=LOST\n",
                              (int)Stepper_GetSteps(), (int)curTarget);
            } else {
                Serial.printf("[STEP] pos=%d target=%d ball=%d(%.1fcm)\n",
                              (int)Stepper_GetSteps(), (int)curTarget, g_ballX, X_CM(g_ballX));
            }
        }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
    }
#else

    Ball_Init();
    uint8_t lastStop = 1;
#if BALL_AUTO_START
    Web_Logf("[BALL] auto start in 5s");
    vTaskDelay(pdMS_TO_TICKS(5000));
    Ball_Start();
#endif
    while (1) {
        if (g_ballCmd) {
            uint8_t bc = g_ballCmd;
            g_ballCmd = 0;
            lastStop = g_stop;
            if (bc == 1) Ball_Start();
            else if (bc == 2) Ball_StartQ4();
        }

        if (g_stop != lastStop) {
            lastStop = g_stop;
            if (!g_stop) {
                Ball_Start();
            } else {
                Ball_Stop();
            }
        }
        Ball_Update(g_ballX);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
    }
#endif
}

void taskOled(void *pv) {
    TickType_t last = xTaskGetTickCount();
    while (1) {
        Oled_Update();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("===== 26TI ESP32 Ball Balance =====");

    Uart_Init();
    Stepper_Init();
    Oled_Init();
    Web_Init();

    _t0 = 0;
    g_stop = 1;

    xTaskCreatePinnedToCore(taskTi,   "TI",   2048, NULL, 3, &thTi,   0);
    xTaskCreatePinnedToCore(taskK230, "K230", 2048, NULL, 2, &thK230, 0);
    xTaskCreatePinnedToCore(taskBall, "Ball", 4096, NULL, 2, &thBall, 1);
    xTaskCreatePinnedToCore(taskOled, "Oled", 2048, NULL, 1, &thOled, 1);

    Web_Logf("[SYS] All tasks started");
    vTaskDelete(NULL);
}

void loop() {}
