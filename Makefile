.PHONY: all build test style-check evidence clean help

# -----------------------------------------------------------------------
# Cross-compile configuration
# Override on command line: make ARCH=arm64 CROSS_COMPILE=aarch64-unknown-linux-gnu-
# Default: host-compiled portable tests (no cross-compile)
# -----------------------------------------------------------------------

ARCH          ?= host
# Auto-detect: prefer aarch64-unknown-linux-gnu- (Linux), fall back to aarch64-elf- (macOS/brew)
ifeq ($(CROSS_COMPILE),)
  ifneq ($(shell which aarch64-unknown-linux-gnu-gcc 2>/dev/null),)
    CROSS_COMPILE := aarch64-unknown-linux-gnu-
  else ifneq ($(shell which aarch64-elf-gcc 2>/dev/null),)
    CROSS_COMPILE := aarch64-elf-
  endif
endif
PLATFORM      ?= qemu-virt

CC      = $(CROSS_COMPILE)gcc
AS      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
AR      = $(CROSS_COMPILE)ar

# -----------------------------------------------------------------------
# Compiler flags
# -----------------------------------------------------------------------

# Common flags shared by host and cross builds
COMMON_CFLAGS = -std=c11 -Wall -Wextra -Werror -Iinclude

# ARM64 cross-compile flags (bare-metal, no stdlib)
ARM64_CFLAGS  = $(COMMON_CFLAGS)            \
                -Iarch/arm64/include         \
                -I.                          \
                -ffreestanding               \
                -fno-builtin                 \
                -fno-stack-protector         \
                -fno-pie                     \
                -nostdlib                    \
                -march=armv8-a               \
                -mgeneral-regs-only          \
                -DHAVEN_ARCH_ARM64           \
                -DHAVEN_PLATFORM_$(shell echo $(PLATFORM) | tr a-z- AZ_)

ARM64_ASFLAGS = -Iarch/arm64/include         \
                -Iinclude                    \
                -D__ASSEMBLY__               \
                -x assembler-with-cpp

# Host-compiled flags (for unit tests, uses libc)
HOST_CFLAGS   = $(COMMON_CFLAGS)

# -----------------------------------------------------------------------
# Source files
# -----------------------------------------------------------------------

# ARM64 architecture layer sources
ARCH_SRCS_ARM64 = \
        arch/arm64/cpu.c      \
        arch/arm64/mm.c       \
        arch/arm64/timer.c    \
        arch/arm64/irq.c

ARCH_ASMS_ARM64 = \
        arch/arm64/entry.S    \
        arch/arm64/context.S  \
        arch/arm64/boot.S     \
        arch/arm64/partition.S

# Hardware driver sources (ARM64 only - GICv3, SMMUv3, and UART)
# UART driver is selected based on PLATFORM to avoid duplicate symbol
# conflicts: both pl011.c and imx_uart.c export uart_putchar/uart_puts/uart_init.
DRIVER_SRCS_ARM64 = \
        drivers/irqchip/gic_v3.c  \
        drivers/iommu/smmu_v3.c

ifeq ($(PLATFORM),imx95-devkit)
DRIVER_SRCS_ARM64 += drivers/uart/imx_uart.c
else
DRIVER_SRCS_ARM64 += drivers/uart/pl011.c
endif

# Platform sources (selected by PLATFORM variable)
PLATFORM_SRCS = \
        src/platform/$(PLATFORM)/platform.c

# Core isolation policy sources (portable - same for host and ARM64)
CORE_SRCS = \
        src/core/mm/stage2.c             \
        src/core/irq/ownership.c         \
        src/core/sched/budget.c          \
        src/core/dma/smmu.c              \
        src/core/iommu/iommu_policy.c    \
        src/core/time/timer.c            \
        src/core/exc/el2_exceptions.c    \
        src/core/partition.c             \
        src/core/init.c

# Common utility sources
COMMON_SRCS = \
        src/common/printk.c  \
        src/common/string.c  \
        src/common/panic.c   \
        src/common/spinlock.c

# Guest driver sources
GUEST_SRCS = \
        src/guest/drivers/uart.c              \
        src/guest/rtos/freertos_integration.c

# -----------------------------------------------------------------------
# Build output directories
# -----------------------------------------------------------------------

BUILD_DIR       = build
OBJ_DIR         = $(BUILD_DIR)/obj
TEST_DIR        = $(BUILD_DIR)/tests
BENCH_DIR       = $(BUILD_DIR)/benchmarks

$(shell mkdir -p $(OBJ_DIR) $(TEST_DIR) $(BENCH_DIR))

# -----------------------------------------------------------------------
# ARM64 build targets (produces haven.elf and haven.bin)
# -----------------------------------------------------------------------

ifeq ($(ARCH),arm64)

CFLAGS  = $(ARM64_CFLAGS)
ASFLAGS = $(ARM64_ASFLAGS)

ARCH_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(ARCH_SRCS_ARM64)) \
            $(patsubst %.S,$(OBJ_DIR)/%.o,$(ARCH_ASMS_ARM64))

DRIVER_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(DRIVER_SRCS_ARM64))

CORE_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))

COMMON_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(COMMON_SRCS))

PLATFORM_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(PLATFORM_SRCS))

GUEST_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(GUEST_SRCS))

ALL_OBJS = $(ARCH_OBJS) $(DRIVER_OBJS) $(CORE_OBJS) $(COMMON_OBJS) $(PLATFORM_OBJS) $(GUEST_OBJS)

all: $(BUILD_DIR)/haven.elf $(BUILD_DIR)/haven.bin $(BUILD_DIR)/guest_a.bin $(BUILD_DIR)/guest_b.bin
	@echo "[arm64] Build complete: $(BUILD_DIR)/haven.bin + $(BUILD_DIR)/guest_a.bin + $(BUILD_DIR)/guest_b.bin"

$(BUILD_DIR)/haven.elf: $(ALL_OBJS) linker.ld
	$(LD) -T linker.ld -o $@ $(ALL_OBJS)
	$(CROSS_COMPILE)size $@

$(BUILD_DIR)/haven.bin: $(BUILD_DIR)/haven.elf
	$(OBJCOPY) -O binary $< $@

# Guest A stub: bare-metal EL1 binary loaded at PA 0x80800000 by qemu-run.sh
$(BUILD_DIR)/guest_a.elf: tests/demos/guest_a_entry.S linker-guest.ld
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c tests/demos/guest_a_entry.S -o $(OBJ_DIR)/guest_a_entry.o
	$(LD) -T linker-guest.ld -o $@ $(OBJ_DIR)/guest_a_entry.o

$(BUILD_DIR)/guest_a.bin: $(BUILD_DIR)/guest_a.elf
	$(OBJCOPY) -O binary $< $@

# Guest B stub: RTOS-class bare-metal EL1 binary at PA 0xA0800000
# Haven maps PA 0xA0800000 → IPA 0x60000000 (PART_B_PA_BASE → PART_B_IPA_BASE).
# Launched on CPU 2 after secondary-CPU bring-up is wired in partition.c.
$(BUILD_DIR)/guest_b.elf: tests/demos/guest_b_entry.S linker-guest-b.ld
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c tests/demos/guest_b_entry.S -o $(OBJ_DIR)/guest_b_entry.o
	$(LD) -T linker-guest-b.ld -o $@ $(OBJ_DIR)/guest_b_entry.o

$(BUILD_DIR)/guest_b.bin: $(BUILD_DIR)/guest_b.elf
	$(OBJCOPY) -O binary $< $@

# Compile C sources
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble .S sources
$(OBJ_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

else
# -----------------------------------------------------------------------
# Host build: portable test archive (original behaviour preserved)
# -----------------------------------------------------------------------

all: build

build:
	./scripts/compile/build.sh

endif

# -----------------------------------------------------------------------
# Targets shared between host and ARM64 builds
# -----------------------------------------------------------------------

test:
	./scripts/dev/test.sh

style-check:
	./scripts/dev/style-check.sh
	./scripts/compile/check-configs.sh

evidence:
	./scripts/evidence/package-evidence.sh

clean:
	rm -rf build out coverage

# -----------------------------------------------------------------------
# ARM64 disassembly (debug aid)
# -----------------------------------------------------------------------

disasm: $(BUILD_DIR)/haven.elf
	$(CROSS_COMPILE)objdump -d -M no-aliases $< | less

qemu-run: $(BUILD_DIR)/haven.bin
	@chmod +x scripts/qemu/qemu-run.sh
	./scripts/qemu/qemu-run.sh

# -----------------------------------------------------------------------
# Help
# -----------------------------------------------------------------------

help:
	@echo "Haven Makefile"
	@echo ""
	@echo "Host build (portable C, for unit tests):"
	@echo "  make                         - build host archive + run tests"
	@echo "  make test                    - run all unit/integration tests"
	@echo "  make style-check             - run style and config checks"
	@echo ""
	@echo "ARM64 cross-compile:"
	@echo "  make ARCH=arm64 CROSS_COMPILE=aarch64-unknown-linux-gnu-"
	@echo "  make ARCH=arm64 CROSS_COMPILE=aarch64-unknown-linux-gnu- PLATFORM=imx95-devkit"
	@echo "  make ARCH=arm64 disasm       - disassemble built binary"
	@echo "  make ARCH=arm64 qemu-run     - launch Haven on QEMU virt (arm64)"
	@echo ""
	@echo "Evidence packaging:"
	@echo "  make evidence                - package test + benchmark evidence"
	@echo ""
	@echo "Clean:"
	@echo "  make clean                   - remove build artifacts"
