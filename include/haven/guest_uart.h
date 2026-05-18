/**
 * Linux guest UART driver interface for hypervisor integration.
 *
 * Provides UART device configuration and I/O for guest partitions,
 * including baud rate control, interrupt handling, and DMA support.
 *
 * Supports:
 *   - Single/multiple UART ports per partition
 *   - Baud rates from 9600 to 115200
 *   - Interrupt-driven or polled I/O
 *   - DMA for bulk transfers (optional)
 *   - Flow control (RTS/CTS)
 *
 * @file include/haven/guest_uart.h
 */

#ifndef HAVEN_GUEST_UART_H
#define HAVEN_GUEST_UART_H

#include <haven/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum UART ports per partition.
 */
#define HV_MAX_UART_PORTS_PER_PARTITION 4

/**
 * Maximum UART ports system-wide.
 */
#define HV_MAX_UART_PORTS 32

/**
 * UART baud rate enumeration.
 */
typedef enum {
	HV_UART_BAUD_9600 = 9600,
	HV_UART_BAUD_19200 = 19200,
	HV_UART_BAUD_38400 = 38400,
	HV_UART_BAUD_57600 = 57600,
	HV_UART_BAUD_115200 = 115200,
} hv_uart_baud_t;

/**
 * UART data bits enumeration.
 */
typedef enum {
	HV_UART_DATA_BITS_8 = 8,
	HV_UART_DATA_BITS_7 = 7,
	HV_UART_DATA_BITS_6 = 6,
	HV_UART_DATA_BITS_5 = 5,
} hv_uart_data_bits_t;

/**
 * UART stop bits enumeration.
 */
typedef enum {
	HV_UART_STOP_BITS_1 = 1,
	HV_UART_STOP_BITS_2 = 2,
} hv_uart_stop_bits_t;

/**
 * UART parity mode enumeration.
 */
typedef enum {
	HV_UART_PARITY_NONE = 0,
	HV_UART_PARITY_ODD = 1,
	HV_UART_PARITY_EVEN = 2,
} hv_uart_parity_t;

/**
 * UART I/O mode.
 */
typedef enum {
	HV_UART_MODE_POLLED = 0, /**< Polled I/O (blocking) */
	HV_UART_MODE_INTERRUPT = 1, /**< Interrupt-driven I/O */
	HV_UART_MODE_DMA = 2, /**< DMA-based transfers */
} hv_uart_mode_t;

/**
 * UART configuration structure.
 */
typedef struct {
	hv_uart_baud_t baud;
	hv_uart_data_bits_t data_bits;
	hv_uart_stop_bits_t stop_bits;
	hv_uart_parity_t parity;
	hv_uart_mode_t mode;
	int rts_cts_enabled; /**< Hardware flow control */
} hv_uart_config_t;

/**
 * UART statistics.
 */
typedef struct {
	hv_u64 bytes_sent;
	hv_u64 bytes_received;
	hv_u64 transmit_errors;
	hv_u64 receive_errors;
	hv_u64 overrun_errors;
} hv_uart_stats_t;

/**
 * Initialize guest UART subsystem.
 *
 * Must be called once at hypervisor startup before any UART access.
 *
 * @return HV_OK on success
 * @return HV_ENOTSUP if UART not available on platform
 */
hv_status_t hv_guest_uart_init(void);

/**
 * Allocate UART port for a guest partition.
 *
 * Reserves a UART port for exclusive use by partition.
 *
 * @param partition  Partition ID to own this UART
 * @param port_id    Output: allocated UART port ID
 *
 * @return HV_OK on success
 * @return HV_EINVAL if partition invalid or port_id NULL
 * @return HV_ENOSPC if max UART ports reached
 */
hv_status_t hv_guest_uart_allocate_port(hv_u32 partition, hv_u32 *port_id);

/**
 * Configure UART port.
 *
 * Sets baud rate, data bits, stop bits, parity, and I/O mode.
 *
 * @param port_id  UART port ID (from allocation)
 * @param config   UART configuration
 *
 * @return HV_OK on success
 * @return HV_EINVAL if port_id invalid or config invalid
 * @return HV_EPERM if port not allocated
 * @return HV_ENOTSUP if baud rate not supported by hardware
 */
hv_status_t hv_guest_uart_configure(hv_u32 port_id,
				    const hv_uart_config_t *config);

/**
 * Write data to UART port.
 *
 * Sends data to UART. Behavior depends on configured mode:
 *   - POLLED: Blocks until all data sent
 *   - INTERRUPT: Queues data, returns immediately
 *   - DMA: Queues DMA transfer, returns immediately
 *
 * @param port_id    UART port ID
 * @param data       Data buffer
 * @param length     Number of bytes to send
 * @param bytes_sent Output: number of bytes actually sent (may be < length in non-polled mode)
 *
 * @return HV_OK on success
 * @return HV_EINVAL if port_id or data invalid
 * @return HV_EPERM if port not allocated
 * @return HV_ENOSPC if buffer full (interrupt/DMA mode)
 */
hv_status_t hv_guest_uart_write(hv_u32 port_id, const hv_u8 *data,
				hv_u32 length, hv_u32 *bytes_sent);

/**
 * Read data from UART port.
 *
 * Receives data from UART. Behavior depends on configured mode:
 *   - POLLED: Blocks until data available
 *   - INTERRUPT: Returns available data (may be 0 if buffer empty)
 *   - DMA: Returns available data from DMA buffer
 *
 * @param port_id      UART port ID
 * @param buffer       Output buffer
 * @param max_length   Maximum bytes to read
 * @param bytes_read   Output: number of bytes actually read
 *
 * @return HV_OK on success
 * @return HV_EINVAL if port_id or buffer invalid
 * @return HV_EPERM if port not allocated
 */
hv_status_t hv_guest_uart_read(hv_u32 port_id, hv_u8 *buffer, hv_u32 max_length,
			       hv_u32 *bytes_read);

/**
 * Check if UART port has data ready.
 *
 * Returns immediately (non-blocking).
 *
 * @param port_id UART port ID
 *
 * @return Number of bytes available in receive buffer, or -1 on error
 */
hv_u32 hv_guest_uart_available(hv_u32 port_id);

/**
 * Flush UART transmit buffer.
 *
 * Waits for all pending transmit data to complete.
 * Useful for ensuring data is sent before shutdown.
 *
 * @param port_id UART port ID
 *
 * @return HV_OK on success
 * @return HV_EINVAL if port_id invalid
 * @return HV_EPERM if port not allocated
 */
hv_status_t hv_guest_uart_flush(hv_u32 port_id);

/**
 * Release UART port.
 *
 * Frees UART port for reuse by other partitions.
 * Flushes any pending data first.
 *
 * @param port_id UART port ID
 *
 * @return HV_OK on success
 * @return HV_EINVAL if port_id invalid
 * @return HV_EPERM if port not allocated
 */
hv_status_t hv_guest_uart_release(hv_u32 port_id);

/**
 * Get UART statistics.
 *
 * Returns counters for sent/received bytes and errors.
 *
 * @param port_id UART port ID
 * @param stats   Output: UART statistics
 *
 * @return HV_OK on success
 * @return HV_EINVAL if port_id or stats NULL
 * @return HV_EPERM if port not allocated
 */
hv_status_t hv_guest_uart_get_stats(hv_u32 port_id, hv_uart_stats_t *stats);

/**
 * Reset UART statistics.
 *
 * Clears all statistics counters for a port.
 *
 * @param port_id UART port ID
 *
 * @return HV_OK on success
 * @return HV_EINVAL if port_id invalid
 * @return HV_EPERM if port not allocated
 */
hv_status_t hv_guest_uart_reset_stats(hv_u32 port_id);

#ifdef __cplusplus
}
#endif

#endif /* HAVEN_GUEST_UART_H */
