#include "oled.h"
#include "k230.h"
#include <U8g2lib.h>
#include <Wire.h>
#include <stdio.h>

#define PIN_OLED_SDA  17
#define PIN_OLED_SCL  18

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

extern volatile uint8_t  g_stop;
extern volatile uint16_t g_runMs;
extern volatile int16_t  g_ballX;

void Oled_Init() {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(30, 20, "26TI ESP32");
    u8g2.drawStr(20, 40, "Ball Balance");
    u8g2.sendBuffer();
    delay(500);
}

void Oled_Update() {
    char buf[24];
    u8g2.clearBuffer();

    u8g2.setFont(u8g2_font_6x10_tf);

    // ?1?: Ball X?? / LOST
    if (g_ballX == K230_LOST) {
        snprintf(buf, sizeof(buf), "Ball: LOST");
    } else {
        snprintf(buf, sizeof(buf), "Ball: %4d", g_ballX);
    }
    u8g2.drawStr(0, 10, buf);

    // ?1???: STOP/RUN
    snprintf(buf, sizeof(buf), "%s", g_stop ? "STOP" : "RUN");
    u8g2.drawStr(80, 10, buf);

    // ????: ????
    u8g2.setFont(u8g2_font_fub20_tf);
    uint16_t t = g_runMs;
    uint16_t sec = t / 1000;
    uint16_t tenth = (t / 100) % 10;
    snprintf(buf, sizeof(buf), "%u.%01u", sec, tenth);
    u8g2.drawStr(8, 50, buf);

    u8g2.sendBuffer();
}
