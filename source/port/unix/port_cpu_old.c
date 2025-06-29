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

static pthread_mutex_t port_cpu_cs;
static volatile uint32_t port_systick_cnt = 0;

static void port_cpu_sigint_handler(int sig_num) {
    app_terminate_set();
}

static void port_cpu_init(void) {
    pthread_mutex_init(&port_cpu_cs, NULL);
    signal(SIGINT, port_cpu_sigint_handler);
}

static void port_cpu_deinit(void) {}

static void port_cpu_reset(void) {
    app_terminate_set();
}

static void port_cpu_watchdog_refresh(void) {}

static void port_cpu_id_get(uint8_t *id) {
    memcpy(id, "UNIXPORTSIMUL", HAL_CPU_ID_SIZE);
}

static uint32_t port_cpu_random_seed_get(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint32_t)(ts.tv_nsec ^ ts.tv_sec);
}

static uint32_t port_cpu_critical_section_enter(hal_cpu_cs_level_t level) {
    pthread_mutex_lock(&port_cpu_cs);
    return 0;
}

static void port_cpu_critical_section_leave(uint32_t last_level) {
    pthread_mutex_unlock(&port_cpu_cs);
}

static void port_cpu_low_power_enter(void) {}

static uint32_t port_cpu_time_get_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void port_cpu_sleep_ms(uint32_t tmr_ms) {
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
