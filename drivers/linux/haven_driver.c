// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/linux/haven_driver.c — Haven hypervisor guest bridge.
 *
 * Exposes /dev/haven as a character device.  Guest Linux uses this
 * to query hypervisor state and trigger test sequences.
 *
 * Hypercall ABI (ARM64 HVC #0):
 *   x0 = function ID, x1..x7 = args
 *   Return: x0 = status, x1..x3 = result
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include "../guest-tools/haven_ioctl.h"

/* Hypercall function IDs (match drivers/linux/README.md ABI table). */
#define HVC_HAVEN_VERSION  0UL
#define HVC_HAVEN_PART_ID  5UL
#define HVC_HAVEN_TEST     6UL

/* -------------------------------------------------------------------------
 * Hypercall helper
 * -------------------------------------------------------------------------
 * Issues HVC #0 with x0 = func_id and x1 = arg.
 * Returns the value placed in x0 by the hypervisor.
 */
static long haven_hvc(uint64_t func_id, uint64_t arg, uint64_t *result)
{
	register uint64_t status asm("x0") = func_id;
	register uint64_t _arg asm("x1") = arg;

	asm volatile(
		"hvc #0"
		: "+r"(status), "+r"(_arg)
		:
		: "memory", "x2", "x3"
	);

	if (result) {
		*result = _arg;
	}
	return (long)status;
}

/* -------------------------------------------------------------------------
 * File operations
 * -------------------------------------------------------------------------*/

static int haven_open(struct inode *inode, struct file *filp)
{
	pr_debug("haven: device opened\n");
	return 0;
}

static int haven_release(struct inode *inode, struct file *filp)
{
	pr_debug("haven: device closed\n");
	return 0;
}

static long haven_ioctl(struct file *filp, unsigned int cmd, unsigned long uarg)
{
	uint32_t val;
	uint64_t out;
	int ret;

	switch (cmd) {
	case HAVEN_IOCTL_GET_VERSION:
		ret = (int)haven_hvc(HVC_HAVEN_VERSION, 0, &out);
		if (ret < 0)
			return ret;
		val = (uint32_t)out;
		if (copy_to_user((void __user *)uarg, &val, sizeof(val)))
			return -EFAULT;
		pr_info("haven: GET_VERSION => 0x%08x\n", val);
		return 0;

	case HAVEN_IOCTL_GET_PARTITION:
		ret = (int)haven_hvc(HVC_HAVEN_PART_ID, 0, &out);
		if (ret < 0)
			return ret;
		val = (uint32_t)out;
		if (copy_to_user((void __user *)uarg, &val, sizeof(val)))
			return -EFAULT;
		pr_info("haven: GET_PARTITION => %u\n", val);
		return 0;

	case HAVEN_IOCTL_TRIGGER_TEST:
		if (copy_from_user(&val, (const void __user *)uarg, sizeof(val)))
			return -EFAULT;
		ret = (int)haven_hvc(HVC_HAVEN_TEST, (uint64_t)val, &out);
		val = (uint32_t)out;
		pr_info("haven: TRIGGER_TEST result=0x%x status=%d\n", val, ret);
		if (ret < 0)
			return ret;
		if (copy_to_user((void __user *)uarg, &val, sizeof(val)))
			return -EFAULT;
		return 0;

	default:
		return -ENOTTY;
	}
}

/* -------------------------------------------------------------------------
 * Misc device registration
 * -------------------------------------------------------------------------*/

static const struct file_operations haven_fops = {
	.owner          = THIS_MODULE,
	.open           = haven_open,
	.release        = haven_release,
	.unlocked_ioctl = haven_ioctl,
};

static struct miscdevice haven_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "haven",
	.fops  = &haven_fops,
	.mode  = 0600,
};

/* -------------------------------------------------------------------------
 * Module init / exit
 * -------------------------------------------------------------------------*/

static int __init haven_init(void)
{
	int ret;

	ret = misc_register(&haven_miscdev);
	if (ret) {
		pr_err("haven: failed to register misc device (%d)\n", ret);
		return ret;
	}

	pr_info("haven: guest bridge loaded — /dev/haven minor=%d\n",
		haven_miscdev.minor);
	return 0;
}

static void __exit haven_exit(void)
{
	misc_deregister(&haven_miscdev);
	pr_info("haven: guest bridge unloaded\n");
}

module_init(haven_init);
module_exit(haven_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Haven hypervisor guest bridge");
MODULE_AUTHOR("Haven Project");
MODULE_VERSION("0.1");
