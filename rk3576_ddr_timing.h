/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * RK3576 DDR timing tables
 *
 * Sources:
 *   JEDEC JESD209-4C  (LPDDR4 / LPDDR4X)
 *   JEDEC JESD209-5B  (LPDDR5)
 *   JEDEC JESD79-4C   (DDR4)
 *   JEDEC JESD79-5    (DDR5)
 *
 * All ps values are JEDEC minimums; the controller-to-clock conversion adds
 * one extra cycle for setup margin (see ps_to_clocks()).
 */

#ifndef RK3576_DDR_TIMING_H
#define RK3576_DDR_TIMING_H

#include "rk3576_ddr.h"

/* =========================================================================
 * Helper: nanoseconds → picoseconds
 * ========================================================================= */
#define NS(x)  ((x) * 1000)

/* =========================================================================
 * LPDDR4 / LPDDR4X timing tables
 * ========================================================================= */
static const ddr_timing_t lpddr4_timing_table[] = {
    /* ---- 1600 Mbps (800 MHz) ---- */
    {
        .freq_mhz = 800,
        .tck_ps   = 1250,
        .tRCD = NS(18),   .tRP  = NS(18),   .tRC  = NS(57),
        .tRAS_min = NS(42), .tRAS_max = NS(9*70000),
        .tRFC = NS(130),   .tRFC2 = 0,
        .tREFI = NS(3900),
        .tWR  = NS(18),   .tRTP = NS(7500),
        .tWTR_S = NS(2500), .tWTR_L = NS(7500),
        .tRRD_S = NS(10), .tRRD_L = NS(10),
        .tFAW = NS(40),   .tXS = NS(150),
        .tXP = NS(7500),  .tCKE = NS(7500),
        .tMOD = 0,        .tDLLK = 0,
        .cl  = 10, .cwl = 8,  .al = 0, .bl = 16,
        .rl  = 10, .wl  = 8,
        .dfi_t_rddata_en = 10, .dfi_t_ctrl_delay = 2,
        .dfi_t_wrdata    = 1,  .dfi_rdlvl_max = 32, .dfi_wrlvl_max = 32,
    },
    /* ---- 2133 Mbps (1066 MHz) ---- */
    {
        .freq_mhz = 1066,
        .tck_ps   = 938,
        .tRCD = NS(18),   .tRP  = NS(18),   .tRC  = NS(57),
        .tRAS_min = NS(42), .tRAS_max = NS(9*70000),
        .tRFC = NS(130),
        .tREFI = NS(3900),
        .tWR  = NS(18),   .tRTP = NS(7500),
        .tWTR_S = NS(2500), .tWTR_L = NS(7500),
        .tRRD_S = NS(10), .tRRD_L = NS(10),
        .tFAW = NS(40),   .tXS = NS(150),
        .tXP = NS(7500),  .tCKE = NS(7500),
        .cl  = 14, .cwl = 10, .al = 0, .bl = 16,
        .rl  = 14, .wl  = 10,
        .dfi_t_rddata_en = 14, .dfi_t_ctrl_delay = 2,
        .dfi_t_wrdata    = 1,  .dfi_rdlvl_max = 40, .dfi_wrlvl_max = 40,
    },
    /* ---- 2400 Mbps (1200 MHz) ---- */
    {
        .freq_mhz = 1200,
        .tck_ps   = 833,
        .tRCD = NS(18),   .tRP  = NS(18),   .tRC  = NS(57),
        .tRAS_min = NS(42), .tRAS_max = NS(9*70000),
        .tRFC = NS(130),
        .tREFI = NS(3900),
        .tWR  = NS(18),   .tRTP = NS(7500),
        .tWTR_S = NS(2500), .tWTR_L = NS(7500),
        .tRRD_S = NS(10), .tRRD_L = NS(10),
        .tFAW = NS(40),   .tXS = NS(150),
        .tXP = NS(7500),  .tCKE = NS(7500),
        .cl  = 16, .cwl = 12, .al = 0, .bl = 16,
        .rl  = 16, .wl  = 12,
        .dfi_t_rddata_en = 16, .dfi_t_ctrl_delay = 2,
        .dfi_t_wrdata    = 1,  .dfi_rdlvl_max = 48, .dfi_wrlvl_max = 48,
    },
    /* ---- 3200 Mbps (1600 MHz) ---- */
    {
        .freq_mhz = 1600,
        .tck_ps   = 625,
        .tRCD = NS(18),   .tRP  = NS(18),   .tRC  = NS(57),
        .tRAS_min = NS(42), .tRAS_max = NS(9*70000),
        .tRFC = NS(130),
        .tREFI = NS(3900),
        .tWR  = NS(18),   .tRTP = NS(7500),
        .tWTR_S = NS(2500), .tWTR_L = NS(7500),
        .tRRD_S = NS(10), .tRRD_L = NS(10),
        .tFAW = NS(40),   .tXS = NS(150),
        .tXP = NS(7500),  .tCKE = NS(7500),
        .cl  = 22, .cwl = 16, .al = 0, .bl = 16,
        .rl  = 22, .wl  = 16,
        .dfi_t_rddata_en = 22, .dfi_t_ctrl_delay = 2,
        .dfi_t_wrdata    = 2,  .dfi_rdlvl_max = 64, .dfi_wrlvl_max = 64,
    },
    /* ---- 4266 Mbps (2133 MHz) ---- */
    {
        .freq_mhz = 2133,
        .tck_ps   = 469,
        .tRCD = NS(18),   .tRP  = NS(18),   .tRC  = NS(57),
        .tRAS_min = NS(42), .tRAS_max = NS(9*70000),
        .tRFC = NS(130),
        .tREFI = NS(3900),
        .tWR  = NS(18),   .tRTP = NS(7500),
        .tWTR_S = NS(2500), .tWTR_L = NS(7500),
        .tRRD_S = NS(10), .tRRD_L = NS(10),
        .tFAW = NS(40),   .tXS = NS(150),
        .tXP = NS(7500),  .tCKE = NS(7500),
        .cl  = 30, .cwl = 22, .al = 0, .bl = 16,
        .rl  = 30, .wl  = 22,
        .dfi_t_rddata_en = 30, .dfi_t_ctrl_delay = 3,
        .dfi_t_wrdata    = 2,  .dfi_rdlvl_max = 80, .dfi_wrlvl_max = 80,
    },
};

/* =========================================================================
 * LPDDR5 timing tables
 * ========================================================================= */
static const ddr_timing_t lpddr5_timing_table[] = {
    /* ---- 3200 Mbps ---- */
    {
        .freq_mhz = 1600,
        .tck_ps   = 625,
        .tRCD = NS(18),   .tRP  = NS(18),   .tRC  = NS(65),
        .tRAS_min = NS(42), .tRAS_max = NS(9*70000),
        .tRFC = NS(210),   .tRFC2 = NS(120),
        .tREFI = NS(3906),
        .tWR  = NS(20),   .tRTP = NS(7500),
        .tWTR_S = NS(2500), .tWTR_L = NS(10000),
        .tRRD_S = NS(5),  .tRRD_L = NS(10),
        .tFAW = NS(20),   .tXS = NS(250),
        .tXP = NS(10),    .tCKE = NS(5),
        .cl  = 14, .cwl = 10, .al = 0, .bl = 16,
        .rl  = 14, .wl  = 10,
        .dfi_t_rddata_en = 14, .dfi_t_ctrl_delay = 2,
        .dfi_t_wrdata    = 1,  .dfi_rdlvl_max = 48, .dfi_wrlvl_max = 48,
    },
    /* ---- 4800 Mbps ---- */
    {
        .freq_mhz = 2400,
        .tck_ps   = 417,
        .tRCD = NS(18),   .tRP  = NS(18),   .tRC  = NS(65),
        .tRAS_min = NS(42), .tRAS_max = NS(9*70000),
        .tRFC = NS(210),   .tRFC2 = NS(120),
        .tREFI = NS(3906),
        .tWR  = NS(20),   .tRTP = NS(7500),
        .tWTR_S = NS(2500), .tWTR_L = NS(10000),
        .tRRD_S = NS(5),  .tRRD_L = NS(10),
        .tFAW = NS(20),   .tXS = NS(250),
        .tXP = NS(10),    .tCKE = NS(5),
        .cl  = 20, .cwl = 14, .al = 0, .bl = 16,
        .rl  = 20, .wl  = 14,
        .dfi_t_rddata_en = 20, .dfi_t_ctrl_delay = 2,
        .dfi_t_wrdata    = 2,  .dfi_rdlvl_max = 64, .dfi_wrlvl_max = 64,
    },
    /* ---- 6400 Mbps ---- */
    {
        .freq_mhz = 3200,
        .tck_ps   = 312,
        .tRCD = NS(18),   .tRP  = NS(18),   .tRC  = NS(65),
        .tRAS_min = NS(42), .tRAS_max = NS(9*70000),
        .tRFC = NS(210),   .tRFC2 = NS(120),
        .tREFI = NS(3906),
        .tWR  = NS(20),   .tRTP = NS(7500),
        .tWTR_S = NS(2500), .tWTR_L = NS(10000),
        .tRRD_S = NS(5),  .tRRD_L = NS(10),
        .tFAW = NS(20),   .tXS = NS(250),
        .tXP = NS(10),    .tCKE = NS(5),
        .cl  = 28, .cwl = 20, .al = 0, .bl = 16,
        .rl  = 28, .wl  = 20,
        .dfi_t_rddata_en = 28, .dfi_t_ctrl_delay = 3,
        .dfi_t_wrdata    = 2,  .dfi_rdlvl_max = 80, .dfi_wrlvl_max = 80,
    },
};

/* =========================================================================
 * DDR4 timing tables
 * ========================================================================= */
static const ddr_timing_t ddr4_timing_table[] = {
    /* ---- DDR4-2133 (CL-15-15-35) ---- */
    {
        .freq_mhz = 1066,
        .tck_ps   = 938,
        .tRCD = NS(14),   .tRP  = NS(14),   .tRC  = NS(49),
        .tRAS_min = NS(35), .tRAS_max = NS(9*70000),
        .tRFC = NS(260),
        .tREFI = NS(7800),
        .tWR  = NS(15),   .tRTP = NS(7500),
        .tWTR_S = NS(2500), .tWTR_L = NS(7500),
        .tRRD_S = NS(5),  .tRRD_L = NS(6),
        .tFAW = NS(35),   .tXS = NS(270),
        .tXP = NS(6),     .tCKE = NS(5),
        .tMOD = 24,       .tDLLK = 768,
        .cl  = 15, .cwl = 10, .al = 0, .bl = 8,
        .rl  = 15, .wl  = 10,
        .dfi_t_rddata_en = 15, .dfi_t_ctrl_delay = 2,
        .dfi_t_wrdata    = 1,  .dfi_rdlvl_max = 40, .dfi_wrlvl_max = 40,
    },
    /* ---- DDR4-2400 (CL-17-17-39) ---- */
    {
        .freq_mhz = 1200,
        .tck_ps   = 833,
        .tRCD = NS(14),   .tRP  = NS(14),   .tRC  = NS(49),
        .tRAS_min = NS(35), .tRAS_max = NS(9*70000),
        .tRFC = NS(260),
        .tREFI = NS(7800),
        .tWR  = NS(15),   .tRTP = NS(7500),
        .tWTR_S = NS(2500), .tWTR_L = NS(7500),
        .tRRD_S = NS(5),  .tRRD_L = NS(6),
        .tFAW = NS(30),   .tXS = NS(270),
        .tXP = NS(6),     .tCKE = NS(5),
        .tMOD = 24,       .tDLLK = 768,
        .cl  = 17, .cwl = 12, .al = 0, .bl = 8,
        .rl  = 17, .wl  = 12,
        .dfi_t_rddata_en = 17, .dfi_t_ctrl_delay = 2,
        .dfi_t_wrdata    = 2,  .dfi_rdlvl_max = 48, .dfi_wrlvl_max = 48,
    },
    /* ---- DDR4-2666 (CL-19-19-43) ---- */
    {
        .freq_mhz = 1333,
        .tck_ps   = 750,
        .tRCD = NS(14),   .tRP  = NS(14),   .tRC  = NS(49),
        .tRAS_min = NS(35), .tRAS_max = NS(9*70000),
        .tRFC = NS(350),
        .tREFI = NS(7800),
        .tWR  = NS(15),   .tRTP = NS(7500),
        .tWTR_S = NS(2500), .tWTR_L = NS(7500),
        .tRRD_S = NS(5),  .tRRD_L = NS(6),
        .tFAW = NS(25),   .tXS = NS(360),
        .tXP = NS(6),     .tCKE = NS(5),
        .tMOD = 24,       .tDLLK = 768,
        .cl  = 19, .cwl = 14, .al = 0, .bl = 8,
        .rl  = 19, .wl  = 14,
        .dfi_t_rddata_en = 19, .dfi_t_ctrl_delay = 2,
        .dfi_t_wrdata    = 2,  .dfi_rdlvl_max = 56, .dfi_wrlvl_max = 56,
    },
    /* ---- DDR4-3200 (CL-22-22-52) ---- */
    {
        .freq_mhz = 1600,
        .tck_ps   = 625,
        .tRCD = NS(14),   .tRP  = NS(14),   .tRC  = NS(49),
        .tRAS_min = NS(35), .tRAS_max = NS(9*70000),
        .tRFC = NS(350),
        .tREFI = NS(7800),
        .tWR  = NS(15),   .tRTP = NS(7500),
        .tWTR_S = NS(2500), .tWTR_L = NS(7500),
        .tRRD_S = NS(5),  .tRRD_L = NS(6),
        .tFAW = NS(25),   .tXS = NS(360),
        .tXP = NS(6),     .tCKE = NS(5),
        .tMOD = 24,       .tDLLK = 768,
        .cl  = 22, .cwl = 16, .al = 0, .bl = 8,
        .rl  = 22, .wl  = 16,
        .dfi_t_rddata_en = 22, .dfi_t_ctrl_delay = 3,
        .dfi_t_wrdata    = 2,  .dfi_rdlvl_max = 64, .dfi_wrlvl_max = 64,
    },
};

/* =========================================================================
 * Look-up function: returns pointer to matching timing entry
 * ========================================================================= */
static inline const ddr_timing_t *
ddr_get_timing(ddr_type_t type, uint32_t freq_mhz)
{
    const ddr_timing_t *tbl = NULL;
    size_t n = 0;
    size_t i;

    switch (type) {
    case DDR_TYPE_LPDDR4:
    case DDR_TYPE_LPDDR4X:
        tbl = lpddr4_timing_table;
        n   = ARRAY_SIZE(lpddr4_timing_table);
        break;
    case DDR_TYPE_LPDDR5:
        tbl = lpddr5_timing_table;
        n   = ARRAY_SIZE(lpddr5_timing_table);
        break;
    case DDR_TYPE_DDR4:
        tbl = ddr4_timing_table;
        n   = ARRAY_SIZE(ddr4_timing_table);
        break;
    default:
        return NULL;
    }

    /* exact match first */
    for (i = 0; i < n; i++) {
        if (tbl[i].freq_mhz == freq_mhz)
            return &tbl[i];
    }

    /* fall back to closest lower frequency */
    for (i = n; i > 0; i--) {
        if (tbl[i-1].freq_mhz <= freq_mhz)
            return &tbl[i-1];
    }

    return &tbl[0];   /* always return at least the lowest bin */
}

#endif /* RK3576_DDR_TIMING_H */
