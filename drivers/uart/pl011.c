/* SPDX-License-Identifier: Apache-2.0 */
/*
 * drivers/uart/pl011.c — ARM PrimeCell PL011 UART driver.
 * Used by: src/platform/qemu-virt/platform.c
 * Reference: ARM DDI 0183G — PrimeCell UART PL011
 *
 * Register map (relative to MMIO base):
 *   UARTDR    0x000  Data register (write = TX, read = RX)
 *   UARTFR    0x018  Flag register: bit 5 = TXFF (TX FIFO full), bit 3 = BUSY
 *   UARTIBRD  0x024  Integer baud rate divisor
 *   UARTFBRD  0x028  Fractional baud rate divisor
 *   UARTLCR_H 0x02C  Line control: FEN=bit4, WLEN=bits[6:5]
 *   UARTCR    0x030  Control: UARTEN=bit0, TXE=bit8, RXE=bit9
 */

#include <stdint.h>
#include "drivers/uart/uart.h"

/* -----------------------------------------------------------------------
 * Register offsets
 * ----------------------------------------------------------------------- */

#define UARTDR          0x000u
#define UARTFR          0x018u
#define UARTIBRD        0x024u
#define UARTFBRD        0x028u
#define UARTLCR_H       0x02Cu
#define UARTCR          0x030u

/* Flag register bits */
#define UARTFR_TXFF     (1u << 5)   /* TX FIFO full  */
#define UARTFR_BUSY     (1u << 3)   /* UART busy     */

/* Line-control register bits */
#define UARTLCR_H_FEN   (1u << 4)           /* FIFO enable */
#define UARTLCR_H_WLEN8 (3u << 5)           /* 8-bit word length */
#define UARTLCR_H_8N1   (UARTLCR_H_WLEN8 | UARTLCR_H_FEN)

/* Control register bits */
#define UARTCR_UARTEN   (1u << 0)
#define UARTCR_TXE      (1u << 8)
#define UARTCR_RXE      (1u << 9)
#define UARTCR_ENABLE   (UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE)

/* -----------------------------------------------------------------------
 * MMIO accessors
 * ----------------------------------------------------------------------- */

static inline uint32_t pl011_read(uintptr_t base, uint32_t offset)
{
        return *(volatile uint32_t *)(base + offset);
}

static inline void pl011_write(uintptr_t base, uint32_t offset, uint32_t val)
{
        *(volatile uint32_t *)(base + offset) = val;
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

void uart_putchar(uintptr_t base, char c)
{
        /* Spin until TX FIFO has space */
        while (pl011_read(base, UARTFR) & UARTFR_TXFF)
                __asm__ volatile("yield");

        pl011_write(base, UARTDR, (uint32_t)(unsigned char)c);
}

void uart_puts(uintptr_t base, const char *s)
{
        while (*s)
                uart_putchar(base, *s++);
}

void uart_init(uintptr_t base, uint32_t baud, uint32_t ref_clk_hz)
{
        /*
         * Baud rate divisor = UARTCLK / (16 * baud_rate).
         * Work in units of 1/64 to derive both IBRD and FBRD in one step:
         *   BRD_64 = UARTCLK * 4 / baud_rate   (rounded to nearest)
         *   IBRD   = BRD_64 / 64
         *   FBRD   = BRD_64 % 64
         */
        uint32_t brd_64 = (ref_clk_hz * 4u + baud / 2u) / baud;
        uint32_t ibrd   = brd_64 / 64u;
        uint32_t fbrd   = brd_64 % 64u;

        /* 1. Disable UART */
        pl011_write(base, UARTCR, 0u);

        /* 2. Wait for any in-progress transmission to complete */
        while (pl011_read(base, UARTFR) & UARTFR_BUSY)
                __asm__ volatile("yield");

        /* 3. Flush FIFOs (clear FEN) */
        pl011_write(base, UARTLCR_H, 0u);

        /* 4. Program baud rate divisors */
        pl011_write(base, UARTIBRD, ibrd);
        pl011_write(base, UARTFBRD, fbrd);

        /* 5. Set 8N1 with FIFO enabled */
        pl011_write(base, UARTLCR_H, UARTLCR_H_8N1);

        /* 6. Enable UART, TX, and RX */
        pl011_write(base, UARTCR, UARTCR_ENABLE);
}
