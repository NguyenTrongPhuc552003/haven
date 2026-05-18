/* SPDX-License-Identifier: Apache-2.0 */
/* Safe memory and string utility functions for the Haven hypervisor.
 * No libc dependency; all functions are freestanding. */

#include <stddef.h>
#include <stdint.h>

/* -----------------------------------------------------------------------
 * hv_memset - fill memory with a byte value.
 * ----------------------------------------------------------------------- */
void *hv_memset(void *dst, int c, size_t n) {
  uint8_t *p = (uint8_t *)dst;
  uint8_t v = (uint8_t)c;
  while (n--) {
    *p++ = v;
  }
  return dst;
}

/* -----------------------------------------------------------------------
 * hv_memcpy - copy n bytes from src to dst (no overlap assumed).
 * ----------------------------------------------------------------------- */
void *hv_memcpy(void *dst, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  while (n--) {
    *d++ = *s++;
  }
  return dst;
}

/* -----------------------------------------------------------------------
 * hv_memmove - copy n bytes allowing src/dst overlap.
 * ----------------------------------------------------------------------- */
void *hv_memmove(void *dst, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  if (d < s || d >= s + n) {
    while (n--)
      *d++ = *s++;
  } else {
    d += n;
    s += n;
    while (n--)
      *--d = *--s;
  }
  return dst;
}

/* -----------------------------------------------------------------------
 * hv_memcmp - compare n bytes.
 * Returns: 0 if equal, negative if a < b, positive if a > b.
 * ----------------------------------------------------------------------- */
int hv_memcmp(const void *a, const void *b, size_t n) {
  const uint8_t *pa = (const uint8_t *)a;
  const uint8_t *pb = (const uint8_t *)b;
  while (n--) {
    if (*pa != *pb)
      return (int)*pa - (int)*pb;
    pa++;
    pb++;
  }
  return 0;
}

/* -----------------------------------------------------------------------
 * hv_strlen - count bytes in a NUL-terminated string.
 * Bounded to HV_STRING_MAX to prevent runaway in corrupted memory.
 * ----------------------------------------------------------------------- */
#define HV_STRING_MAX 4096

size_t hv_strlen(const char *s) {
  size_t n = 0;
  while (*s && n < HV_STRING_MAX) {
    s++;
    n++;
  }
  return n;
}

/* -----------------------------------------------------------------------
 * hv_strcmp - compare two NUL-terminated strings.
 * ----------------------------------------------------------------------- */
int hv_strcmp(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return (unsigned char)*a - (unsigned char)*b;
}

/* -----------------------------------------------------------------------
 * hv_strncmp - compare up to n characters.
 * ----------------------------------------------------------------------- */
int hv_strncmp(const char *a, const char *b, size_t n) {
  while (n && *a && *a == *b) {
    a++;
    b++;
    n--;
  }
  if (n == 0)
    return 0;
  return (unsigned char)*a - (unsigned char)*b;
}

/* -----------------------------------------------------------------------
 * hv_uitoa_hex - convert unsigned 64-bit to hex string (no leading zeros).
 * buf must be at least 17 bytes (16 hex digits + NUL).
 * Returns pointer to start of string within buf.
 * ----------------------------------------------------------------------- */
char *hv_uitoa_hex(uint64_t val, char *buf, size_t bufsz) {
  static const char hex[] = "0123456789abcdef";
  if (bufsz < 17)
    return buf;

  char tmp[17];
  int i = 0;

  if (val == 0) {
    tmp[i++] = '0';
  } else {
    uint64_t v = val;
    while (v && i < 16) {
      tmp[i++] = hex[v & 0xf];
      v >>= 4;
    }
  }

  /* Reverse into buf */
  int j = 0;
  while (i > 0)
    buf[j++] = tmp[--i];
  buf[j] = '\0';
  return buf;
}

/* -----------------------------------------------------------------------
 * hv_uitoa_dec - convert unsigned 64-bit to decimal string.
 * buf must be at least 21 bytes.
 * ----------------------------------------------------------------------- */
char *hv_uitoa_dec(uint64_t val, char *buf, size_t bufsz) {
  if (bufsz < 21)
    return buf;

  char tmp[21];
  int i = 0;

  if (val == 0) {
    tmp[i++] = '0';
  } else {
    uint64_t v = val;
    while (v && i < 20) {
      tmp[i++] = (char)('0' + (v % 10));
      v /= 10;
    }
  }

  int j = 0;
  while (i > 0)
    buf[j++] = tmp[--i];
  buf[j] = '\0';
  return buf;
}
