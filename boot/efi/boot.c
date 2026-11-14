/* SPDX-License-Identifier: Apache-2.0 */
/* haven-boot.efi: load HAVEN and its partition images at their fixed
 * addresses, leave UEFI and enter HAVEN at EL2 with the MMU off (L1b,
 * docs/methodology/stage2/20261109-uefi-loading/README.md, steps 1 to 8;
 * option A2 of docs/porting/cix-p1-loading.md).
 *
 * Position independent with no data needing relocation, like the probe.
 * Until ExitBootServices it prints through the firmware's console; after
 * it, only through the platform's UART. */
#include "efi_lib.h"
#include "layout.h"
#include "sha256.h"

/* handoff.S: masks interrupts, turns SCTLR_EL2.M and C off, prints the jump
 * line and branches to entry with x0 to x3 = 0. */
void haven_handoff(uint64_t entry, uint64_t uart) __attribute__((noreturn));

#define TAG "haven-boot"
#define EBS_TRIES 8

static const struct boot_range ranges[] = {BOOT_RANGES};
#define NR_RANGES (sizeof(ranges) / sizeof(ranges[0]))

static uint8_t map_buf[32768] __attribute__((aligned(8)));

/* ---- UART, for after ExitBootServices (PL011: DR at 0, FR at 0x18) ---- */

static void uart_puts(const char *s)
{
	volatile uint32_t *dr = (volatile uint32_t *)(uintptr_t)BOOT_UART;
	volatile uint32_t *fr =
		(volatile uint32_t *)(uintptr_t)(BOOT_UART + 0x18);

	for (; *s; s++) {
		if (*s == '\n') {
			while (*fr & (1U << 5))
				;
			*dr = '\r';
		}
		while (*fr & (1U << 5)) /* TXFF */
			;
		*dr = (uint8_t)*s;
	}
}

/* ---- memory ---- */

static void zero_range(uint64_t base, uint64_t size)
{
	uint64_t dczid = READ_SYSREG("dczid_el0");
	uint64_t blk = 4UL << (dczid & 0xfU);
	uint64_t a;

	if (!(dczid & (1U << 4)) && !(base % blk) && !(size % blk)) {
		for (a = base; a < base + size; a += blk)
			__asm__ volatile("dc zva, %0" : : "r"(a) : "memory");
		return;
	}
	for (a = base; a < base + size; a += 8)
		*(volatile uint64_t *)(uintptr_t)a = 0;
}

static void clean_range(uint64_t base, uint64_t size)
{
	uint64_t line = 4UL << ((READ_SYSREG("ctr_el0") >> 16) & 0xfU);
	uint64_t a;

	for (a = base & ~(line - 1); a < base + size; a += line)
		__asm__ volatile("dc civac, %0" : : "r"(a) : "memory");
}

static void release(struct efi_boot_services *bs, unsigned int n)
{
	while (n--)
		bs->free_pages(ranges[n].base, ranges[n].size / EFI_PAGE_SIZE);
}

/* ---- files ---- */

static void print_digest(const uint8_t d[32])
{
	char b[65];
	int i;

	for (i = 0; i < 32; i++) {
		b[2 * i] = "0123456789abcdef"[d[i] >> 4];
		b[2 * i + 1] = "0123456789abcdef"[d[i] & 0xfU];
	}
	b[64] = 0;
	con_puts(b);
}

/* Read r->file into r's range; a file larger than the range is an error. */
static efi_status load_file(struct efi_file *root, const struct boot_range *r)
{
	char16 name[sizeof(r->file)];
	struct efi_file *f;
	uint64_t n = r->size, extra = 1;
	uint8_t probe, digest[32];
	efi_status st;
	unsigned int i;

	for (i = 0; i < sizeof(r->file); i++)
		name[i] = (char16)r->file[i];
	st = root->open(root, &f, name, EFI_FILE_MODE_READ, 0);
	if (st != EFI_SUCCESS) {
		con_puts(TAG ": cannot open ");
		con_puts(r->file);
		con_puts("\n");
		return st;
	}
	st = f->read(f, &n, (void *)(uintptr_t)r->base);
	if (st == EFI_SUCCESS && n == r->size)
		st = f->read(f, &extra, &probe);
	else
		extra = 0;
	f->close(f);
	if (st != EFI_SUCCESS || extra) {
		con_puts(TAG ": cannot load ");
		con_puts(r->file);
		con_puts(extra ? " (larger than its range)\n" : "\n");
		return st != EFI_SUCCESS ? st : EFI_BAD_BUFFER_SIZE;
	}
	sha256((const void *)(uintptr_t)r->base, n, digest);
	con_puts(TAG ": load ");
	con_puts(r->file);
	con_puts(" at ");
	con_hex(r->base);
	con_puts(" size ");
	con_hex(n);
	con_puts(" sha256 ");
	print_digest(digest);
	con_puts("\n");
	return EFI_SUCCESS;
}

static efi_status open_root(efi_handle image, struct efi_boot_services *bs,
			    struct efi_file **root,
			    struct efi_loaded_image **li)
{
	const struct efi_guid li_guid = EFI_LOADED_IMAGE_GUID;
	const struct efi_guid fs_guid = EFI_SIMPLE_FILE_SYSTEM_GUID;
	struct efi_simple_file_system *fs;
	efi_status st;

	st = bs->handle_protocol(image, &li_guid, (void **)li);
	if (st == EFI_SUCCESS)
		st = bs->handle_protocol((*li)->device_handle, &fs_guid,
					 (void **)&fs);
	if (st == EFI_SUCCESS)
		st = fs->open_volume(fs, root);
	if (st != EFI_SUCCESS)
		con_puts(TAG ": cannot open the boot volume\n");
	return st;
}

/* ---- main ---- */

/* 2. claim every range at its fixed address (released again on failure) */
static efi_status claim_ranges(struct efi_boot_services *bs)
{
	uint64_t addr;
	efi_status s;
	unsigned int i;

	for (i = 0; i < NR_RANGES; i++) {
		addr = ranges[i].base;
		s = bs->allocate_pages(EFI_ALLOCATE_ADDRESS, ranges[i].type,
				       ranges[i].size / EFI_PAGE_SIZE, &addr);
		if (s != EFI_SUCCESS || addr != ranges[i].base) {
			con_puts(TAG ": cannot claim ");
			con_puts(ranges[i].name);
			con_puts("\n");
			release(bs, i);
			return s != EFI_SUCCESS ? s : EFI_LOAD_ERROR;
		}
		con_puts(TAG ": claim ");
		con_puts(ranges[i].name);
		con_puts(" at ");
		con_hex(ranges[i].base);
		con_puts(" size ");
		con_hex(ranges[i].size);
		con_puts("\n");
	}
	return EFI_SUCCESS;
}

/* 3. zero the ranges; 4. load the images into them */
static efi_status load_ranges(efi_handle image, struct efi_boot_services *bs,
			      struct efi_loaded_image **li)
{
	struct efi_file *root;
	efi_status s;
	unsigned int i;

	for (i = 0; i < NR_RANGES; i++)
		zero_range(ranges[i].base, ranges[i].size);
	s = open_root(image, bs, &root, li);
	for (i = 0; s == EFI_SUCCESS && i < NR_RANGES; i++)
		if (ranges[i].file[0])
			s = load_file(root, &ranges[i]);
	return s;
}

/*
 * 5. leave boot services; after the first call only GetMemoryMap and
 * ExitBootServices may be used. Returns the number of tries, or 0 when the
 * first memory map could not be read (boot services still usable). Does
 * not return when a later try fails: there is nowhere safe to return to.
 */
static unsigned int leave_boot_services(efi_handle image,
					struct efi_boot_services *bs,
					efi_status *err)
{
	uint64_t size, key, dsize;
	uint32_t dver;
	efi_status s = EFI_LOAD_ERROR;
	unsigned int tries;

	con_puts(TAG ": exit-boot-services\n");
	for (tries = 1; tries <= EBS_TRIES; tries++) {
		size = sizeof(map_buf);
		s = bs->get_memory_map(&size,
				       (struct efi_memory_descriptor *)map_buf,
				       &key, &dsize, &dver);
		if (s == EFI_SUCCESS)
			s = bs->exit_boot_services(image, key);
		if (s == EFI_SUCCESS)
			return tries;
		if (tries == 1 && s != EFI_INVALID_PARAMETER) {
			con_puts(TAG ": no memory map\n");
			*err = s;
			return 0;
		}
	}
	for (;;) /* boot services may be gone: nowhere safe to return */
		__asm__ volatile("wfe");
}

/* 7. clean every claimed range and this image to the point of coherency */
static void clean_all(uint64_t self_base, uint64_t self_size)
{
	unsigned int i;

	for (i = 0; i < NR_RANGES; i++)
		clean_range(ranges[i].base, ranges[i].size);
	clean_range(self_base, self_size);
	__asm__ volatile("dsb sy\n\tic ialluis\n\tdsb sy\n\tisb"
			 :
			 :
			 : "memory");
}

efi_status efi_main(efi_handle image, struct efi_system_table *st)
{
	struct efi_boot_services *bs = st->boot_services;
	uint64_t el = (READ_SYSREG("CurrentEL") >> 2) & 3U;
	struct efi_loaded_image *li;
	uint64_t self_base = 0, self_size = 0;
	efi_status s;
	unsigned int tries = 0;
	char line[] = TAG ": exit-boot-services tries 0\n";

	con_init(st->con_out);
	con_puts(TAG ": HAVEN-BOOT 1\n");
	con_kv(TAG, "CurrentEL", el);
	if (el != 2U)
		return EFI_UNSUPPORTED;

	s = claim_ranges(bs);
	if (s != EFI_SUCCESS)
		return s;
	s = load_ranges(image, bs, &li);
	if (s == EFI_SUCCESS) {
		/* read before boot services go: the protocol is theirs */
		self_base = (uint64_t)(uintptr_t)li->image_base;
		self_size = li->image_size;
		tries = leave_boot_services(image, bs, &s);
	}
	if (s != EFI_SUCCESS || tries == 0) {
		release(bs, NR_RANGES);
		return s != EFI_SUCCESS ? s : EFI_LOAD_ERROR;
	}

	/* 6. from here on, the UART only */
	line[sizeof(line) - 3] = (char)('0' + tries);
	uart_puts(line);
	clean_all(self_base, self_size);

	/* 8. MMU and data cache off, then HAVEN */
	haven_handoff(BOOT_ENTRY, BOOT_UART);
}
