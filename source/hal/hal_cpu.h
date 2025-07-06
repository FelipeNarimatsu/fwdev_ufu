#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#define HAL_CPU_ID_SIZE 12

typedef enum hal_cpu_cs_level_e
{
    // highest priority, blocks all interrupts
    HAL_CPU_CS_PROCESSOR_LEVEL = 0,
    HAL_CPU_CS_CUSTOM_LEVEL1,
    HAL_CPU_CS_CUSTOM_LEVEL2,
    // lowest priority, only stops code execution from non-interrupt context
    HAL_CPU_CS_USER_LEVEL,
} hal_cpu_cs_level_t;

typedef struct hal_cpu_driver_s
{
    void (*init)(void);
    void (*deinit)(void);
    void (*reset)(void);
    void (*watchdog_refresh)(void);
    void (*id_get)(uint8_t id[HAL_CPU_ID_SIZE]);
    uint32_t (*random_seed_get)(void);
    uint32_t (*critical_section_enter)(hal_cpu_cs_level_t level);
    void (*critical_section_leave)(uint32_t last_level);
    void (*low_power_enter)(void);
    void (*sleep_ms)(uint32_t tmr_ms);
    uint32_t (*time_get_ms)(void);
    uint32_t (*time_elapsed_get_ms)(uint32_t tmr_old_ms);
} hal_cpu_driver_t;

void hal_cpu_init(void);
void hal_cpu_deinit(void);
void hal_cpu_reset(void);
void hal_cpu_watchdog_refresh(void);
void hal_cpu_id_get(uint8_t id[HAL_CPU_ID_SIZE]);
uint32_t hal_cpu_random_seed_get(void);
uint32_t hal_cpu_critical_section_enter(hal_cpu_cs_level_t level);
void hal_cpu_critical_section_leave(uint32_t last_level);
void hal_cpu_low_power_enter(void);
void hal_cpu_sleep_ms(uint32_t tmr_ms);
uint32_t hal_cpu_time_get_ms(void);
uint32_t hal_cpu_time_elapsed_get_ms(uint32_t tmr_old_ms);

#ifdef __cplusplus
}
#endif
