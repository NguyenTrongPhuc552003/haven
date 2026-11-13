/* SPDX-License-Identifier: Apache-2.0 */
#ifndef HAVEN_BOOT_SHA256_H
#define HAVEN_BOOT_SHA256_H

#include <stdint.h>

void sha256(const void *data, uint64_t n, uint8_t out[32]);

#endif /* HAVEN_BOOT_SHA256_H */
