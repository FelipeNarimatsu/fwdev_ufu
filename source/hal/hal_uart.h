#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum hal_uart_port_e
{
    HAL_UART_PORT0 = 0,
    HAL_UART_PORT1,
    HAL_UART_NUM_PORTS,
} hal_uart_port_t;

typedef enum hal_uart_baud_rate_e
{
    HAL_UART_BAUD_RATE_9600 = 0,
    HAL_UART_BAUD_RATE_19200,
    HAL_UART_BAUD_RATE_38400,
    HAL_UART_BAUD_RATE_57600,
    HAL_UART_BAUD_RATE_115200,
} hal_uart_baud_rate_t;

typedef enum hal_uart_stop_bits_e
{
    HAL_UART_STOP_BITS_1 = 0,
    HAL_UART_STOP_BITS_2,
} hal_uart_stop_bits_t;

typedef enum hal_uart_parity_e
{
    HAL_UART_PARITY_NONE = 0,
    HAL_UART_PARITY_ODD,
    HAL_UART_PARITY_EVEN,
} hal_uart_parity_t;

typedef enum hal_uart_flow_control_e
{
    HAL_UART_FLOW_CONTROL_NONE = 0,
    HAL_UART_FLOW_CONTROL_CTS_RTS,
} hal_uart_flow_control_t;

typedef struct hal_uart_dev_s* hal_uart_dev_t;

typedef void (*hal_uart_interrupt_t)(uint8_t c);

typedef struct hal_uart_config_s
{
    hal_uart_baud_rate_t baud_rate;
    hal_uart_parity_t parity;
    hal_uart_stop_bits_t stop_bits;
    hal_uart_flow_control_t flow_control;
    hal_uart_interrupt_t interrupt_callback;
} hal_uart_config_t;

typedef struct hal_uart_driver_s
{
    void (*init)(void);
    void (*deinit)(void);
    hal_uart_dev_t (*open)(hal_uart_port_t dev, hal_uart_config_t* cfg);
    void (*close)(hal_uart_dev_t dev);
    size_t (*bytes_available)(hal_uart_dev_t dev);
    int32_t (*read)(hal_uart_dev_t dev, uint8_t* buffer, size_t size);
    int32_t (*write)(hal_uart_dev_t dev, uint8_t* buffer, size_t size);
    void (*flush)(hal_uart_dev_t dev);
} hal_uart_driver_t;

void hal_uart_init(void);
void hal_uart_deinit(void);
hal_uart_dev_t hal_uart_open(hal_uart_port_t dev, hal_uart_config_t* cfg);
void hal_uart_close(hal_uart_dev_t dev);
size_t hal_uart_bytes_available(hal_uart_dev_t dev);
int32_t hal_uart_read(hal_uart_dev_t dev, uint8_t* buffer, size_t size);
int32_t hal_uart_write(hal_uart_dev_t dev, uint8_t* buffer, size_t size);
void hal_uart_flush(hal_uart_dev_t dev);
int32_t hal_uart_byte_read(hal_uart_dev_t dev, uint8_t* c);
int32_t hal_uart_byte_write(hal_uart_dev_t dev, uint8_t c);

#ifdef __cplusplus
}
#endif
