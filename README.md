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

## Files

|      File      |                    Description                       |
|----------------|------------------------------------------------------|
| `ddr_init.c`   | Main initialization entry point and sequencing       |
| `ddr_phy.c`    | PHY configuration and calibration routines           |
| `ddr_timing.c` | Timing parameter tables derived from blob extraction |
| `ddr_bist.c`   | Hardware BIST pattern test                           |

---

## License

`SPDX-License-Identifier: GPL-2.0-or-later`

Copyright (C) 2026 — see individual source files.

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 2 of the License, or (at your option) any later version.
