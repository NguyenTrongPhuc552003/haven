/**
 * Unit tests for guest UART driver.
 *
 * Tests cover:
 *   - UART port allocation
 *   - Configuration (baud rate, data bits, etc.)
 *   - Write operations (polled and buffered modes)
 *   - Read operations
 *   - Buffer availability check
 *   - Statistics tracking
 *
 * @file tests/unit/test_guest_uart.c
 */

#include <haven/guest_uart.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define TEST_PASS(msg) printf("[PASS] %s\n", msg)
#define TEST_FAIL(msg) do { printf("[FAIL] %s\n", msg); assert(0); } while(0)

/**
 * Test: Initialize UART subsystem.
 */
static void test_uart_init(void) {
	hv_status_t status = hv_guest_uart_init();
	assert(status == HV_OK);
	TEST_PASS("uart_init: basic initialization");
}

/**
 * Test: Allocate UART port.
 */
static void test_uart_allocate_port(void) {
	hv_guest_uart_init();

	hv_u32 port1, port2;
	hv_status_t status;

	/* Allocate first port. */
	status = hv_guest_uart_allocate_port(0, &port1);
	assert(status == HV_OK);
	assert(port1 == 0);
	TEST_PASS("uart_allocate_port: first port allocated");

	/* Allocate second port. */
	status = hv_guest_uart_allocate_port(1, &port2);
	assert(status == HV_OK);
	assert(port2 == 1);
	TEST_PASS("uart_allocate_port: second port allocated");

	/* Negative: NULL port_id pointer. */
	status = hv_guest_uart_allocate_port(2, NULL);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_allocate_port: rejects NULL port_id");

	/* Negative: Invalid partition. */
	status = hv_guest_uart_allocate_port(256, &port1);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_allocate_port: rejects invalid partition");
}

/**
 * Test: Configure UART port.
 */
static void test_uart_configure(void) {
	hv_guest_uart_init();

	hv_u32 port;
	hv_status_t status;
	hv_uart_config_t config = {
		.baud = HV_UART_BAUD_115200,
		.data_bits = HV_UART_DATA_BITS_8,
		.stop_bits = HV_UART_STOP_BITS_1,
		.parity = HV_UART_PARITY_NONE,
		.mode = HV_UART_MODE_POLLED,
		.rts_cts_enabled = 0,
	};

	/* Allocate port first. */
	status = hv_guest_uart_allocate_port(0, &port);
	assert(status == HV_OK);

	/* Configure port. */
	status = hv_guest_uart_configure(port, &config);
	assert(status == HV_OK);
	TEST_PASS("uart_configure: basic configuration");

	/* Negative: Invalid port. */
	status = hv_guest_uart_configure(HV_MAX_UART_PORTS + 1, &config);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_configure: rejects invalid port");

	/* Negative: NULL config. */
	status = hv_guest_uart_configure(port, NULL);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_configure: rejects NULL config");

	/* Negative: Unsupported baud rate. */
	config.baud = 999999;
	status = hv_guest_uart_configure(port, &config);
	assert(status == HV_ENOTSUP);
	TEST_PASS("uart_configure: rejects unsupported baud rate");

	/* Negative: Invalid data bits. */
	config.baud = HV_UART_BAUD_115200;
	config.data_bits = 4;  /* Too small */
	status = hv_guest_uart_configure(port, &config);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_configure: rejects invalid data bits");
}

/**
 * Test: Write to UART port.
 */
static void test_uart_write(void) {
	hv_guest_uart_init();

	hv_u32 port, bytes_sent;
	hv_status_t status;
	hv_uart_config_t config = {
		.baud = HV_UART_BAUD_115200,
		.data_bits = HV_UART_DATA_BITS_8,
		.stop_bits = HV_UART_STOP_BITS_1,
		.parity = HV_UART_PARITY_NONE,
		.mode = HV_UART_MODE_POLLED,
		.rts_cts_enabled = 0,
	};

	hv_guest_uart_allocate_port(0, &port);
	hv_guest_uart_configure(port, &config);

	/* Write data in polled mode. */
	hv_u8 data[] = "Hello";
	status = hv_guest_uart_write(port, data, 5, &bytes_sent);
	assert(status == HV_OK);
	assert(bytes_sent == 5);
	TEST_PASS("uart_write: polled write succeeds");

	/* Negative: NULL data. */
	status = hv_guest_uart_write(port, NULL, 5, &bytes_sent);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_write: rejects NULL data");

	/* Negative: NULL bytes_sent. */
	status = hv_guest_uart_write(port, data, 5, NULL);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_write: rejects NULL bytes_sent pointer");

	/* Negative: Invalid port. */
	status = hv_guest_uart_write(HV_MAX_UART_PORTS + 1, data, 5, &bytes_sent);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_write: rejects invalid port");
}

/**
 * Test: Read from UART port.
 */
static void test_uart_read(void) {
	hv_guest_uart_init();

	hv_u32 port, bytes_read;
	hv_status_t status;
	hv_uart_config_t config = {
		.baud = HV_UART_BAUD_115200,
		.data_bits = HV_UART_DATA_BITS_8,
		.stop_bits = HV_UART_STOP_BITS_1,
		.parity = HV_UART_PARITY_NONE,
		.mode = HV_UART_MODE_INTERRUPT,  /* Use interrupt mode for buffering */
		.rts_cts_enabled = 0,
	};

	hv_guest_uart_allocate_port(0, &port);
	hv_guest_uart_configure(port, &config);

	/* Read empty buffer (should return 0 bytes). */
	hv_u8 buffer[64];
	status = hv_guest_uart_read(port, buffer, sizeof(buffer), &bytes_read);
	assert(status == HV_OK);
	assert(bytes_read == 0);
	TEST_PASS("uart_read: empty read returns 0 bytes");

	/* Negative: NULL buffer. */
	status = hv_guest_uart_read(port, NULL, sizeof(buffer), &bytes_read);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_read: rejects NULL buffer");

	/* Negative: NULL bytes_read. */
	status = hv_guest_uart_read(port, buffer, sizeof(buffer), NULL);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_read: rejects NULL bytes_read pointer");

	/* Negative: Invalid port. */
	status = hv_guest_uart_read(HV_MAX_UART_PORTS + 1, buffer, sizeof(buffer), &bytes_read);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_read: rejects invalid port");
}

/**
 * Test: Check UART buffer availability.
 */
static void test_uart_available(void) {
	hv_guest_uart_init();

	hv_u32 port, available;
	hv_uart_config_t config = {
		.baud = HV_UART_BAUD_115200,
		.data_bits = HV_UART_DATA_BITS_8,
		.stop_bits = HV_UART_STOP_BITS_1,
		.parity = HV_UART_PARITY_NONE,
		.mode = HV_UART_MODE_INTERRUPT,
		.rts_cts_enabled = 0,
	};

	hv_guest_uart_allocate_port(0, &port);
	hv_guest_uart_configure(port, &config);

	/* Empty buffer should have 0 available. */
	available = hv_guest_uart_available(port);
	assert(available == 0);
	TEST_PASS("uart_available: empty buffer has 0 bytes");

	/* Negative: Invalid port. */
	available = hv_guest_uart_available(HV_MAX_UART_PORTS + 1);
	assert(available == (hv_u32)-1);
	TEST_PASS("uart_available: returns -1 for invalid port");
}

/**
 * Test: Flush UART transmit buffer.
 */
static void test_uart_flush(void) {
	hv_guest_uart_init();

	hv_u32 port;
	hv_status_t status;

	hv_guest_uart_allocate_port(0, &port);

	status = hv_guest_uart_flush(port);
	assert(status == HV_OK);
	TEST_PASS("uart_flush: flush succeeds");

	/* Negative: Invalid port. */
	status = hv_guest_uart_flush(HV_MAX_UART_PORTS + 1);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_flush: rejects invalid port");
}

/**
 * Test: Release UART port.
 */
static void test_uart_release(void) {
	hv_guest_uart_init();

	hv_u32 port;
	hv_status_t status;

	hv_guest_uart_allocate_port(0, &port);

	status = hv_guest_uart_release(port);
	assert(status == HV_OK);
	TEST_PASS("uart_release: release succeeds");

	/* Negative: Double release. */
	status = hv_guest_uart_release(port);
	assert(status == HV_EPERM);
	TEST_PASS("uart_release: denies double release");

	/* Negative: Invalid port. */
	status = hv_guest_uart_release(HV_MAX_UART_PORTS + 1);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_release: rejects invalid port");
}

/**
 * Test: UART statistics tracking.
 */
static void test_uart_stats(void) {
	hv_guest_uart_init();

	hv_u32 port, bytes_sent;
	hv_uart_stats_t stats;
	hv_status_t status;
	hv_uart_config_t config = {
		.baud = HV_UART_BAUD_115200,
		.data_bits = HV_UART_DATA_BITS_8,
		.stop_bits = HV_UART_STOP_BITS_1,
		.parity = HV_UART_PARITY_NONE,
		.mode = HV_UART_MODE_POLLED,
		.rts_cts_enabled = 0,
	};

	hv_guest_uart_allocate_port(0, &port);
	hv_guest_uart_configure(port, &config);

	/* Get initial stats (should be zero). */
	status = hv_guest_uart_get_stats(port, &stats);
	assert(status == HV_OK);
	assert(stats.bytes_sent == 0);
	assert(stats.bytes_received == 0);
	TEST_PASS("uart_stats: initial stats are zero");

	/* Write some data. */
	hv_u8 data[] = "Test";
	hv_guest_uart_write(port, data, 4, &bytes_sent);

	/* Check stats updated. */
	status = hv_guest_uart_get_stats(port, &stats);
	assert(status == HV_OK);
	assert(stats.bytes_sent == 4);
	TEST_PASS("uart_stats: bytes_sent incremented");

	/* Reset stats. */
	status = hv_guest_uart_reset_stats(port);
	assert(status == HV_OK);

	/* Verify reset. */
	status = hv_guest_uart_get_stats(port, &stats);
	assert(status == HV_OK);
	assert(stats.bytes_sent == 0);
	TEST_PASS("uart_stats: reset clears all counters");

	/* Negative: NULL stats pointer. */
	status = hv_guest_uart_get_stats(port, NULL);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_stats: rejects NULL stats pointer");

	/* Negative: Invalid port. */
	status = hv_guest_uart_get_stats(HV_MAX_UART_PORTS + 1, &stats);
	assert(status == HV_EINVAL);
	TEST_PASS("uart_stats: rejects invalid port");
}

/**
 * Main test runner.
 */
int main(void) {
	printf("\n=== Guest UART Driver Unit Tests ===\n\n");

	test_uart_init();
	test_uart_allocate_port();
	test_uart_configure();
	test_uart_write();
	test_uart_read();
	test_uart_available();
	test_uart_flush();
	test_uart_release();
	test_uart_stats();

	printf("\n=== All guest UART tests passed ===\n\n");
	return 0;
}
