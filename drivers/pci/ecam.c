/* SPDX-License-Identifier: Apache-2.0 */
/*
 * drivers/pci/ecam.c - minimal PCI function setup through ECAM.
 *
 * Haven, not the partition, places a granted function's memory BAR and
 * turns on memory decoding and bus mastering, so the partition can be given
 * its config page read-only: it cannot move the BAR over another device or
 * re-enable a function Haven left off.  Bus mastering is enabled only after
 * the SMMU enforces the function's DMA window.
 *
 * Config space layout: PCI Local Bus Specification 3.0, 6.1 and 6.2.5
 * (vendor ID at 0x00, command at 0x04, BARs at 0x10 to 0x24; a memory BAR
 * has bit 0 = 0 and type bits [2:1] = 0b10 for 64-bit).
 */

#include "drivers/pci/ecam.h"

#define PCI_VENDOR_ID 0x00
#define PCI_COMMAND 0x04
#define PCI_BAR0 0x10
#define PCI_COMMAND_MEMORY (1U << 1)
#define PCI_COMMAND_MASTER (1U << 2)

static inline uint32_t cfg_read(uintptr_t cfg, uint32_t off)
{
	return *(volatile uint32_t *)(cfg + off);
}

static inline void cfg_write(uintptr_t cfg, uint32_t off, uint32_t val)
{
	*(volatile uint32_t *)(cfg + off) = val;
}

/* Write the command register only (the upper half is the status register,
 * whose bits are write-1-to-clear: write zeros there). */
static void cfg_command(uintptr_t cfg, uint32_t cmd)
{
	cfg_write(cfg, PCI_COMMAND, cmd & 0xffffU);
}

int pci_ecam_present(uintptr_t cfg)
{
	return (cfg_read(cfg, PCI_VENDOR_ID) & 0xffffU) != 0xffffU;
}

hv_status_t pci_ecam_place_bar(uintptr_t cfg, uint32_t bar, uint64_t addr,
			       uint64_t max_size)
{
	uint32_t off = PCI_BAR0 + 4U * bar;
	uint32_t orig, cmd, lo, hi = 0xffffffffU;
	uint64_t size;
	int is64;

	if (bar > 5U)
		return HV_EINVAL;
	orig = cfg_read(cfg, off);
	if (orig & 1U)
		return HV_EINVAL; /* I/O BAR */
	is64 = (orig & 0x6U) == 0x4U;
	if (is64 && bar == 5U)
		return HV_EINVAL;

	/* Size the BAR with decoding off. */
	cmd = cfg_read(cfg, PCI_COMMAND) & 0xffffU;
	cfg_command(cfg, cmd & ~(PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER));
	cfg_write(cfg, off, 0xffffffffU);
	lo = cfg_read(cfg, off) & ~0xfU;
	if (is64) {
		cfg_write(cfg, off + 4U, 0xffffffffU);
		hi = cfg_read(cfg, off + 4U);
	}
	size = ~(((uint64_t)hi << 32) | lo) + 1U;
	if (lo == 0U || size > max_size || (addr & (size - 1U)) != 0U ||
	    (!is64 && (addr >> 32) != 0U)) {
		cfg_write(cfg, off, orig);
		return HV_EINVAL;
	}

	cfg_write(cfg, off, (uint32_t)addr);
	if (is64)
		cfg_write(cfg, off + 4U, (uint32_t)(addr >> 32));
	return HV_OK;
}

void pci_ecam_enable(uintptr_t cfg)
{
	uint32_t cmd = cfg_read(cfg, PCI_COMMAND) & 0xffffU;

	cfg_command(cfg, cmd | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
}
