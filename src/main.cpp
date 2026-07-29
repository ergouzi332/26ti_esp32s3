#include <Arduino.h>
#include "gray.h"
#include "oled.h"
#include "uart.h"
#include "oled.h"

// ============== IR ==============
#define PIN_IR_EN   11

// ============== Global ==============
volatile uint8_t  g_stop    = 1;     // 1=等待START 0=运行
volatile uint16_t g_runMs   = 0;
unsigned long     _t0       = 0;

// ============== RTOS Handles ==============
TaskHandle_t thGray = NULL;
TaskHandle_t thBall = NULL;
TaskHandle_t thOled = NULL;

// ============== Task: Gray (5ms, core 0) ==============
void taskGray(void *pv) {
    TickType_t last = xTaskGetTickCount();
    while (1) {
        Gray_Sample();

        uint8_t flag = (g_lost ? 1:0) | ((g_allBlack?1:0)<<1);
        Uart_SendER(g_er, flag);
        static uint32_t lastPrint = 0;
        if (millis() - lastPrint >= 200) { lastPrint = millis();
            Serial1.printf("ER:%+4d  G:%02X\n", g_er, g_gray);
        }

        uint8_t cmd = Uart_CheckCmd();
        if (cmd == 0x01) { _t0 = millis(); g_stop = 0; }
        else if (cmd == 0x02 || cmd == 0x03) { g_stop = 1; }
        if (!g_stop) { g_runMs = (uint16_t)(millis() - _t0); }
        vTaskDelayUntil(&last, pdMS_TO_TICKS(5));
    }
}

// ============== Task: Ball (10ms, core 1) ==============
void taskBall(void *pv) {
    TickType_t last = xTaskGetTickCount();
    while (1) {
        // TODO: K230 -> Ball PID -> Servo
        vTaskDelayUntil(&last, pdMS_TO_TICKS(10));
    }
}

// ============== Task: OLED (100ms, core 1) ==============
void taskOled(void *pv) {
    TickType_t last = xTaskGetTickCount();
    while (1) {
        Oled_Update();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
    }
}

// ============== Setup ==============
void setup() {
    Serial.begin(115200);
    Serial1.begin(115200, SERIAL_8N1, 9, 8);
    delay(100);
    Serial.println("===== 26TI ESP32 Start =====");

    pinMode(PIN_IR_EN, OUTPUT);
    digitalWrite(PIN_IR_EN, HIGH);
    Serial.println("[IR] ON");

    Gray_Init();
    Uart_Init();
    Oled_Init();

    // 等待TI START命令
    _t0 = 0;
    g_stop = 1;

    xTaskCreatePinnedToCore(taskGray, "Gray", 4096, NULL, 3, &thGray, 0);
    xTaskCreatePinnedToCore(taskBall, "Ball", 4096, NULL, 2, &thBall, 1);
    xTaskCreatePinnedToCore(taskOled, "Oled", 4096, NULL, 1, &thOled, 1);

    vTaskDelete(NULL);
}

void loop() {}
