#ifndef __PRINT_H
#define __PRINT_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "cmsis_os2.h"
#include "usart.h"       // °üº¬ huart1 ÉùÃ÷

void my_printf_init(void);
int my_printf(const char *fmt, ...);


#endif
