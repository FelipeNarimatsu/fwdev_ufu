#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>

#include "hal.h"
#include "utl_dbg.h"
#include "utl_cbf.h"

#define PORT_UART_BUFFER_SIZE 512
#define PORT_FILE_NAME_LEN 255
#define PORT_UART_DEV_NAME_LEN 255

static void port_uart_close(hal_uart_dev_t pdev);

UTL_CBF_DECLARE(cb0, PORT_UART_BUFFER_SIZE);
UTL_CBF_DECLARE(cb1, PORT_UART_BUFFER_SIZE);

struct hal_uart_dev_s
{
    utl_cbf_t* cb;
    uint8_t name[PORT_FILE_NAME_LEN];
    hal_uart_interrupt_t cbk;
    hal_uart_config_t cfg;
    pthread_t thread;
    int file;
    bool in_use;
    hal_uart_port_t dev;
};

static struct hal_uart_dev_s port_uart_ctrl[] = {
    {.cb = &cb0, .name = "/dev/ttys010", .dev = HAL_UART_PORT0},
    {.cb = &cb1, .name = "/dev/ttys011", .dev = HAL_UART_PORT1},
};

static void* port_uart_rx_thread(void* thread_param)
{
    uint8_t c;
    struct hal_uart_dev_s* pdev = (struct hal_uart_dev_s*) thread_param;

    UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "Starting thread for port %s\n", pdev->name);

    while(true)
    {
        if(pdev->in_use)
        {
            int n = read(pdev->file, &c, 1);
            if(n <= 0)
            {
                usleep(500);
            }
            else
            {
                // UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "RX %02X\n",c);
                if(pdev->cfg.interrupt_callback)
                    pdev->cfg.interrupt_callback(c);
                else
                    utl_cbf_put(pdev->cb, c);
            }
        }
        else
            break;
    }

    UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "Stoping thread for port %s\n", pdev->name);

    return 0;
}

static void port_uart_port_check(size_t dev)
{
    UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "Checking serial port %s...\n", port_uart_ctrl[dev].name);

    int file = open((char*) port_uart_ctrl[dev].name, O_RDWR | O_NOCTTY | O_NDELAY);

    if(file < 0)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "Serial port %s does not exist\n", port_uart_ctrl[dev].name);
    }
    else
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "Serial port %s is available\n", port_uart_ctrl[dev].name);
        close(file);
    }
}

static void port_uart_init(void)
{
    for(size_t dev = HAL_UART_PORT0; dev < HAL_UART_NUM_PORTS; dev++)
    {
        port_uart_port_check(dev);

        port_uart_ctrl[dev].in_use = false;
        port_uart_ctrl[dev].cbk = 0;
        port_uart_ctrl[dev].file = -1;
        utl_cbf_flush(port_uart_ctrl[dev].cb);
    }
}

static void port_uart_deinit(void)
{
    for(size_t dev = HAL_UART_PORT0; dev < HAL_UART_NUM_PORTS; dev++)
    {
        if(port_uart_ctrl[dev].in_use)
        {
            port_uart_close(&(port_uart_ctrl[dev]));
        }
    }
}

static speed_t port_uart_baud_rate_translate(hal_uart_baud_rate_t baud_rate)
{
    switch(baud_rate)
    {
    case HAL_UART_BAUD_RATE_9600:
        return B9600;
    case HAL_UART_BAUD_RATE_19200:
        return B19200;
    case HAL_UART_BAUD_RATE_38400:
        return B38400;
    case HAL_UART_BAUD_RATE_57600:
        return B57600;
    case HAL_UART_BAUD_RATE_115200:
        return B115200;
    default:
        return B9600; // Default to 9600 if unknown
    }
}

static hal_uart_dev_t port_uart_open(hal_uart_port_t dev, hal_uart_config_t* cfg)
{
    hal_uart_dev_t pdev = 0;

    if(dev >= HAL_UART_NUM_PORTS)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "Invalid UART port %d\n", dev);
        return 0;
    }

    if(port_uart_ctrl[dev].in_use)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "UART port %d in use\n", dev);
        return 0;
    }

    // point to device and open serial port
    pdev = &(port_uart_ctrl[dev]);
    pdev->cfg = *cfg;
    utl_cbf_flush(pdev->cb);

    pdev->file = open((char*) pdev->name, O_RDWR | O_NOCTTY | O_NDELAY);
    if(pdev->file < 0)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "Can not open serial device %d (%s)\n", dev, pdev->name);
        return 0;
    }

    // set baud rate
    struct termios settings;
    tcgetattr(port_uart_ctrl[dev].file, &settings);
    cfsetospeed(&settings, port_uart_baud_rate_translate(pdev->cfg.baud_rate));
    cfsetispeed(&settings, port_uart_baud_rate_translate(pdev->cfg.baud_rate));

    // parity
    if(pdev->cfg.parity == HAL_UART_PARITY_NONE)
        settings.c_cflag &= ~PARENB; // no parity
    else if(pdev->cfg.parity == HAL_UART_PARITY_ODD)
        settings.c_cflag |= (PARENB | PARODD); // odd parity
    else if(pdev->cfg.parity == HAL_UART_PARITY_EVEN)
        settings.c_cflag |= PARENB; // even parity

    // stop bits
    if(pdev->cfg.stop_bits == HAL_UART_STOP_BITS_1)
        settings.c_cflag &= ~CSTOPB; // 1 stop bit
    else if(pdev->cfg.stop_bits == HAL_UART_STOP_BITS_2)
        settings.c_cflag |= CSTOPB; // 2 stop bits

    // flow control
    if(pdev->cfg.flow_control == HAL_UART_FLOW_CONTROL_NONE)
        settings.c_cflag &= ~CRTSCTS;
    else if(pdev->cfg.flow_control == HAL_UART_FLOW_CONTROL_CTS_RTS)
        settings.c_cflag |= CRTSCTS;

    // 8 bits data size
    settings.c_cflag |= CS8;
    // desativa bits de dados 5, 6 e 7
    settings.c_cflag &= ~(CS5 | CS6 | CS7);

    // coloca a porta em modo raw
    // INLCR: Não traduz \n (line feed) em \r (carriage return) na entrada.
    // IGNCR: Não ignora \r na entrada.
    // ICRNL: Não converte \r em \n na entrada.
    // IXON: Desativa controle de fluxo por software (XON).
    // IXOFF: Desativa controle de fluxo por software (XOFF).
    settings.c_iflag &= ~(INLCR | IGNCR | ICRNL | IXON | IXOFF);
    // ONLCR: Não converte \n em \r\n na saída.
    // OCRNL: Não converte \r em \n na saída.
    settings.c_oflag &= ~(ONLCR | OCRNL);
    // ECHO: Desativa o eco dos caracteres digitados.
    // ECHONL: Desativa o eco automático de \n.
    // ICANON: Desativa o modo canônico (linha a linha); ativa modo raw (caractere a caractere).
    // ISIG: Desativa sinais gerados por teclas como Ctrl+C (SIGINT), Ctrl+Z (SIGTSTP), etc.
    // IEXTEN: Desativa extensões de entrada como Ctrl+V (literal next).
    settings.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

    // leitura não exige nenhum byte mínimo para retornar
    settings.c_cc[VMIN] = 0;
    // timeout de 0,1 segundo na leitura
    settings.c_cc[VTIME] = 1;

    // aplicar as configurações
    tcsetattr(pdev->file, TCSANOW, &settings);
    tcflush(pdev->file, TCIOFLUSH);

    // create thread to receive data
    int err = pthread_create(&pdev->thread, NULL, &port_uart_rx_thread, (void*) pdev);
    if(err != 0)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "Cant create RX thread for port %d\n", pdev->dev);
        close(pdev->file);
        return 0;
    }

    UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "Serial port %d opened (%s)\n", pdev->dev, pdev->name);
    pdev->in_use = true;

    return pdev;
}

static void port_uart_close(hal_uart_dev_t pdev)
{
    if(pdev->in_use)
    {
        UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "Closing serial port %s ...\n", pdev->name);
        pdev->in_use = false;
        pthread_join(pdev->thread, NULL);
        close(pdev->file);
        pdev->file = -1;
    }
}

static size_t port_uart_bytes_available(hal_uart_dev_t pdev)
{
    size_t size;

    if(pdev->cfg.interrupt_callback)
        return 0;

    uint32_t state = hal_cpu_critical_section_enter(HAL_CPU_CS_PROCESSOR_LEVEL);
    size = utl_cbf_bytes_available(pdev->cb);
    hal_cpu_critical_section_leave(state);

    return size;
}

static int32_t port_uart_read(hal_uart_dev_t pdev, uint8_t* buffer, size_t size)
{
    size_t pos = 0;

    // data are not stored on buffers when using interrupt
    if(pdev->cfg.interrupt_callback)
        return 0;

    if(port_uart_bytes_available(pdev) == 0)
        return 0;

    while(pos < size)
    {
        if(utl_cbf_get(pdev->cb, &buffer[pos]) == UTL_CBF_EMPTY)
            break;

        pos++;
    }

    return (int32_t) pos;
}

static int32_t port_uart_write(hal_uart_dev_t pdev, uint8_t* buffer, size_t size)
{
    int bytes_written;
    uint8_t* pdata = buffer;
    uint32_t retries = 20;

    if(pdev->in_use)
    {
        while(size > 0)
        {
            bytes_written = write(pdev->file, pdata, size);
            if(bytes_written < 0)
            {
                // Handle error
                UTL_DBG_PRINTF(UTL_DBG_MOD_UART, "Error writing to serial port %s: %s\n", pdev->name, strerror(errno));
                usleep(1000);
                if(--retries == 0)
                    break;
                else
                    continue;
            }
            retries = 20;
            pdata += bytes_written;
            size -= bytes_written;
        }
    }

    return (int32_t) (pdata - buffer);
}

static void port_uart_flush(hal_uart_dev_t pdev)
{
    if(pdev->cfg.interrupt_callback)
        return;

    uint32_t state = hal_cpu_critical_section_enter(HAL_CPU_CS_PROCESSOR_LEVEL);
    utl_cbf_flush(pdev->cb);
    hal_cpu_critical_section_leave(state);
}

hal_uart_driver_t HAL_UART_DRIVER = {
    .init = port_uart_init,
    .deinit = port_uart_deinit,
    .open = port_uart_open,
    .close = port_uart_close,
    .bytes_available = port_uart_bytes_available,
    .read = port_uart_read,
    .write = port_uart_write,
    .flush = port_uart_flush,
};
