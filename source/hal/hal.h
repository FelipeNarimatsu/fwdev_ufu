#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdalign.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

#define HAL_DEBUG_IN_SLEEP_MODE 1
#define HAL_WDG_ENABLED 0

#if defined(__GNUC__)
#define __WEAK __attribute__((weak))
#define __UNUSED __attribute__((unused))
#else
#define __WEAK
#define __UNUSED
#endif

#include "utl_printf.h"
#include "utl_dbg.h"
#include "hal_cpu.h"
#include "hal_uart.h"

extern hal_cpu_driver_t HAL_CPU_DRIVER;
extern hal_uart_driver_t HAL_UART_DRIVER;

void hal_init(void);
void hal_deinit(void);

#ifdef __cplusplus
}
#endif
