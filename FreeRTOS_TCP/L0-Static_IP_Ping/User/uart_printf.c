
#include "stm32f4xx_hal.h"
#include "usart.h"
#include "stdio.h"

/* redirect printf to uart */
int __io_putchar(int ch) {
  huart6.Instance->DR = (ch);
  while (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_TC) == 0);
  return ch;
}

void UART_Printf_Init()
    {
    /* io buffer off*/
    /* redirect printf to uart */
    setvbuf(stdout, NULL, _IONBF, 0);
    }
