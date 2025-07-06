#include "hal.h"

static hal_cpu_driver_t* drv = &HAL_CPU_DRIVER;

void hal_cpu_init(void)
{
    drv->init();
}

void hal_cpu_deinit(void)
{
    drv->deinit();
}

void hal_cpu_reset(void)
{
    drv->reset();
}

void hal_cpu_watchdog_refresh(void)
{
    drv->watchdog_refresh();
}

void hal_cpu_id_get(uint8_t id[HAL_CPU_ID_SIZE])
{
    drv->id_get(id);
}

uint32_t hal_cpu_random_seed_get(void)
{
    return drv->random_seed_get();
}

uint32_t hal_cpu_critical_section_enter(hal_cpu_cs_level_t level)
{
    return drv->critical_section_enter(level);
}

void hal_cpu_critical_section_leave(uint32_t last_level)
{
    drv->critical_section_leave(last_level);
}

void hal_cpu_low_power_enter(void)
{
    drv->low_power_enter();
}

void hal_cpu_sleep_ms(uint32_t tmr_ms)
{
    drv->sleep_ms(tmr_ms);
}

uint32_t hal_cpu_time_get_ms(void)
{
    return drv->time_get_ms();
}

uint32_t hal_cpu_time_elapsed_get_ms(uint32_t tmr_old_ms)
{
    uint32_t elapsed_ms;
    uint32_t tmr_new_ms = hal_cpu_time_get_ms();

    if(tmr_new_ms < tmr_old_ms)
        elapsed_ms = UINT32_MAX - tmr_old_ms + tmr_new_ms + 1;
    else
        elapsed_ms = tmr_new_ms - tmr_old_ms;

    return elapsed_ms;
}
