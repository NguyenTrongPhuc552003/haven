/* SPDX-License-Identifier: Apache-2.0 */
/*
 * drivers/uart/imx_uart.c - NXP i.MX LPUART driver.
 * Used by: src/platform/imx95-devkit/platform.c
 * Reference: NXP i.MX95 Reference Manual, LPUART chapter
 *
 * Register map (relative to MMIO base):
 *   LPUART_BAUD  0x10  Baud rate: OSR[28:24], SBR[12:0]
 *   LPUART_STAT  0x14  Status: bit 23 = TDRE (TX data register empty)
 *   LPUART_CTRL  0x18  Control: bit 19 = TE (transmitter enable)
 *   LPUART_DATA  0x1C  Data register (write = TX)
 *
 * Note: TDRE is at STAT bit 23 per NXP i.MX95 RM.  This driver targets
 * the i.MX95 Dev Kit where LPUART1 (base 0x44380000) is the debug console.
 */

#include "drivers/uart/uart.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Register offsets
 * ----------------------------------------------------------------------- */

#define LPUART_BAUD 0x10u
#define LPUART_STAT 0x14u
#define LPUART_CTRL 0x18u
#define LPUART_DATA 0x1Cu

/* STAT register bits */
#define LPUART_STAT_TDRE (1u << 23) /* Transmit Data Register Empty */

/* CTRL register bits */
#define LPUART_CTRL_TE (1u << 19) /* Transmitter Enable */

/* BAUD register fields */
#define LPUART_BAUD_OSR_SHIFT 24u
#define LPUART_BAUD_OSR_MASK 0x1fu
#define LPUART_BAUD_SBR_MASK 0x1fffu

/* -----------------------------------------------------------------------
 * MMIO accessors
 * ----------------------------------------------------------------------- */

static inline uint32_t imx_read(uintptr_t base, uint32_t offset) {
  return *(volatile uint32_t *)(base + offset);
}

static inline void imx_write(uintptr_t base, uint32_t offset, uint32_t val) {
  *(volatile uint32_t *)(base + offset) = val;
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

void uart_putchar(uintptr_t base, char c) {
  /* Spin until TX data register is empty */
  while (!(imx_read(base, LPUART_STAT) & LPUART_STAT_TDRE))
    __asm__ volatile("yield");

  imx_write(base, LPUART_DATA, (uint32_t)(unsigned char)c);
}

void uart_puts(uintptr_t base, const char *s) {
  while (*s)
    uart_putchar(base, *s++);
}

void uart_init(uintptr_t base, uint32_t baud, uint32_t ref_clk_hz) {
  /*
   * OSR = 15 selects 16x oversampling (actual ratio = OSR + 1).
   * SBR = ref_clk_hz / ((OSR + 1) * baud_rate), rounded.
   */
  uint32_t osr = 15u;
  uint32_t sbr =
      (ref_clk_hz + (((osr + 1u) * baud) / 2u)) / ((osr + 1u) * baud);

  /* Program baud rate: OSR[28:24] | SBR[12:0] */
  imx_write(base, LPUART_BAUD,
            ((osr & LPUART_BAUD_OSR_MASK) << LPUART_BAUD_OSR_SHIFT) |
                (sbr & LPUART_BAUD_SBR_MASK));

  /* Enable transmitter */
  uint32_t ctrl = imx_read(base, LPUART_CTRL);
  ctrl |= LPUART_CTRL_TE;
  imx_write(base, LPUART_CTRL, ctrl);
}
