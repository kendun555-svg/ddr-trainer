# SPDX-License-Identifier: GPL-2.0-or-later
#
# Makefile — RK3576 DDR Trainer
#
# Cross-compile for AArch64 bare-metal (FSBL / SRAM execution).
# Requires an AArch64 GNU toolchain, e.g.:
#   apt install gcc-aarch64-linux-gnu
#   or download from https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
#
# Usage:
#   make          — build rk3576_ddr_train.elf and .bin
#   make clean    — remove build artefacts
#   make size     — show section sizes
#   make disasm   — disassemble the ELF

CROSS_COMPILE ?= aarch64-linux-gnu-
CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE    := $(CROSS_COMPILE)size

# Target: AArch64, no FPU/SIMD in SRAM context, no stdlib
ARCH_FLAGS := -march=armv8-a -mtune=cortex-a72 -mgeneral-regs-only

CFLAGS  := $(ARCH_FLAGS)          \
            -O2                    \
            -ffreestanding         \
            -fno-builtin           \
            -fno-stack-protector   \
            -fno-common            \
            -Wall                  \
            -Wextra                \
            -Werror                \
            -Wmissing-prototypes   \
            -std=c11               \
            -nostdinc              \
            -I.

LDFLAGS := -nostdlib                 \
            -T rk3576_ddr_train.ld   \
            -Map rk3576_ddr_train.map

SRCS    := rk3576_ddr_train.c \
            rk3576_ddr_board.c

OBJS    := $(SRCS:.c=.o)

TARGET  := rk3576_ddr_train

all: $(TARGET).bin

$(TARGET).elf: $(OBJS) rk3576_ddr_train.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: size disasm clean

size: $(TARGET).elf
	$(SIZE) $<

disasm: $(TARGET).elf
	$(OBJDUMP) -d -S $< > $(TARGET).S

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).map $(TARGET).S
