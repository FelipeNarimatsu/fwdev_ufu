#include "hal.h"
#include "app.h"

static uint8_t dev_id[HAL_CPU_ID_SIZE + 1];
static hal_uart_dev_t uart_dev = 0;
static hal_uart_config_t uart_cfg = {
    .baud_rate = HAL_UART_BAUD_RATE_115200,
    .parity = HAL_UART_PARITY_NONE,
    .stop_bits = HAL_UART_STOP_BITS_1,
    .flow_control = HAL_UART_FLOW_CONTROL_NONE,
    .interrupt_callback = 0,
};

void app_init(void)
{
    utl_dbg_mod_enable(UTL_DBG_MOD_APP);
    utl_dbg_mod_enable(UTL_DBG_MOD_UART);
    UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Initalizing app...\n");

    hal_cpu_id_get(dev_id);
    dev_id[HAL_CPU_ID_SIZE] = '\0';
    UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Device ID: %s\n", dev_id);

    uart_dev = hal_uart_open(HAL_UART_PORT0, &uart_cfg);

    if(uart_dev == 0)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Failed to open UART port 0\n");
        app_terminate_set();
    }
}

bool app_loop(void)
{
    uint8_t data[128];
    int32_t size;
    UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "App loop started.\n");

    while(app_terminate_get() == false)
    {
        size = hal_uart_read(uart_dev, data, sizeof(data));

        if(size > 0)
        {
            UTL_DBG_DUMP(UTL_DBG_MOD_APP, data, size);
            if(hal_uart_write(uart_dev, data, size) <= 0)
            {
                UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Failed to write data to UART\n");
                app_terminate_set();
            }
        }

        hal_cpu_sleep_ms(100);
    }

    hal_uart_close(uart_dev);

    UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "App loop terminated.\n");

    return true;
}