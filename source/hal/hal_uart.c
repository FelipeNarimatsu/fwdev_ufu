#include "hal.h"

static hal_uart_driver_t* drv = &HAL_UART_DRIVER;

void hal_uart_init(void)
{
    drv->init();
}

void hal_uart_deinit(void)
{
    drv->deinit();
}

hal_uart_dev_t hal_uart_open(hal_uart_port_t dev, hal_uart_config_t* cfg)
{
    return drv->open(dev, cfg);
}

void hal_uart_close(hal_uart_dev_t dev)
{
    drv->close(dev);
}

size_t hal_uart_bytes_available(hal_uart_dev_t dev)
{
    return drv->bytes_available(dev);
}

ssize_t hal_uart_read(hal_uart_dev_t dev, uint8_t* buffer, size_t size)
{
    return drv->read(dev, buffer, size);
}

ssize_t hal_uart_write(hal_uart_dev_t dev, uint8_t* buffer, size_t size)
{
    return drv->write(dev, buffer, size);
}

void hal_uart_flush(hal_uart_dev_t dev)
{
    drv->flush(dev);
}

ssize_t hal_uart_byte_read(hal_uart_dev_t dev, uint8_t* c)
{
    return drv->read(dev, c, 1);
}

ssize_t hal_uart_byte_write(hal_uart_dev_t dev, uint8_t c)
{
    return drv->write(dev, &c, 1);
}
