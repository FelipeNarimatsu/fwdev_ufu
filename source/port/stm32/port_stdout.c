#include <stdio.h>
#include "main.h"
#include "stm32l4xx_ll_usart.h"

int __io_putchar(int ch)
{
    ITM_SendChar(ch);

    return 1;
}
