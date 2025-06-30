#include "main.h"
#include "hal.h"

extern RNG_HandleTypeDef hrng;

static void port_cpu_init(void)
{
#if HAL_DEBUG_IN_SLEEP_MODE == 1
	LL_DBGMCU_EnableDBGStopMode();
	LL_DBGMCU_EnableDBGStandbyMode();
#endif
}

static void port_cpu_deinit(void)
{
}

static void port_cpu_reset(void)
{
    NVIC_SystemReset();
}

static void port_cpu_watchdog_refresh(void)
{
#if HAL_WDG_ENABLED == 1
    LL_IWDG_ReloadCounter(IWDG);
#endif
}

static void port_cpu_id_get(uint8_t *id)
{
    uint32_t ID1 = LL_GetUID_Word0();
    uint32_t ID2 = LL_GetUID_Word1();
    uint32_t ID3 = LL_GetUID_Word2();

    id[0] = ID1 >> 24;
    id[1] = ID1 >> 16;
    id[2] = ID1 >> 8;
    id[3] = ID1;

    id[4] = ID2 >> 24;
    id[5] = ID2 >> 16;
    id[6] = ID2 >> 8;
    id[7] = ID2;

    id[8] = ID3 >> 24;
    id[9] = ID3 >> 16;
    id[10] = ID3 >> 8;
    id[11] = ID3;
}

static uint32_t port_cpu_random_seed_get(void)
{
    uint32_t rnd;
#if 0
    LL_RNG_ReadRandData32(RNGx)
    uint32_t ID1 = LL_GetUID_Word0();
    uint32_t ID2 = LL_GetUID_Word1();
    uint32_t ID3 = LL_GetUID_Word2();

    return (ID1^ID2^ID3);
#endif

    HAL_RNG_GenerateRandomNumber(&hrng,&rnd);

    return rnd;
}

// Input level codification:
//   0: disable all interrupts
//   n: interrupt level
//      interrupts with priority >= n will be disabled
//      interrupts with priority <  n will be enabled
//
// Returned level codification:
//   If bit1 is on (bit A), then basepri was used instead of primask (a level was provided)
//   and the level value is shifted by 4 bits to the right.
//
//   If bit1 is off, then primask was used instead of basepri (the same as using __disable_irq() / __enable_irq())
//   and bit 0 (bit B) indicates the last interrupt state (0 enabled, 1 disabled).
//
// Output level codification:
// |         31-8           | 7-4| 3-0|
// |000000000000000000000000|PRIO|00AB|
//
// indicates when basepri was used or not
#define PORT_CPU_BASEPRI_USED  (0x02)
// indicates if interrupts were enabled or disabled before entering critical section
#define PORT_CPU_INT_DISABLED  (0x01)
#define PORT_CPU_INT_ENABLED   (0x00)
// bits 7:4 are used for priority, bits 3:0 are not used
#define PORT_CPU_PRIO_BITS_POS (4)

static uint32_t port_cpu_critsec_enter_imp(uint32_t level)
{
    uint32_t last_level = 0;

    if(level == 0)
    {
    	// PRIMASK:
        // 0 - interrupts enabled,
        // 1 - interrupts disabled
        last_level = __get_PRIMASK() ? PORT_CPU_INT_DISABLED : PORT_CPU_INT_ENABLED;
        __disable_irq();
    }
    else
    {
    	last_level = __get_BASEPRI() | PORT_CPU_BASEPRI_USED;
        __set_BASEPRI(level << PORT_CPU_PRIO_BITS_POS);
    }

    __ISB(); // flush pipeline
    __DSB(); // wait for all memory accesses to complete

    return last_level;
}

static void port_cpu_critsec_leave(uint32_t last_level)
{
    if(last_level & PORT_CPU_BASEPRI_USED)
    {
    	last_level &=  ~(PORT_CPU_BASEPRI_USED);
    	__set_BASEPRI(last_level);
    }
    else
    {
    	if(last_level == PORT_CPU_INT_ENABLED)
    	{
        	// Restoring interrupts as they were enabled before calling
    		__enable_irq();
    	}
    	// else: keep interrupts disabled (as they were before)
    }

	__ISB(); // flush pipeline
	__DSB(); // wait for all memory accesses to complete
}

static uint32_t port_cpu_critsec_enter(hal_cpu_cs_level_t level)
{
    uint32_t prio_translate[] = {0, 5, 10, 15};

    return port_cpu_critsec_enter_imp(prio_translate[level]);
}

static hal_cpu_state_t port_cpu_state_get(void)
{
	if(__get_IPSR() == 0)
	{
		// No active interrupt, we are in thread mode
		return HAL_CPU_STATE_THREAD;
	}
	else
	{
		// Active interrupt, we are in handler mode
		return HAL_CPU_STATE_HANDLER;
	}
}

static void port_cpu_low_power_enter(void)
{
    __WFI();
}

static uint32_t port_cpu_time_get_ms(void)
{
    return HAL_GetTick();
}

static void port_cpu_sleep_ms(uint32_t tmr_ms)
{
    volatile uint32_t tmr_new_ms;
    volatile uint32_t tmr_old_ms = port_cpu_time_get_ms();

    while(1)
    {
        if(hal_time_elapsed(tmr_old_ms) >= tmr_ms)
            break;

        port_cpu_low_power_enter();
    }
}

hal_cpu_driver_t HAL_CPU_DRIVER =
{
    .init = port_cpu_init,
    .deinit = port_cpu_deinit,
    .reset = port_cpu_reset,
    .watchdog_refresh = port_cpu_watchdog_refresh,
    .id_get = port_cpu_id_get,
    .random_seed_get = port_cpu_random_seed_get,
    .critical_section_enter = port_cpu_critsec_enter,
    .critical_section_leave = port_cpu_critsec_leave,
    .low_power_enter = port_cpu_low_power_enter,
    .sleep_ms = port_cpu_sleep_ms,
    .time_get_ms = port_cpu_time_get_ms,
    .time_elapsed_get_ms = port_cpu_time_elapsed_get_ms,
};
