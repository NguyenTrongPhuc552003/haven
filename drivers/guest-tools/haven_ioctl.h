#ifndef HAVEN_GUEST_IOCTL_H
#define HAVEN_GUEST_IOCTL_H

#include <stdint.h>
#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define HAVEN_MAGIC 'H'
#define HAVEN_IOCTL_GET_VERSION _IOR(HAVEN_MAGIC, 1, uint32_t)
#define HAVEN_IOCTL_GET_PARTITION _IOR(HAVEN_MAGIC, 2, uint32_t)
#define HAVEN_IOCTL_TRIGGER_TEST _IOWR(HAVEN_MAGIC, 3, uint32_t)

#endif /* HAVEN_GUEST_IOCTL_H */
