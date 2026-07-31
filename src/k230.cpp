#include "k230.h"
#include <Arduino.h>

#define K230_BUF_SIZE    64

static char s_buf[K230_BUF_SIZE];
static uint8_t s_idx = 0;
static int16_t s_ballX = K230_LOST;

static uint32_t s_lastRecv = 0;

void K230_Init(uint32_t baud) {
    Serial1.begin(baud, SERIAL_8N1, 9, 8);
    s_idx = 0;
    s_ballX = K230_LOST;
    Serial.printf("[K230] Init @%u baud, RX=9\n", baud);
}

static int16_t parse_x(const char *buf) {
    if (buf[0] == 'N' || buf[0] == 'n') {
        return K230_LOST;
    }
    return (int16_t)atoi(buf);
}

void K230_Process() {
    while (Serial1.available()) {
        char c = (char)Serial1.read();

        if (c == '\n' || c == '\r') {
            if (s_idx > 0) {
                s_buf[s_idx] = '\0';
                s_ballX = parse_x(s_buf);
                s_lastRecv = millis();
                s_idx = 0;

                if (s_ballX == K230_LOST) {
                    Serial.println("[K230] LOST");
                } else {
                    Serial.printf("[K230] X=%d\n", s_ballX);
                }
            }
        }
        else if (s_idx < K230_BUF_SIZE - 1) {
            s_buf[s_idx++] = c;
        }
    }

    if (s_lastRecv != 0 && millis() - s_lastRecv >= 1000) {
        s_lastRecv = millis();
        Serial.println("[K230] WARN: no data for 1s");
    }
}

int16_t K230_GetX() {
    return s_ballX;
}
