// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * rk3576_ddr_board.c — Board-level integration for Flipper One / RK3576
 *
 * Changes from original (blob analysis v1.09):
 *  - freq_mhz now set to FSP0=528 MHz (blob boots at boot_fsp=0, f1=528).
 *    Original used 1600 MHz directly — training cold at full speed is why
 *    most open trainers fail on this SoC. 528 MHz has wide timing margins.
 *  - Added fsp_freq_mhz[] table matching blob: 528 / 1068 / 1560 MHz.
 *  - Added target_freq_mhz=2112 (DFS scales up here; trainer does not).
 *  - Added odt_en_freq_mhz=800 (blob: lp4x_dq_odten_freq_mhz=800).
 *  - Enabled per_bank_ref_en and derate_en (both=1 in blob).
 *  - udelay() now uses ARM Generic Timer (CNTPCT_EL0 @ 24 MHz).
 *    Original used a busy-loop — breaks timing-sensitive ZQ and VREF steps.
 */

#include "rk3576_ddr.h"

/* =========================================================================
 * udelay — ARM Generic Timer implementation
 *
 * RK3576 CNTFRQ_EL0 = 24 MHz (24 ticks per microsecond).
 * This is the correct implementation; the original stub was inaccurate.
 * ========================================================================= */
void udelay(uint32_t us)
{
    uint64_t start, end;

    __asm__ __volatile__("mrs %0, cntpct_el0" : "=r"(start));
    end = start + (uint64_t)us * 24;   /* 24 MHz → 24 ticks/us */
    do {
        __asm__ __volatile__("mrs %0, cntpct_el0" : "=r"(start));
    } while (start < end);
}

/* =========================================================================
 * board_ddr_init — Flipper One / RK3576 DDR configuration
 *
 * Target DRAM: LPDDR4X dual-channel (matches RK3576 EVB and most SBCs).
 * Geometry: 8 Gb per die, x32 per channel, 1 rank, 17-row / 10-col / 3-bank.
 * Replace col_bits/row_bits/bank_bits/size_mb if your DRAM differs.
 *
 * The trainer runs at FSP0 (528 MHz). The DFS driver (in ATF BL31 or kernel)
 * then scales up to the target 2112 MHz at runtime.
 * ========================================================================= */
static ddr_cfg_t ddr_cfg;

int board_ddr_init(void)
{
    int i;

    ddr_cfg_init_defaults(&ddr_cfg);

    /* ---- Dual-channel enable ---- */
    ddr_cfg.active_channels = 2;

    /* ---- Global features from blob params ---- */
    ddr_cfg.per_bank_ref_en = true;   /* per_bank_ref_en=1 */
    ddr_cfg.derate_en       = true;   /* derate_en=1        */

    for (i = 0; i < 2; i++) {
        ddr_chan_cfg_t *ch = &ddr_cfg.chan[i];

        ch->enabled = true;
        ch->type    = DDR_TYPE_LPDDR4X;

        /*
         * FSP frequency table — matches blob lp4x_f1/f2/f3.
         * Trainer runs at FSP0 (528 MHz); DFS handles the rest.
         */
        ch->fsp_freq_mhz[0] = 528;    /* FSP0 — boot / training freq */
        ch->fsp_freq_mhz[1] = 1068;   /* FSP1 */
        ch->fsp_freq_mhz[2] = 1560;   /* FSP2 */
        ch->boot_fsp         = 0;      /* start at FSP0 */
        ch->target_freq_mhz  = 2112;  /* lp4x_freq=2112 in blob */

        /* Timing resolved from fsp_freq_mhz[boot_fsp] */
        ch->timing.freq_mhz  = ch->fsp_freq_mhz[ch->boot_fsp];

        /*
         * ODT enable threshold: blob lp4x_dq_odten_freq_mhz=800.
         * ODT is active only at or above this frequency.
         * At FSP0=528 MHz, ODT is therefore OFF during training,
         * which is correct — avoids margin loss on DQS at low speed.
         */
        ch->odt_en_freq_mhz = 800;

        /* ---- DRAM geometry (8 Gb LPDDR4X, x32, 1 rank) ---- */
        ch->ranks     = 1;
        ch->cs_map    = 0x1;
        ch->bus_width = 32;
        ch->col_bits  = 10;
        ch->row_bits  = 17;
        ch->bank_bits = 3;
        ch->bg_bits   = 0;
        ch->size_mb   = 4096;    /* 4 GiB per channel */
    }

    /* ---- Training options ---- */
    ddr_cfg.do_wrlvl       = true;
    ddr_cfg.do_gate        = true;
    ddr_cfg.do_rd_deskew   = true;
    ddr_cfg.do_wr_deskew   = true;
    ddr_cfg.do_vref        = true;
    ddr_cfg.do_zq          = true;
    ddr_cfg.do_ca_training = true;

    ddr_cfg.verbosity = 2;

    return ddr_init_and_train(&ddr_cfg);
}
