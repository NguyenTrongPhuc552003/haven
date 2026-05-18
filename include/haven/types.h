#ifndef HAVEN_TYPES_H
#define HAVEN_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef uint64_t hv_u64;
typedef uint32_t hv_u32;
typedef uint16_t hv_u16;
typedef uint8_t hv_u8;

typedef int32_t hv_status_t;

enum
{
    HV_OK = 0,
    HV_EINVAL = -22,
    HV_EPERM = -1,
    HV_ENOMEM = -12,
    HV_ENOSPC = -28,
    HV_ENOTSUP = -95
};

#endif
