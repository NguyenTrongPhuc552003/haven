/**
 * Linux guest UART driver implementation.
 *
 * @file src/guest/drivers/uart.c
 */

#include <haven/guest_uart.h>
#include <haven/string.h>

/**
 * Per-UART port state.
 */
typedef struct {
	hv_u32 partition;
	hv_uart_config_t config;
	hv_uart_stats_t stats;
	int allocated;
	hv_u8 tx_buffer[4096];
	hv_u32 tx_head;
	hv_u32 tx_tail;
	hv_u8 rx_buffer[4096];
	hv_u32 rx_head;
	hv_u32 rx_tail;
} hv_uart_port_t;

/**
 * UART port array.
 */
static hv_uart_port_t uart_ports[HV_MAX_UART_PORTS] = {0};

/**
 * Port count.
 */
static hv_u16 uart_port_count = 0;

hv_status_t hv_guest_uart_init(void)
{
	memset(uart_ports, 0, sizeof(uart_ports));
	uart_port_count = 0;
	return HV_OK;
}

hv_status_t hv_guest_uart_allocate_port(hv_u32 partition, hv_u32 *port_id)
{
	if (port_id == NULL || partition >= 256) {
		return HV_EINVAL;
	}

	/* Find first available port. */
	for (hv_u32 i = 0; i < HV_MAX_UART_PORTS; i++) {
		if (!uart_ports[i].allocated) {
			uart_ports[i].partition = partition;
			uart_ports[i].allocated = 1;
			uart_port_count++;
			*port_id = i;
			return HV_OK;
		}
	}

	return HV_ENOSPC;
}

hv_status_t hv_guest_uart_configure(hv_u32 port_id,
				    const hv_uart_config_t *config)
{
	if (port_id >= HV_MAX_UART_PORTS || config == NULL) {
		return HV_EINVAL;
	}

	if (!uart_ports[port_id].allocated) {
		return HV_EPERM;
	}

	/* Validate baud rate support. */
	if (config->baud != HV_UART_BAUD_9600 &&
	    config->baud != HV_UART_BAUD_19200 &&
	    config->baud != HV_UART_BAUD_38400 &&
	    config->baud != HV_UART_BAUD_57600 &&
	    config->baud != HV_UART_BAUD_115200) {
		return HV_ENOTSUP;
	}

	/* Validate data bits. */
	if (config->data_bits < 5 || config->data_bits > 8) {
		return HV_EINVAL;
	}

	/* Store configuration. */
	memcpy(&uart_ports[port_id].config, config, sizeof(*config));

	return HV_OK;
}

hv_status_t hv_guest_uart_write(hv_u32 port_id, const hv_u8 *data,
				hv_u32 length, hv_u32 *bytes_sent)
{
	if (port_id >= HV_MAX_UART_PORTS || data == NULL ||
	    bytes_sent == NULL) {
		return HV_EINVAL;
	}

	if (!uart_ports[port_id].allocated) {
		return HV_EPERM;
	}

	hv_uart_port_t *port = &uart_ports[port_id];
	*bytes_sent = 0;

	if (port->config.mode == HV_UART_MODE_POLLED) {
		/* Polled mode: immediately update stats and claim success. */
		*bytes_sent = length;
		port->stats.bytes_sent += length;
	} else if (port->config.mode == HV_UART_MODE_INTERRUPT ||
		   port->config.mode == HV_UART_MODE_DMA) {
		/* Interrupt/DMA mode: queue data into buffer. */
		hv_u32 available = (port->tx_head >= port->tx_tail) ?
					   (sizeof(port->tx_buffer) -
					    port->tx_head + port->tx_tail) :
					   (port->tx_tail - port->tx_head);

		if (available < length) {
			return HV_ENOSPC; /* Buffer full */
		}

		/* Copy data to buffer. */
		if (port->tx_head + length > sizeof(port->tx_buffer)) {
			hv_u32 first_part =
				sizeof(port->tx_buffer) - port->tx_head;
			memcpy(&port->tx_buffer[port->tx_head], data,
			       first_part);
			memcpy(port->tx_buffer, &data[first_part],
			       length - first_part);
		} else {
			memcpy(&port->tx_buffer[port->tx_head], data, length);
		}

		port->tx_head =
			(port->tx_head + length) % sizeof(port->tx_buffer);
		*bytes_sent = length;
		port->stats.bytes_sent += length;
	}

	return HV_OK;
}

hv_status_t hv_guest_uart_read(hv_u32 port_id, hv_u8 *buffer, hv_u32 max_length,
			       hv_u32 *bytes_read)
{
	if (port_id >= HV_MAX_UART_PORTS || buffer == NULL ||
	    bytes_read == NULL) {
		return HV_EINVAL;
	}

	if (!uart_ports[port_id].allocated) {
		return HV_EPERM;
	}

	hv_uart_port_t *port = &uart_ports[port_id];
	*bytes_read = 0;

	/* Calculate available data in buffer. */
	hv_u32 available;
	if (port->rx_head >= port->rx_tail) {
		available = port->rx_head - port->rx_tail;
	} else {
		available =
			sizeof(port->rx_buffer) - port->rx_tail + port->rx_head;
	}

	hv_u32 to_read = (available < max_length) ? available : max_length;

	if (to_read > 0) {
		if (port->rx_tail + to_read > sizeof(port->rx_buffer)) {
			hv_u32 first_part =
				sizeof(port->rx_buffer) - port->rx_tail;
			memcpy(buffer, &port->rx_buffer[port->rx_tail],
			       first_part);
			memcpy(&buffer[first_part], port->rx_buffer,
			       to_read - first_part);
		} else {
			memcpy(buffer, &port->rx_buffer[port->rx_tail],
			       to_read);
		}

		port->rx_tail =
			(port->rx_tail + to_read) % sizeof(port->rx_buffer);
		*bytes_read = to_read;
		port->stats.bytes_received += to_read;
	}

	return HV_OK;
}

hv_u32 hv_guest_uart_available(hv_u32 port_id)
{
	if (port_id >= HV_MAX_UART_PORTS || !uart_ports[port_id].allocated) {
		return (hv_u32)-1;
	}

	hv_uart_port_t *port = &uart_ports[port_id];

	if (port->rx_head >= port->rx_tail) {
		return port->rx_head - port->rx_tail;
	} else {
		return sizeof(port->rx_buffer) - port->rx_tail + port->rx_head;
	}
}

hv_status_t hv_guest_uart_flush(hv_u32 port_id)
{
	if (port_id >= HV_MAX_UART_PORTS) {
		return HV_EINVAL;
	}

	if (!uart_ports[port_id].allocated) {
		return HV_EPERM;
	}

	/**
	 * In real implementation, would:
	 * 1. Wait for transmit buffer to empty
	 * 2. Poll hardware status register for transmission complete
	 */

	uart_ports[port_id].tx_head = 0;
	uart_ports[port_id].tx_tail = 0;

	return HV_OK;
}

hv_status_t hv_guest_uart_release(hv_u32 port_id)
{
	if (port_id >= HV_MAX_UART_PORTS) {
		return HV_EINVAL;
	}

	if (!uart_ports[port_id].allocated) {
		return HV_EPERM;
	}

	/* Flush before release. */
	hv_guest_uart_flush(port_id);

	memset(&uart_ports[port_id], 0, sizeof(uart_ports[0]));
	uart_port_count--;

	return HV_OK;
}

hv_status_t hv_guest_uart_get_stats(hv_u32 port_id, hv_uart_stats_t *stats)
{
	if (port_id >= HV_MAX_UART_PORTS || stats == NULL) {
		return HV_EINVAL;
	}

	if (!uart_ports[port_id].allocated) {
		return HV_EPERM;
	}

	memcpy(stats, &uart_ports[port_id].stats, sizeof(*stats));

	return HV_OK;
}

hv_status_t hv_guest_uart_reset_stats(hv_u32 port_id)
{
	if (port_id >= HV_MAX_UART_PORTS) {
		return HV_EINVAL;
	}

	if (!uart_ports[port_id].allocated) {
		return HV_EPERM;
	}

	memset(&uart_ports[port_id].stats, 0,
	       sizeof(uart_ports[port_id].stats));

	return HV_OK;
}
