#include "hal.h"

void hal_deinit(void)
{
    hal_uart_deinit();
    hal_cpu_deinit();
}

void hal_init(void)
{
    utl_dbg_init();
    utl_dbg_mod_enable(UTL_DBG_MOD_PORT);
    hal_cpu_init();
    hal_uart_init();

    // init C random seed
    srand(hal_cpu_random_seed_get());
}
