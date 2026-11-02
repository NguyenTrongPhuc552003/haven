/* SPDX-License-Identifier: Apache-2.0 */
/*
 * drivers/pci/ecam.h - minimal PCI function setup through ECAM.
 *
 * cfg is the physical address of one function's 4 KiB config page.
 */

#ifndef HAVEN_DRIVERS_PCI_ECAM_H
#define HAVEN_DRIVERS_PCI_ECAM_H

#include <haven/types.h>
#include <stdint.h>

/* 1 if a function answers at cfg (vendor ID is not 0xffff). */
int pci_ecam_present(uintptr_t cfg);

/* Place memory BAR bar at addr, with decoding off.  Fails (HV_EINVAL) for an
 * I/O or missing BAR, a BAR larger than max_size, or a misaligned addr. */
hv_status_t pci_ecam_place_bar(uintptr_t cfg, uint32_t bar, uint64_t addr,
			       uint64_t max_size);

/* Turn on memory decoding and bus mastering. */
void pci_ecam_enable(uintptr_t cfg);

#endif /* HAVEN_DRIVERS_PCI_ECAM_H */
