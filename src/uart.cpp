#include "uart.h"
#include <Arduino.h>

#define PIN_TI_RX   4
#define PIN_TI_TX   5
#define TI_BAUD     115200

void Uart_Init() {
    Serial2.begin(TI_BAUD, SERIAL_8N1, PIN_TI_RX, PIN_TI_TX);
}

void Uart_SendER(int16_t er, uint8_t flag) {
    uint8_t erH = (uint8_t)(er >> 8);
    uint8_t erL = (uint8_t)(er & 0xFF);
    uint8_t sum = 0x04 ^ erH ^ erL ^ flag;
    uint8_t buf[7] = {0xAA, 0x55, 0x04, erH, erL, flag, sum};
    Serial2.write(buf, 7);
}

// 调试用字符串发送
void Uart_SendStr(const char *s) {
    Serial2.print(s);
}
