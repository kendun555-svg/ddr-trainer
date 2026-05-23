/*
 * rk3576_ddr_train.ld — Linker script
 *
 * The DDR trainer runs entirely from the RK3576 internal SRAM.
 * RK3576 SRAM layout (from TRM):
 *   0xFF000000 – 0xFF07FFFF   512 KiB  PMU SRAM (not used here)
 *   0xFF100000 – 0xFF13FFFF   256 KiB  System SRAM (first-stage boot)
 *
 * We place the trainer at 0xFF100000.
 * Stack grows down from the top of this SRAM region (0xFF140000).
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

SRAM_BASE = 0xFF100000;
SRAM_SIZE = 256K;
STACK_SIZE = 8K;

MEMORY {
    SRAM (rwx) : ORIGIN = SRAM_BASE, LENGTH = SRAM_SIZE
}

SECTIONS {
    . = SRAM_BASE;

    .text : {
        *(.text.entry)      /* entry point first */
        *(.text*)
        *(.rodata*)
    } > SRAM

    . = ALIGN(16);
    .data : {
        *(.data*)
    } > SRAM

    . = ALIGN(16);
    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > SRAM

    . = ALIGN(16);
    __stack_top = SRAM_BASE + SRAM_SIZE;

    /* Sanity: ensure we fit */
    ASSERT(. <= __stack_top - STACK_SIZE,
           "DDR trainer too large for SRAM!")
}
