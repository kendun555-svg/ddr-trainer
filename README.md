# RK3576 Open LPDDR4X DDR Trainer
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

An open-source DDR trainer for the Rockchip RK3576, written as a direct replacement for the closed binary blob in the Flipper One boot chain.

---

## What This Is

The RK3576 DDR initialization blob is the last closed binary standing between Flipper One and a fully open boot chain. The Flipper team has an open community task specifically requesting a replacement. This is that replacement.

Four corrected C source files implement a complete open LPDDR4X trainer for the RK3576, with every parameter sourced from the actual blob rather than guessed — the training sequence, register values, impedances, VREF handling, address masks, and boot frequency all match what Rockchip's own binary does.

---

## How It Was Made

The closed DDR blob was reverse-engineered from scratch using Termux:

- Cloned the `rkbin` repository and extracted all tunable parameters from the DDR blob using `ddrbin_tool`
- Disassembled all three blob variants (standard, ultra, and base) using Capstone
- Confirmed the ultra variant contains undocumented code extensions not present in the others
- Cross-referenced all extracted data against the RK3576 TRM to verify register meanings and timing constraints

This produced the complete reference dataset needed to audit and correctly implement the initialization sequence in open C.

---

## What It Does (In order):

1. **Clock setup** — configures the PLL to 528 MHz, a safe startup frequency
2. **Controller and PHY reset release** — brings the DDR controller and PHY out of reset in the correct sequence
3. **Timing register programming** — writes hundreds of timing parameters to the controller and PHY
4. **JEDEC mode register commands** — instructs the DRAM chip to configure its internal impedance and voltage reference
5. **Calibration sweeps** — runs write leveling, gate training, DQ deskew, and VREF training in sequence, each tightening     signal integrity until read/write margins are maximised
6. **BIST pattern test** — runs a hardware memory test to verify the RAM is functional before returning

After this returns successfully, the full DDR address space is available and the normal Linux boot proceeds.

---

## Building

### Prerequisites

An AArch64 bare-metal cross-compiler. On Debian/Ubuntu:

```bash
apt install gcc-aarch64-linux-gnu
```

Or download directly from [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads).
If your toolchain prefix differs from `aarch64-linux-gnu-`, override it:

```bash
make CROSS_COMPILE=aarch64-none-elf-
```

### Compile

```bash
make          # produces rk3576_ddr_train.elf and rk3576_ddr_train.bin
make size     # check it fits in the 256 KiB SRAM budget
make disasm   # optional: disassemble to rk3576_ddr_train.S for inspection
```

The binary loads into System SRAM at `0xFF100000` and must not exceed ~248 KiB
(256 KiB region minus 8 KiB stack).

### Flashing

The `.bin` is loaded by the RK3576 maskrom via `rkdeveloptool`:

```bash
# Put the board into maskrom mode (hold MASKROM button while powering on)
rkdeveloptool db rk3576_loader.bin          # download boot stub
rkdeveloptool wl 0x0 rk3576_ddr_train.bin  # write DDR trainer
rkdeveloptool rd                            # run
```

`rk3576_loader.bin` is the miniloader from the `rkbin` repo — the same one used
during blob extraction. The trainer replaces only the DDR init stage; everything
else in the boot chain is unchanged.

### Adapting for Different DRAM

Geometry is configured in `rk3576_ddr_board.c`. The defaults match an 8 Gb
LPDDR4X die in x32 configuration (4 GiB per channel). If your board differs:

| Field | Meaning |
|-------|---------|
| `col_bits` | Column address bits (typically 10) |
| `row_bits` | Row address bits (14–17 depending on density) |
| `bank_bits` | Banks (3 = 8 banks, standard for LPDDR4X) |
| `size_mb` | Capacity per channel in MiB |
| `ranks` / `cs_map` | Single rank = 1 / 0x1 · dual rank = 2 / 0x3 |

---

## Files

| File | Description |
|------|-------------|
| `rk3576_ddr_train.c` | Entry point, sequencing, ZQ / CA / write-level / gate / deskew / VREF / BIST |
| `rk3576_ddr_board.c` | Board-level config: FSP table, geometry, ODT threshold, `udelay()` |
| `rk3576_ddr.h` | Top-level structs, enums, memory map, public API |
| `rk3576_ddr_regs.h` | Raw register definitions for DDRMC, DDRPHY, DDRGRF, MSCH, CRU |
| `rk3576_ddr_timing.h` | JEDEC timing tables for LPDDR4/4X, LPDDR5, DDR4; lookup function |
| `rk3576_ddr_train.ld` | Linker script — places trainer at SRAM `0xFF100000`, 8 KiB stack |
| `Makefile` | Cross-compile build system (`aarch64-linux-gnu-gcc`, bare-metal flags) |

---

## License

`SPDX-License-Identifier: GPL-2.0-or-later`

Copyright (C) 2026 — see individual source files.

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 2 of the License, or (at your option) any later version.
