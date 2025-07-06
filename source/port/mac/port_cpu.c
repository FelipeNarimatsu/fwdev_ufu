#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <dispatch/dispatch.h>

#include "hal.h"
#include "app.h"

extern char* main_app_name_get(void);

static bool port_cpu_init_done = false;
static pthread_mutex_t port_cpu_cs;
static pthread_mutexattr_t port_cpu_cs_attr;

static dispatch_queue_t port_systick_queue;
static dispatch_source_t port_systick_timer = 0;
static volatile uint32_t port_systick_cnt = 0;

static void port_cpu_sigint_handler(int sig_num)
{
    UTL_DBG_PRINTF(UTL_DBG_MOD_PORT, "CTRL+C pressed!\n");
    app_terminate_set();
}

static void port_cpu_systick_handler_cbk(void* context)
{
    port_systick_cnt++;
}

static void port_systick_create(void)
{
    port_systick_queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    port_systick_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, port_systick_queue);

    if(port_systick_timer)
    {
        dispatch_source_set_timer(port_systick_timer, dispatch_time(DISPATCH_TIME_NOW, 0), 1 * NSEC_PER_MSEC, 0);

        dispatch_source_set_event_handler_f(port_systick_timer, port_cpu_systick_handler_cbk);
        dispatch_set_context(port_systick_timer, port_systick_timer);
        dispatch_resume(port_systick_timer);
    }
    else
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_PORT, "Could not create systick timer!\n");
    }
}

static void port_cpu_init(void)
{
    // creating critical section mutex
    UTL_DBG_PRINTF(UTL_DBG_MOD_PORT, "Creating critical section mutex...\n");
    pthread_mutexattr_init(&port_cpu_cs_attr);
    pthread_mutexattr_settype(&port_cpu_cs_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&port_cpu_cs, &port_cpu_cs_attr);

    // handling SIGINT (CTRL+C)
    signal(SIGINT, port_cpu_sigint_handler);

    // adding support for 1ms tick timer (systick)
    UTL_DBG_PRINTF(UTL_DBG_MOD_PORT, "Initializing 1ms tick timer...\n");
    port_systick_create();

    port_cpu_init_done = true;
}

static void port_cpu_deinit(void)
{
    dispatch_suspend(port_systick_timer);
}

static void port_cpu_reset(void)
{
    char* app_name = main_app_name_get();
    char* rst_cmd = malloc(strlen(app_name) + 32);
    UTL_DBG_PRINTF(UTL_DBG_MOD_PORT, "Restarting app %s in 5s...\n", app_name);
    sprintf(rst_cmd, "/bin/bash -c 'sleep 5; %s '", app_name);
    system(rst_cmd);
    free(rst_cmd);
    // app must terminate itself in 5 seconds, check app_terminate() function
    app_terminate_set();
}

static void port_cpu_watchdog_refresh(void)
{
}

static void port_cpu_id_get(uint8_t* id)
{
    memcpy(id, "STM32F4SIMUL", HAL_CPU_ID_SIZE);
}

static uint32_t port_cpu_random_seed_get(void)
{
    // generate a 32 bits random seed based on the current time
    uint32_t rnd_seed;
    time_t tm = time(NULL);

    if(sizeof(time_t) > sizeof(uint32_t))
        rnd_seed = (uint32_t) (tm) ^ ((uint32_t) (tm >> 32));
    else
        rnd_seed = (uint32_t) tm;

    UTL_DBG_PRINTF(UTL_DBG_MOD_PORT, "Random seed: 0x%08X\n", rnd_seed);

    return rnd_seed;
}

static void port_cpu_critical_section_leave(uint32_t last_level)
{
    pthread_mutex_unlock(&port_cpu_cs);
}

static uint32_t port_cpu_critical_section_enter(hal_cpu_cs_level_t level)
{
    pthread_mutex_lock(&port_cpu_cs);
    return 0;
}

static void port_cpu_low_power_enter(void)
{
}

static uint32_t port_cpu_time_get_ms(void)
{
    return port_systick_cnt;
}

static void port_cpu_sleep_ms(uint32_t tmr_ms)
{
    usleep(1000 * tmr_ms);
}

hal_cpu_driver_t HAL_CPU_DRIVER = {
    .init = port_cpu_init,
    .deinit = port_cpu_deinit,
    .reset = port_cpu_reset,
    .watchdog_refresh = port_cpu_watchdog_refresh,
    .id_get = port_cpu_id_get,
    .random_seed_get = port_cpu_random_seed_get,
    .critical_section_enter = port_cpu_critical_section_enter,
    .critical_section_leave = port_cpu_critical_section_leave,
    .low_power_enter = port_cpu_low_power_enter,
    .sleep_ms = port_cpu_sleep_ms,
    .time_get_ms = port_cpu_time_get_ms,
};
