/* SPDX-License-Identifier: Apache-2.0 */
#ifndef HAVEN_DRIVERS_UART_H
#define HAVEN_DRIVERS_UART_H
#include <stdint.h>
#include <haven/types.h>

void uart_putchar(uintptr_t base, char c);
void uart_puts(uintptr_t base, const char *s);
void uart_init(uintptr_t base, uint32_t baud, uint32_t ref_clk_hz);
#endif
