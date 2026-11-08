// SPDX-License-Identifier: Apache-2.0
/*
 * /init for SMP3 (README.md): CPU hotplug of vCPU 1, then what the KDAL CI
 * init does (kdal/scripts/ci/kunit-init.c): load /kdal.ko, print KDAL's
 * demand between markers, power off.
 *
 * Freestanding arm64 Linux program: no libc, raw system calls. Sysfs is
 * mounted at /d (the only directory mkcpio.py creates), cpu1/online is
 * written 0, the online list printed after HOTPLUG-OFF, a 3 s pause (a
 * read point for read-back), then 1 and HOTPLUG-ON. /d is unmounted again
 * before debugfs goes there.
 */

#define AT_FDCWD -100
#define O_RDONLY 0
#define O_WRONLY 1
#define NR_UMOUNT2 39
#define NR_MOUNT 40
#define NR_OPENAT 56
#define NR_CLOSE 57
#define NR_READ 63
#define NR_WRITE 64
#define NR_NANOSLEEP 101
#define NR_REBOOT 142
#define NR_FINIT_MODULE 273
#define REBOOT_MAGIC1 0xfee1dead
#define REBOOT_MAGIC2 672274793
#define REBOOT_POWER_OFF 0x4321fedc

static long sys5(long nr, long a, long b, long c, long d, long e)
{
	register long x8 __asm__("x8") = nr;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x4 __asm__("x4") = e;

	__asm__ volatile("svc #0"
			 : "+r"(x0)
			 : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4)
			 : "memory");
	return x0;
}

#define sys3(nr, a, b, c) sys5(nr, a, b, c, 0, 0)
#define sys4(nr, a, b, c, d) sys5(nr, a, b, c, d, 0)

static void say(const char *s)
{
	long n = 0;

	while (s[n])
		n++;
	sys3(NR_WRITE, 1, (long)s, n);
}

static void cat(const char *path)
{
	static char buf[256];
	long fd = sys4(NR_OPENAT, AT_FDCWD, (long)path, O_RDONLY, 0), n;

	if (fd < 0) {
		say("?\n");
		return;
	}
	while ((n = sys3(NR_READ, fd, (long)buf, sizeof(buf))) > 0)
		sys3(NR_WRITE, 1, (long)buf, n);
	sys3(NR_CLOSE, fd, 0, 0);
}

static long put(const char *path, const char *v)
{
	long fd = sys4(NR_OPENAT, AT_FDCWD, (long)path, O_WRONLY, 0), r;

	if (fd < 0)
		return fd;
	r = sys3(NR_WRITE, fd, (long)v, 1);
	sys3(NR_CLOSE, fd, 0, 0);
	return r;
}

static void hotplug(void)
{
	static const long pause[2] = {3, 0}; /* struct timespec: 3 s */
	const char *ctl = "/d/devices/system/cpu/cpu1/online";

	if (sys5(NR_MOUNT, (long)"sysfs", (long)"/d", (long)"sysfs", 0, 0) !=
	    0) {
		say("HOTPLUG: cannot mount sysfs\n");
		return;
	}
	say(put(ctl, "0") == 1 ? "HOTPLUG-OFF ok, online: " :
				 "HOTPLUG-OFF failed, online: ");
	cat("/d/devices/system/cpu/online");
	sys3(NR_NANOSLEEP, (long)pause, 0, 0);
	say(put(ctl, "1") == 1 ? "HOTPLUG-ON ok, online: " :
				 "HOTPLUG-ON failed, online: ");
	cat("/d/devices/system/cpu/online");
	sys3(NR_UMOUNT2, (long)"/d", 0, 0);
}

static void print_demand(void)
{
	static char buf[8192];
	long fd, n;

	if (sys5(NR_MOUNT, (long)"debugfs", (long)"/d", (long)"debugfs", 0,
		 0) != 0) {
		say("KDAL-CI: cannot mount debugfs\n");
		return;
	}
	fd = sys4(NR_OPENAT, AT_FDCWD, (long)"/d/kdal/demand", O_RDONLY, 0);
	if (fd < 0) {
		say("KDAL-CI: no /d/kdal/demand\n");
		return;
	}
	say("KDAL-DEMAND-BEGIN\n");
	while ((n = sys3(NR_READ, fd, (long)buf, sizeof(buf))) > 0)
		sys3(NR_WRITE, 1, (long)buf, n);
	say("KDAL-DEMAND-END\n");
	sys3(NR_CLOSE, fd, 0, 0);
}

void _start(void)
{
	long fd, ret;

	hotplug();
	fd = sys4(NR_OPENAT, AT_FDCWD, (long)"/kdal.ko", O_RDONLY, 0);
	if (fd < 0) {
		say("KDAL-CI: cannot open /kdal.ko\n");
	} else {
		ret = sys3(NR_FINIT_MODULE, fd, (long)"", 0);
		say(ret == 0 ? "KDAL-CI: insmod ok\n" :
			       "KDAL-CI: insmod failed\n");
		if (ret == 0)
			print_demand();
	}
	sys4(NR_REBOOT, REBOOT_MAGIC1, REBOOT_MAGIC2, REBOOT_POWER_OFF, 0);
	for (;;)
		;
}
