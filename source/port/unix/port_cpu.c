#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hal.h"
#include "app.h"

pthread_mutex_t semaphore;

extern char *main_app_name_get(void);

//interrupt signal handler
static void port_cpu_sigint_handler(int sig_num)
{
    UTL_DBG_PRINTF(UTL_DBG_MOD_PORT, "CTRL+C pressed!\n");
    app_terminate_set();
}

static void port_cpu_init(void)
{
    UTL_DBG_PRINTF(UTL_DBG_MOD_PORT, "Top master semaphore lock data protection!\n");
    pthread_mutex_lock(&semaphore);

    //atrribute the SIGINT (CTRL+C) terminal command to the handler function
    signal(SIGINT, port_cpu_sigint_handler);

    UTL_DBG_PRINTF(UTL_DBG_MOD_PORT, "Top master semaphore unlock!\n");
    pthread_mutex_unlock(&semaphore);
}

static void port_cpu_deinit(void)
{
    //nothing to deinit
}

static void port_cpu_reset(void)
{
    char *app_name = main_app_name_get();
    char *rst_cmd = malloc(strlen(app_name) + 32);
    UTL_DBG_PRINTF(UTL_DBG_MOD_PORT, "Restarting app %s in 5s...\n", app_name);
    sprintf(rst_cmd, "/bin/bash -c 'sleep 5; %s '", app_name);
    system(rst_cmd);
    free(rst_cmd);
    // app must terminate itself in 5 seconds, check app_terminate() function
    app_terminate_set();
}

static void port_cpu_watchdog_refresh(void)
{
    //no cpu watchdog refresh implementation
}

static void port_cpu_id_get(uint8_t *id)
{
    memcpy(id, "STM32F4SIMUL", HAL_CPU_ID_SIZE);
}

static uint32_t port_cpu_random_seed_get(void)
{
    //rand() generates a 15 to 31 bit random
    //or two to create a full 0 to 31 bit random
    uint32_t rnd = ((uint32_t)rand() << 16) | (uint32_t)rand();
    UTL_DBG_PRINTF(UTL_DBG_MOD_PORT, "Random seed: 0x%08X\n", rnd);

    return rnd;
}

static uint32_t port_cpu_critical_section_enter(hal_cpu_cs_level_t level) {
    pthread_mutex_lock(&semaphore);

    //return timestamp in ms
    // return port_cpu_time_get_ms();
    return 0;
}

static void port_cpu_critical_section_leave(uint32_t last_level)
{
    pthread_mutex_unlock(&semaphore);
}

static void port_cpu_low_power_enter(void)
{
    //don't have low power mode implementation 
}


static void port_cpu_sleep_ms(uint32_t tmr_ms) {
    usleep(tmr_ms * 1000);
}

//port_get_time_since_simul_cpu_boot_ms
static uint32_t port_cpu_time_get_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);

    return (uint32_t)(t.tv_sec * 1000);
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
