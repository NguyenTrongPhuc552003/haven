/* SPDX-License-Identifier: Apache-2.0 */
/*
 * /init for the interference pipeline with Linux in partition A.
 *
 * Freestanding arm64 Linux program (no libc, raw system calls), built
 * once per load with -DLOAD_MODE=0 (idle: sleeps), 1 (cpu: a busy loop)
 * or 2 (mem: writes and reads a 64 MiB buffer, 64 bytes apart). It prints
 * one line and then runs its load forever, so partition A is still
 * loaded when partition B's latency probe starts (4 s after power-on).
 */

#define NR_WRITE 64
#define NR_NANOSLEEP 101
#define NR_MMAP 222
#define MEM_BYTES (64UL << 20)

#ifndef LOAD_MODE
#define LOAD_MODE 0
#endif

static long sys6(long nr, long a, long b, long c, long d, long e, long f)
{
	register long x8 __asm__("x8") = nr;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x4 __asm__("x4") = e;
	register long x5 __asm__("x5") = f;

	__asm__ volatile("svc #0"
			 : "+r"(x0)
			 : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
			 : "memory");
	return x0;
}

static void say(const char *s)
{
	long n = 0;

	while (s[n])
		n++;
	sys6(NR_WRITE, 1, (long)s, n, 0, 0, 0);
}

void _start(void)
{
#if LOAD_MODE == 0
	static const long one_second[2] = {1, 0};

	say("LOAD-INIT: mode idle running\n");
	for (;;)
		sys6(NR_NANOSLEEP, (long)one_second, 0, 0, 0, 0, 0);
#elif LOAD_MODE == 1
	volatile unsigned long n = 0;

	say("LOAD-INIT: mode cpu running\n");
	for (;;)
		n++;
#else
	/* PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS */
	volatile unsigned long *buf = (volatile unsigned long *)sys6(
		NR_MMAP, 0, MEM_BYTES, 3, 0x22, -1, 0);
	unsigned long i;
	volatile unsigned long sum = 0;

	if ((long)buf < 0 && (long)buf > -4096) {
		say("LOAD-INIT: mmap failed\n");
		for (;;)
			;
	}
	say("LOAD-INIT: mode mem running\n");
	for (;;) {
		for (i = 0; i < MEM_BYTES / 8; i += 8)
			buf[i] = i;
		for (i = 0; i < MEM_BYTES / 8; i += 8)
			sum += buf[i];
	}
#endif
}
