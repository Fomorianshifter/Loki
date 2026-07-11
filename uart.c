/**
 * UART Hardware Abstraction Layer Implementation
 * Linux SBC (Raspberry Pi) — production implementation
 *
 * RS-485 control (option 3):
 *   1. Kernel auto-direction via TIOCSRS485 ioctl (preferred)
 *   2. Manual GPIO DE toggle via sysfs (fallback, always available on Linux)
 *      Optionally replaced by libgpiod when compiled with -DHAVE_LIBGPIOD
 */

#include "uart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <pthread.h>
#include <time.h>

#ifdef __linux__
#include <linux/serial.h>   /* struct serial_rs485, TIOCSRS485 */
#endif

#ifdef HAVE_LIBGPIOD
#include <gpiod.h>
#endif

/* ===== CONSTANTS ===== */

/** Default UART device on Raspberry Pi (symlink to active UART). */
#define UART_DEFAULT_DEVICE      "/dev/serial0"
#define UART_FALLBACK_DEVICE_1   "/dev/ttyAMA0"
#define UART_FALLBACK_DEVICE_2   "/dev/ttyS0"

/** Ring buffer capacity — must be a power of 2. */
#define UART_RX_BUFFER_SIZE      512u
#define UART_RX_BUFFER_MASK      (UART_RX_BUFFER_SIZE - 1u)

/** RX thread poll timeout in milliseconds. */
#define UART_RX_THREAD_POLL_MS   50

/** Maximum length for a sysfs GPIO path string. */
#define UART_SYSFS_PATH_LEN      64

/* ===== RING BUFFER ===== */

/**
 * Lock-free single-producer / single-consumer ring buffer.
 * head and tail are monotonically increasing uint32_t indices;
 * unsigned subtraction gives the correct count when they wrap.
 * Requires UART_RX_BUFFER_SIZE to be a power of 2.
 */
typedef struct {
    uint8_t  buf[UART_RX_BUFFER_SIZE];
    uint32_t head;  /* write index (incremented by RX thread) */
    uint32_t tail;  /* read  index (incremented by consumer)  */
} ring_buf_t;

static inline uint32_t rb_count(const ring_buf_t *rb)
{
    return rb->head - rb->tail;
}

static inline int rb_full(const ring_buf_t *rb)
{
    return rb_count(rb) == UART_RX_BUFFER_SIZE;
}

static inline void rb_push(ring_buf_t *rb, uint8_t byte)
{
    rb->buf[rb->head & UART_RX_BUFFER_MASK] = byte;
    rb->head++;
}

static inline uint8_t rb_pop(ring_buf_t *rb)
{
    uint8_t b = rb->buf[rb->tail & UART_RX_BUFFER_MASK];
    rb->tail++;
    return b;
}

/* ===== UART CONTEXT ===== */

typedef struct {
    uart_port_t          port;
    int                  fd;
    uart_config_t        config;
    uart_rx_callback_t   rx_callback;
    uint8_t              initialized;

    /* RS-485 state */
    uint8_t              rs485_kernel;       /* 1 = TIOCSRS485 auto-direction active */
    uint8_t              rs485_gpio_active;  /* 1 = manual GPIO DE active            */
    int                  rs485_gpio_val_fd;  /* sysfs .../value fd; -1 if unused     */
#ifdef HAVE_LIBGPIOD
    struct gpiod_chip   *gpiod_chip;
    struct gpiod_line_request *gpiod_de_request;
    unsigned int        gpiod_de_offset;
#endif

    /* Thread synchronisation */
    pthread_mutex_t      mutex;
    pthread_cond_t       rx_cond;
    pthread_t            rx_thread;
    volatile uint8_t     rx_thread_stop;
    uint8_t              rx_thread_started;

    /* Receive ring buffer (protected by mutex) */
    ring_buf_t           rx_buf;
} uart_context_t;

/* Static context for UART_PORT_1 */
static uart_context_t uart_context_1 = {
    .port              = UART_PORT_1,
    .fd                = -1,
    .rx_callback       = NULL,
    .initialized       = 0,
    .rs485_kernel      = 0,
    .rs485_gpio_active = 0,
    .rs485_gpio_val_fd = -1,
    .rx_thread_stop    = 0,
    .rx_thread_started = 0,
};

/* ===== INTERNAL HELPERS ===== */

static uart_context_t *ctx_for_port(uart_port_t port)
{
    if (port == UART_PORT_1) {
        return &uart_context_1;
    }
    return NULL;
}

static int uart_open_first_available(const char *primary_path)
{
    const char *candidates[3] = { primary_path, NULL, NULL };
    size_t i;

    if (primary_path == NULL || primary_path[0] == '\0'
            || strcmp(primary_path, UART_DEFAULT_DEVICE) == 0) {
        candidates[0] = UART_DEFAULT_DEVICE;
        candidates[1] = UART_FALLBACK_DEVICE_1;
        candidates[2] = UART_FALLBACK_DEVICE_2;
    }

    for (i = 0; i < 3 && candidates[i] != NULL; i++) {
        int fd = open(candidates[i], O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd >= 0) {
            return fd;
        }
        fprintf(stderr, "[uart] open %s: %s\n", candidates[i], strerror(errno));
    }

    return -1;
}

/**
 * Map an integer baud rate to a POSIX speed_t constant.
 * Falls back to B115200 for unrecognised values.
 */
static speed_t baud_to_speed(uint32_t baud)
{
    switch (baud) {
        case 1200:    return B1200;
        case 2400:    return B2400;
        case 4800:    return B4800;
        case 9600:    return B9600;
        case 19200:   return B19200;
        case 38400:   return B38400;
        case 57600:   return B57600;
        case 115200:  return B115200;
        case 230400:  return B230400;
        case 460800:  return B460800;
        case 921600:  return B921600;
        case 1000000: return B1000000;
        case 1500000: return B1500000;
        case 2000000: return B2000000;
        case 3000000: return B3000000;
        default:
            fprintf(stderr, "[uart] unknown baud %u, defaulting to 115200\n", baud);
            return B115200;
    }
}

/**
 * Apply termios settings (baud, data bits, parity, stop bits, raw mode)
 * to an already-opened file descriptor.
 * Returns 0 on success, -1 on error.
 */
static int configure_termios(int fd, const uart_config_t *cfg)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "[uart] tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    /* Baud rate */
    speed_t speed = baud_to_speed(cfg->baud_rate);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    /* Data bits */
    tty.c_cflag &= ~CSIZE;
    switch (cfg->data_bits) {
        case UART_DATA_BITS_5: tty.c_cflag |= CS5; break;
        case UART_DATA_BITS_6: tty.c_cflag |= CS6; break;
        case UART_DATA_BITS_7: tty.c_cflag |= CS7; break;
        case UART_DATA_BITS_8:
        default:               tty.c_cflag |= CS8; break;
    }

    /* Stop bits */
    if (cfg->stop_bits == UART_STOP_BITS_2) {
        tty.c_cflag |=  CSTOPB;
    } else {
        tty.c_cflag &= ~CSTOPB;
    }

    /* Parity */
    tty.c_cflag &= ~(PARENB | PARODD);
    if (cfg->parity == UART_PARITY_EVEN) {
        tty.c_cflag |= PARENB;
    } else if (cfg->parity == UART_PARITY_ODD) {
        tty.c_cflag |= PARENB | PARODD;
    }

    /* No hardware flow control, enable receiver, ignore modem lines */
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |=  CREAD | CLOCAL;

    /* Raw mode: no line discipline processing */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    /* Non-blocking reads: return immediately with whatever is available */
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "[uart] tcsetattr: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/* ===== RS-485 SUPPORT ===== */

/**
 * Try to enable kernel RS-485 auto-direction via TIOCSRS485.
 * Returns 1 on success, 0 if the ioctl is unsupported on this device.
 */
static int rs485_try_kernel(int fd)
{
#if defined(__linux__) && defined(TIOCSRS485)
    struct serial_rs485 rs485;
    memset(&rs485, 0, sizeof(rs485));
    rs485.flags = SER_RS485_ENABLED | SER_RS485_RTS_ON_SEND;
    /* DE goes low after send (normal half-duplex convention) */
    rs485.flags &= ~(uint32_t)SER_RS485_RTS_AFTER_SEND;
    rs485.delay_rts_before_send = 0;
    rs485.delay_rts_after_send  = 0;

    if (ioctl(fd, TIOCSRS485, &rs485) == 0) {
        fprintf(stderr, "[uart] RS-485: kernel auto-direction enabled (TIOCSRS485)\n");
        return 1;
    }
    fprintf(stderr, "[uart] RS-485: TIOCSRS485 not supported on this device (%s)\n",
            strerror(errno));
#else
    (void)fd;
    fprintf(stderr, "[uart] RS-485: TIOCSRS485 not available on this platform\n");
#endif
    return 0;
}

#ifdef HAVE_LIBGPIOD
/**
 * Initialise RS-485 DE control via libgpiod.
 * Returns 1 on success.
 */
static int rs485_init_gpio_libgpiod(uart_context_t *ctx, int gpio_pin)
{
    struct gpiod_line_settings *settings = NULL;
    struct gpiod_line_config *line_cfg = NULL;
    struct gpiod_request_config *req_cfg = NULL;
    struct gpiod_line_request *request = NULL;
    unsigned int offset;

    if (gpio_pin < 0) {
        return 0;
    }

    offset = (unsigned int)gpio_pin;
    ctx->gpiod_chip = gpiod_chip_open("/dev/gpiochip0");
    if (!ctx->gpiod_chip) {
        fprintf(stderr, "[uart] libgpiod: cannot open /dev/gpiochip0: %s\n", strerror(errno));
        return 0;
    }

    settings = gpiod_line_settings_new();
    line_cfg = gpiod_line_config_new();
    req_cfg = gpiod_request_config_new();
    if (!settings || !line_cfg || !req_cfg) {
        fprintf(stderr, "[uart] libgpiod: unable to allocate request objects\n");
        goto error;
    }

    if (gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT) < 0) {
        fprintf(stderr, "[uart] libgpiod: unable to set output direction for GPIO %d\n", gpio_pin);
        goto error;
    }

    if (gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings) < 0) {
        fprintf(stderr, "[uart] libgpiod: unable to configure GPIO %d\n", gpio_pin);
        goto error;
    }

    gpiod_request_config_set_consumer(req_cfg, "uart-rs485-de");
    request = gpiod_chip_request_lines(ctx->gpiod_chip, req_cfg, line_cfg);
    if (!request) {
        fprintf(stderr, "[uart] libgpiod: GPIO %d request output failed\n", gpio_pin);
        goto error;
    }

    if (gpiod_line_request_set_value(request, offset, GPIOD_LINE_VALUE_INACTIVE) < 0) {
        fprintf(stderr, "[uart] libgpiod: failed to drive GPIO %d low\n", gpio_pin);
        goto error;
    }

    ctx->gpiod_de_request = request;
    ctx->gpiod_de_offset = offset;
    gpiod_line_settings_free(settings);
    gpiod_line_config_free(line_cfg);
    gpiod_request_config_free(req_cfg);

    fprintf(stderr, "[uart] RS-485: libgpiod GPIO %d configured as DE\n", gpio_pin);
    return 1;

error:
    if (request) {
        gpiod_line_request_release(request);
    }
    gpiod_line_settings_free(settings);
    gpiod_line_config_free(line_cfg);
    gpiod_request_config_free(req_cfg);
    if (ctx->gpiod_chip) {
        gpiod_chip_close(ctx->gpiod_chip);
        ctx->gpiod_chip = NULL;
    }
    ctx->gpiod_de_request = NULL;
    return 0;
}
#endif /* HAVE_LIBGPIOD */

/**
 * Initialise RS-485 DE control via Linux sysfs GPIO.
 * Returns 1 on success.
 */
static int rs485_init_gpio_sysfs(uart_context_t *ctx, int gpio_pin)
{
    char path[UART_SYSFS_PATH_LEN];
    char num_buf[16];
    int  fd;
    ssize_t n;

    /* Export the GPIO pin */
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "[uart] sysfs: cannot open gpio/export: %s\n", strerror(errno));
        return 0;
    }
    snprintf(num_buf, sizeof(num_buf), "%d", gpio_pin);
    n = write(fd, num_buf, strlen(num_buf));
    close(fd);
    /* EBUSY means already exported — treat as non-fatal */
    if (n < 0 && errno != EBUSY) {
        fprintf(stderr, "[uart] sysfs: export GPIO %d: %s\n", gpio_pin, strerror(errno));
        return 0;
    }

    /* Set direction to output */
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio_pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "[uart] sysfs: cannot open %s: %s\n", path, strerror(errno));
        return 0;
    }
    n = write(fd, "out", 3);
    close(fd);
    if (n < 0) {
        fprintf(stderr, "[uart] sysfs: set direction GPIO %d: %s\n", gpio_pin, strerror(errno));
        return 0;
    }

    /* Open value file and keep it open for fast DE toggling */
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio_pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "[uart] sysfs: cannot open %s: %s\n", path, strerror(errno));
        return 0;
    }

    /* Start DE low (RX mode) */
    n = write(fd, "0", 1);
    if (n < 0) {
        fprintf(stderr, "[uart] sysfs: DE initial low GPIO %d: %s\n", gpio_pin, strerror(errno));
        close(fd);
        return 0;
    }

    ctx->rs485_gpio_val_fd = fd;
    fprintf(stderr, "[uart] RS-485: sysfs GPIO %d configured as DE\n", gpio_pin);
    return 1;
}

/** Assert DE high (enter TX mode). */
static void rs485_de_assert(uart_context_t *ctx)
{
#ifdef HAVE_LIBGPIOD
    if (ctx->gpiod_de_request) {
        (void)gpiod_line_request_set_value(
                ctx->gpiod_de_request, ctx->gpiod_de_offset, GPIOD_LINE_VALUE_ACTIVE);
        return;
    }
#endif
    if (ctx->rs485_gpio_val_fd >= 0) {
        /* Rewind to byte 0 before each sysfs write to avoid EINVAL on some kernels */
        lseek(ctx->rs485_gpio_val_fd, 0, SEEK_SET);
        (void)write(ctx->rs485_gpio_val_fd, "1", 1);
    }
}

/** Deassert DE low (enter RX mode). */
static void rs485_de_deassert(uart_context_t *ctx)
{
#ifdef HAVE_LIBGPIOD
    if (ctx->gpiod_de_request) {
        (void)gpiod_line_request_set_value(
                ctx->gpiod_de_request, ctx->gpiod_de_offset, GPIOD_LINE_VALUE_INACTIVE);
        return;
    }
#endif
    if (ctx->rs485_gpio_val_fd >= 0) {
        lseek(ctx->rs485_gpio_val_fd, 0, SEEK_SET);
        (void)write(ctx->rs485_gpio_val_fd, "0", 1);
    }
}

/** Release all GPIO resources. */
static void rs485_gpio_cleanup(uart_context_t *ctx)
{
#ifdef HAVE_LIBGPIOD
    if (ctx->gpiod_de_request) {
        gpiod_line_request_release(ctx->gpiod_de_request);
        ctx->gpiod_de_request = NULL;
    }
    if (ctx->gpiod_chip) {
        gpiod_chip_close(ctx->gpiod_chip);
        ctx->gpiod_chip = NULL;
    }
#endif
    if (ctx->rs485_gpio_val_fd >= 0) {
        close(ctx->rs485_gpio_val_fd);
        ctx->rs485_gpio_val_fd = -1;
    }
    ctx->rs485_gpio_active = 0;
}

/* ===== RX BACKGROUND THREAD ===== */

/**
 * Background thread: polls the serial fd and pushes received bytes into the
 * ring buffer. Signals rx_cond after each batch so uart_receive() can wake up.
 */
static void *uart_rx_thread_fn(void *arg)
{
    uart_context_t *ctx = (uart_context_t *)arg;
    struct pollfd pfd;
    uint8_t tmp[64];

    pfd.fd     = ctx->fd;
    pfd.events = POLLIN;

    while (!ctx->rx_thread_stop) {
        int ret = poll(&pfd, 1, UART_RX_THREAD_POLL_MS);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            /* Unrecoverable poll error — exit thread */
            break;
        }

        if (ret == 0 || !(pfd.revents & POLLIN)) {
            continue;
        }

        ssize_t n = read(ctx->fd, tmp, sizeof(tmp));
        if (n <= 0) {
            continue;
        }

        pthread_mutex_lock(&ctx->mutex);
        for (ssize_t i = 0; i < n; i++) {
            if (!rb_full(&ctx->rx_buf)) {
                rb_push(&ctx->rx_buf, tmp[i]);
            }
            /* Callback is invoked under the mutex — keep it brief */
            if (ctx->rx_callback) {
                ctx->rx_callback(tmp[i]);
            }
        }
        pthread_cond_broadcast(&ctx->rx_cond);
        pthread_mutex_unlock(&ctx->mutex);
    }

    return NULL;
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t uart_init(uart_port_t port, const uart_config_t *config)
{
    if (config == NULL) {
        return HAL_INVALID_PARAM;
    }

    uart_context_t *ctx = ctx_for_port(port);
    if (ctx == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (ctx->initialized) {
        return HAL_OK;
    }

    /* Resolve device path */
    const char *dev = (config->device_path != NULL && config->device_path[0] != '\0')
                       ? config->device_path
                       : UART_DEFAULT_DEVICE;

    /* Open serial device */
    ctx->fd = uart_open_first_available(dev);
    if (ctx->fd < 0) {
        return HAL_ERROR;
    }

    /* Configure termios */
    if (configure_termios(ctx->fd, config) != 0) {
        close(ctx->fd);
        ctx->fd = -1;
        return HAL_ERROR;
    }

    /* Discard any stale data */
    tcflush(ctx->fd, TCIOFLUSH);

    /* Store configuration */
    memcpy(&ctx->config, config, sizeof(uart_config_t));

    /* RS-485 setup */
    if (config->rs485_enabled) {
        /* Prefer kernel auto-direction */
        ctx->rs485_kernel = (uint8_t)rs485_try_kernel(ctx->fd);

        /* GPIO fallback if kernel RS-485 is not available */
        if (!ctx->rs485_kernel
                && config->rs485_de_gpio_fallback
                && config->rs485_de_gpio_pin >= 0) {
#ifdef HAVE_LIBGPIOD
            ctx->rs485_gpio_active = (uint8_t)rs485_init_gpio_libgpiod(
                    ctx, config->rs485_de_gpio_pin);
#endif
            if (!ctx->rs485_gpio_active) {
                ctx->rs485_gpio_active = (uint8_t)rs485_init_gpio_sysfs(
                        ctx, config->rs485_de_gpio_pin);
            }

            if (!ctx->rs485_gpio_active) {
                fprintf(stderr,
                        "[uart] RS-485 GPIO fallback unavailable — DE control disabled\n");
            }
        }
    }

    /* Initialise ring buffer */
    ctx->rx_buf.head = 0;
    ctx->rx_buf.tail = 0;

    /* Initialise mutex and condition variable */
    pthread_mutex_init(&ctx->mutex, NULL);
    pthread_cond_init(&ctx->rx_cond, NULL);

    /* Start background RX thread */
    ctx->rx_thread_stop    = 0;
    ctx->rx_thread_started = 0;
    if (pthread_create(&ctx->rx_thread, NULL, uart_rx_thread_fn, ctx) == 0) {
        ctx->rx_thread_started = 1;
    } else {
        fprintf(stderr, "[uart] failed to start RX thread: %s\n", strerror(errno));
        /* Non-fatal: uart_receive will time out without incoming data */
    }

    ctx->initialized = 1;
    return HAL_OK;
}

hal_status_t uart_send(uart_port_t port, const uint8_t *data, uint32_t length)
{
    if (data == NULL || length == 0) {
        return HAL_INVALID_PARAM;
    }

    uart_context_t *ctx = ctx_for_port(port);
    if (ctx == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (!ctx->initialized || ctx->fd < 0) {
        return HAL_NOT_READY;
    }

    /* Assert DE before transmitting (manual GPIO RS-485 path) */
    if (ctx->rs485_gpio_active) {
        rs485_de_assert(ctx);
    }

    /* Write loop — handle partial writes and EINTR */
    const uint8_t *ptr       = data;
    size_t         remaining = (size_t)length;

    while (remaining > 0) {
        ssize_t n = write(ctx->fd, ptr, remaining);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "[uart] write: %s\n", strerror(errno));
            if (ctx->rs485_gpio_active) {
                rs485_de_deassert(ctx);
            }
            return HAL_ERROR;
        }
        ptr       += (size_t)n;
        remaining -= (size_t)n;
    }

    /* Wait for all bytes to leave the hardware FIFO */
    tcdrain(ctx->fd);

    /* Optional turnaround delay before releasing the bus */
    if (ctx->rs485_gpio_active) {
        if (ctx->config.rs485_turnaround_us > 0) {
            struct timespec ts = {
                .tv_sec  = 0,
                .tv_nsec = (long)ctx->config.rs485_turnaround_us * 1000L,
            };
            nanosleep(&ts, NULL);
        }
        rs485_de_deassert(ctx);
    }

    return HAL_OK;
}

hal_status_t uart_send_byte(uart_port_t port, uint8_t byte)
{
    return uart_send(port, &byte, 1);
}

hal_status_t uart_receive(uart_port_t port, uint8_t *data, uint32_t length,
                          uint32_t timeout_ms)
{
    if (data == NULL || length == 0) {
        return HAL_INVALID_PARAM;
    }

    uart_context_t *ctx = ctx_for_port(port);
    if (ctx == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (!ctx->initialized || ctx->fd < 0) {
        return HAL_NOT_READY;
    }

    /* Build absolute deadline (timeout_ms == 0 means wait forever) */
    struct timespec deadline;
    int use_deadline = (timeout_ms > 0);
    if (use_deadline) {
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec  += (time_t)(timeout_ms / 1000u);
        deadline.tv_nsec += (long)((timeout_ms % 1000u) * 1000000L);
        if (deadline.tv_nsec >= 1000000000L) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000L;
        }
    }

    pthread_mutex_lock(&ctx->mutex);

    uint32_t received = 0;
    while (received < length) {
        /* Drain ring buffer */
        while (received < length && rb_count(&ctx->rx_buf) > 0) {
            data[received++] = rb_pop(&ctx->rx_buf);
        }

        if (received >= length) {
            break;
        }

        /* Wait for more data */
        if (!use_deadline) {
            if (pthread_cond_wait(&ctx->rx_cond, &ctx->mutex) != 0) {
                /* Spurious wake or error — loop and re-check */
            }
        } else {
            int rc = pthread_cond_timedwait(&ctx->rx_cond, &ctx->mutex, &deadline);
            if (rc == ETIMEDOUT) {
                pthread_mutex_unlock(&ctx->mutex);
                return HAL_TIMEOUT;
            }
        }
    }

    pthread_mutex_unlock(&ctx->mutex);
    return HAL_OK;
}

hal_status_t uart_receive_byte(uart_port_t port, uint8_t *byte, uint32_t timeout_ms)
{
    if (byte == NULL) {
        return HAL_INVALID_PARAM;
    }
    return uart_receive(port, byte, 1, timeout_ms);
}

uint32_t uart_available(uart_port_t port)
{
    uart_context_t *ctx = ctx_for_port(port);
    if (ctx == NULL || !ctx->initialized) {
        return 0;
    }

    pthread_mutex_lock(&ctx->mutex);
    uint32_t count = rb_count(&ctx->rx_buf);
    pthread_mutex_unlock(&ctx->mutex);
    return count;
}

hal_status_t uart_set_rx_callback(uart_port_t port, uart_rx_callback_t callback)
{
    uart_context_t *ctx = ctx_for_port(port);
    if (ctx == NULL) {
        return HAL_INVALID_PARAM;
    }

    pthread_mutex_lock(&ctx->mutex);
    ctx->rx_callback = callback;
    pthread_mutex_unlock(&ctx->mutex);
    return HAL_OK;
}

hal_status_t uart_flush(uart_port_t port)
{
    uart_context_t *ctx = ctx_for_port(port);
    if (ctx == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (!ctx->initialized || ctx->fd < 0) {
        return HAL_NOT_READY;
    }

    pthread_mutex_lock(&ctx->mutex);
    ctx->rx_buf.head = 0;
    ctx->rx_buf.tail = 0;
    pthread_mutex_unlock(&ctx->mutex);

    tcflush(ctx->fd, TCIFLUSH);
    return HAL_OK;
}

hal_status_t uart_deinit(uart_port_t port)
{
    uart_context_t *ctx = ctx_for_port(port);
    if (ctx == NULL) {
        return HAL_INVALID_PARAM;
    }

    if (!ctx->initialized) {
        return HAL_OK;
    }

    /* Mark uninitialized first so concurrent API callers return HAL_NOT_READY */
    ctx->initialized = 0;

    /* Signal RX thread to stop and wait for it to exit */
    ctx->rx_thread_stop = 1;
    if (ctx->rx_thread_started) {
        pthread_cond_broadcast(&ctx->rx_cond); /* unblock any timedwait */
        pthread_join(ctx->rx_thread, NULL);
        ctx->rx_thread_started = 0;
    }

    /* Release RS-485 GPIO resources */
    rs485_gpio_cleanup(ctx);
    ctx->rs485_kernel = 0;

    /* Close serial device */
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }

    /* Destroy synchronisation primitives */
    pthread_mutex_destroy(&ctx->mutex);
    pthread_cond_destroy(&ctx->rx_cond);

    return HAL_OK;
}
