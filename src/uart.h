#ifndef _UART_H_
#define _UART_H_

#include <stdint.h>

void Uart_Init();
void Uart_SendER(int16_t er, uint8_t flag);
void Uart_SendStr(const char *s);

#endif
