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

#define HAVEN_MAGIC 'H'
#define HAVEN_IOCTL_GET_VERSION   _IOR(HAVEN_MAGIC, 1, uint32_t)
#define HAVEN_IOCTL_GET_PARTITION _IOR(HAVEN_MAGIC, 2, uint32_t)
#define HAVEN_IOCTL_TRIGGER_TEST  _IOW(HAVEN_MAGIC, 3, uint32_t)

/* Hypercall function IDs */
#define HVC_HAVEN_VERSION  0x48560001UL
#define HVC_HAVEN_PART_ID  0x48560002UL
#define HVC_HAVEN_TEST     0x48560003UL

/* -------------------------------------------------------------------------
 * Hypercall helper
 * -------------------------------------------------------------------------
 * Issues HVC #0 with x0 = func_id and x1 = arg.
 * Returns the value placed in x0 by the hypervisor.
 */
static uint64_t haven_hvc(uint64_t func_id, uint64_t arg)
{
	register uint64_t ret  asm("x0") = func_id;
	register uint64_t _arg asm("x1") = arg;

	asm volatile(
		"hvc #0"
		: "=r"(ret)
		: "r"(ret), "r"(_arg)
		: "memory", "x2", "x3"
	);

	return ret;
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
	int ret;

	switch (cmd) {
	case HAVEN_IOCTL_GET_VERSION:
		val = (uint32_t)haven_hvc(HVC_HAVEN_VERSION, 0);
		if (copy_to_user((void __user *)uarg, &val, sizeof(val)))
			return -EFAULT;
		pr_info("haven: GET_VERSION => 0x%08x\n", val);
		return 0;

	case HAVEN_IOCTL_GET_PARTITION:
		val = (uint32_t)haven_hvc(HVC_HAVEN_PART_ID, 0);
		if (copy_to_user((void __user *)uarg, &val, sizeof(val)))
			return -EFAULT;
		pr_info("haven: GET_PARTITION => %u\n", val);
		return 0;

	case HAVEN_IOCTL_TRIGGER_TEST:
		if (copy_from_user(&val, (const void __user *)uarg, sizeof(val)))
			return -EFAULT;
		ret = (int)haven_hvc(HVC_HAVEN_TEST, (uint64_t)val);
		pr_info("haven: TRIGGER_TEST(0x%x) => %d\n", val, ret);
		return ret < 0 ? ret : 0;

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
	.mode  = 0666,
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
