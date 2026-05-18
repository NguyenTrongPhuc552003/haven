/* SPDX-License-Identifier: Apache-2.0 */
/* String/memory utilities for Haven.
 *
 * On bare-metal ARM64 builds (HAVEN_ARCH_ARM64 defined): provides hv_mem*
 * declarations backed by src/common/string.c, with compatibility macros
 * aliasing the standard names.
 *
 * On host builds (unit tests, style checks): simply includes <string.h>
 * so tests can link against the system libc without modification.
 */

#ifndef HAVEN_STRING_H
#define HAVEN_STRING_H

#ifdef HAVEN_ARCH_ARM64

#include <stddef.h>
#include <stdint.h>

void  *hv_memset(void *s, int c, size_t n);
void  *hv_memcpy(void *dst, const void *src, size_t n);
void  *hv_memmove(void *dst, const void *src, size_t n);
int    hv_memcmp(const void *a, const void *b, size_t n);
size_t hv_strlen(const char *s);
int    hv_strcmp(const char *a, const char *b);
int    hv_strncmp(const char *a, const char *b, size_t n);

#define memset   hv_memset
#define memcpy   hv_memcpy
#define memmove  hv_memmove
#define memcmp   hv_memcmp
#define strlen   hv_strlen
#define strcmp   hv_strcmp
#define strncmp  hv_strncmp

#else /* host build */

#include <string.h>

#endif /* HAVEN_ARCH_ARM64 */

#endif /* HAVEN_STRING_H */
