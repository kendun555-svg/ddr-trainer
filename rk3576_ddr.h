/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Rockchip RK3576 DDR Trainer — top-level header
 *
 * Changes from original (blob analysis v1.09):
 *  - Added fsp_freq_mhz[3] and boot_fsp to ddr_chan_cfg_t
 *    (blob boots at FSP0=528 MHz; blob params: lp4x_f1=528, f2=1068, f3=1560)
 *  - Added odt_en_freq_mhz (blob: lp4x_dq_odten_freq_mhz=800)
 *  - Added per_bank_ref_en, derate_en flags to ddr_cfg_t
 *    (blob: per_bank_ref_en=1, derate_en=1)
 *  - Fixed CRU DPLL base: DPLL is PLL index 2, offset = 2 × 0x20 = 0x40
 *    Original had 0x80 (which is GPLL/index 4 — wrong)
 */

#ifndef RK3576_DDR_H
#define RK3576_DDR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* =========================================================================
 * Compiler / architecture helpers
 * ========================================================================= */
#define __iomem
#define barrier() __asm__ __volatile__("" ::: "memory")
#define dsb()     __asm__ __volatile__("dsb sy" ::: "memory")
#define isb()     __asm__ __volatile__("isb" ::: "memory")

#define readl(addr)         (*((volatile uint32_t *)(uintptr_t)(addr)))
#define writel(val, addr)   (*((volatile uint32_t *)(uintptr_t)(addr)) = (uint32_t)(val))
#define setbits_le32(addr, set)       writel(readl(addr) | (set),  addr)
#define clrbits_le32(addr, clr)       writel(readl(addr) & ~(clr), addr)
#define clrsetbits_le32(addr,clr,set) writel((readl(addr) & ~(clr)) | (set), addr)

#define BIT(n)          (1u << (n))
#define GENMASK(h, l)   (((1u << ((h)-(l)+1)) - 1u) << (l))
#define FIELD_PREP(mask, val) (((uint32_t)(val) << __builtin_ctz(mask)) & (mask))
#define FIELD_GET(mask, reg)  (((reg) & (mask)) >> __builtin_ctz(mask))

#define min(a, b)  ((a) < (b) ? (a) : (b))
#define max(a, b)  ((a) > (b) ? (a) : (b))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* =========================================================================
 * RK3576 memory map — DDR subsystem
 *
 * Channel 0:  DDRMC0  0xF7000000   DDRPHY0  0xF7400000
 * Channel 1:  DDRMC1  0xF7200000   DDRPHY1  0xF7600000
 * Shared:     DDRGRF  0xF7800000   MSCH     0xF7900000
 *             CRU     0xFF010000   PMUGRF   0xFF080000
 * ========================================================================= */
#define DDRMC0_BASE     0xF7000000UL
#define DDRMC1_BASE     0xF7200000UL
#define DDRPHY0_BASE    0xF7400000UL
#define DDRPHY1_BASE    0xF7600000UL
#define DDRGRF_BASE     0xF7800000UL
#define MSCH_BASE       0xF7900000UL
#define CRU_BASE        0xFF010000UL
#define PMUGRF_BASE     0xFF080000UL

#define DDR_MAX_CHANNELS    2
#define DDR_MAX_BYTES       4
#define DDR_MAX_BITS        32
#define DDR_CA_LANES        6
#define DDR_MAX_RANKS       2
#define DDR_MAX_CS          2

/*
 * FSP (Frequency Setpoint) count.
 * Blob uses 3 FSPs for LPDDR4X: 528 / 1068 / 1560 MHz
 * before reaching the target of 2112 MHz.
 */
#define DDR_MAX_FSP         3

/* =========================================================================
 * DDR type enumeration
 * ========================================================================= */
typedef enum {
    DDR_TYPE_DDR4    = 0,
    DDR_TYPE_DDR5    = 1,
    DDR_TYPE_LPDDR4  = 2,
    DDR_TYPE_LPDDR4X = 3,
    DDR_TYPE_LPDDR5  = 4,
    DDR_TYPE_UNKNOWN = 0xFF,
} ddr_type_t;

/* =========================================================================
 * JEDEC speed bins
 * ========================================================================= */
typedef enum {
    LPDDR4_1600  = 1600,
    LPDDR4_2133  = 2133,
    LPDDR4_2400  = 2400,
    LPDDR4_3200  = 3200,
    LPDDR4_4266  = 4266,
    LPDDR5_3200  = 3200,
    LPDDR5_4800  = 4800,
    LPDDR5_6400  = 6400,
    LPDDR5_7500  = 7500,
    DDR4_2133    = 2133,
    DDR4_2400    = 2400,
    DDR4_2666    = 2666,
    DDR4_3200    = 3200,
    DDR5_4800    = 4800,
    DDR5_5600    = 5600,
    DDR5_6400    = 6400,
} ddr_speed_bin_t;

/* =========================================================================
 * Timing parameters (picoseconds unless noted)
 * ========================================================================= */
typedef struct {
    uint32_t freq_mhz;
    uint32_t tck_ps;
    uint32_t tRCD, tRP, tRC;
    uint32_t tRAS_min, tRAS_max;
    uint32_t tRFC, tRFC2;
    uint32_t tREFI;
    uint32_t tWR, tRTP;
    uint32_t tWTR_S, tWTR_L;
    uint32_t tRRD_S, tRRD_L;
    uint32_t tFAW, tXS, tXP, tCKE;
    uint32_t tMOD, tDLLK;
    uint32_t cl, cwl, al, bl, rl, wl;
    uint32_t dfi_t_rddata_en;
    uint32_t dfi_t_ctrl_delay;
    uint32_t dfi_t_wrdata;
    uint32_t dfi_rdlvl_max;
    uint32_t dfi_wrlvl_max;
} ddr_timing_t;

/* =========================================================================
 * PHY calibration results — per byte lane
 * ========================================================================= */
typedef struct {
    int32_t  wrlvl_dqs_delay[DDR_MAX_BYTES];
    uint8_t  wrlvl_done;
    int32_t  gate_delay[DDR_MAX_BYTES];
    uint8_t  gate_done;
    int32_t  rd_dq_delay[DDR_MAX_BYTES][8];
    int32_t  rd_dqs_delay[DDR_MAX_BYTES];
    uint8_t  rd_deskew_done;
    int32_t  wr_dq_delay[DDR_MAX_BYTES][8];
    int32_t  wr_dqs_delay[DDR_MAX_BYTES];
    uint8_t  wr_deskew_done;
    uint8_t  vref_dq_host;
    uint8_t  vref_dq_dram;
    uint8_t  vref_done;
    uint32_t zq_pull_up;
    uint32_t zq_pull_dn;
    uint8_t  zq_done;
    int32_t  rd_odt_delay[DDR_MAX_RANKS];
    int32_t  wr_odt_delay[DDR_MAX_RANKS];
    int32_t  ca_delay[DDR_CA_LANES];
    int32_t  ck_delay;
    uint8_t  ca_done;
    uint8_t  rd_eye_width[DDR_MAX_BYTES];
    uint8_t  wr_eye_width[DDR_MAX_BYTES];
} ddr_phy_result_t;

/* =========================================================================
 * Per-channel configuration
 * ========================================================================= */
typedef struct {
    uint8_t   channel;
    bool      enabled;

    ddr_type_t      type;
    ddr_speed_bin_t speed;

    uint8_t   ranks;
    uint8_t   cs_map;
    uint8_t   bus_width;
    uint8_t   dq_swap;

    uint8_t   col_bits;
    uint8_t   row_bits;
    uint8_t   bank_bits;
    uint8_t   bg_bits;
    uint32_t  size_mb;

    /*
     * FSP (Frequency Setpoint) table.
     * fsp_freq_mhz[0] = boot frequency (528 MHz from blob).
     * fsp_freq_mhz[1..2] = intermediate steps (1068, 1560 MHz).
     * The trainer trains at fsp_freq_mhz[boot_fsp] (normally index 0).
     * Runtime DFS then scales up; that path lives in the ATF/kernel driver.
     *
     * Blob values (lp4x):
     *   f1=528, f2=1068, f3=1560  target=2112 MHz
     */
    uint32_t  fsp_freq_mhz[DDR_MAX_FSP];
    uint8_t   boot_fsp;          /* index into fsp_freq_mhz[] to train at */
    uint32_t  target_freq_mhz;   /* final DFS target (informational) */

    /*
     * ODT enable frequency threshold (MHz).
     * ODT is only enabled at or above this frequency.
     * Blob: lp4x_dq_odten_freq_mhz=800, lp4x_ca_odten_freq_mhz=800
     */
    uint32_t  odt_en_freq_mhz;

    ddr_timing_t    timing;
    ddr_phy_result_t cal;

    uintptr_t mc_base;
    uintptr_t phy_base;
} ddr_chan_cfg_t;

/* =========================================================================
 * Top-level DDR configuration
 * ========================================================================= */
typedef struct {
    ddr_chan_cfg_t chan[DDR_MAX_CHANNELS];
    uint8_t        active_channels;
    uint32_t       total_size_mb;

    bool do_wrlvl;
    bool do_gate;
    bool do_rd_deskew;
    bool do_wr_deskew;
    bool do_vref;
    bool do_zq;
    bool do_ca_training;

    /*
     * per_bank_ref_en: enables per-bank refresh (LPDDR4/5 JEDEC feature).
     * Blob: per_bank_ref_en=1. Reduces refresh overhead at the cost of
     * slightly more complex timing; mandatory for high-density LPDDR4X.
     */
    bool per_bank_ref_en;

    /*
     * derate_en: enables temperature-based timing derating (JEDEC JESD209-4).
     * Blob: derate_en=1. Without this, tRCD/tRP/tRC are under-margined
     * above 85 °C. Must be enabled for production use.
     */
    bool derate_en;

    uint8_t  verbosity;
    bool     hw_ddr_debug;
} ddr_cfg_t;

/* =========================================================================
 * Training return codes
 * ========================================================================= */
typedef enum {
    DDR_TRAIN_OK            =  0,
    DDR_TRAIN_ERR_TIMEOUT   = -1,
    DDR_TRAIN_ERR_WRLVL     = -2,
    DDR_TRAIN_ERR_GATE      = -3,
    DDR_TRAIN_ERR_RD_DESKEW = -4,
    DDR_TRAIN_ERR_WR_DESKEW = -5,
    DDR_TRAIN_ERR_VREF      = -6,
    DDR_TRAIN_ERR_ZQ        = -7,
    DDR_TRAIN_ERR_CA        = -8,
    DDR_TRAIN_ERR_MRS       = -9,
    DDR_TRAIN_ERR_INIT      = -10,
    DDR_TRAIN_ERR_BIST      = -11,
} ddr_train_err_t;

/* =========================================================================
 * Constants
 * ========================================================================= */
#define PHY_DLL_MAX_DELAY   128
#define PHY_DLL_MID_DELAY    64
#define PHY_DLL_TAP_PS       12
#define PHY_VREF_STEPS       64
#define PHY_VREF_STEP_MV      7

/* =========================================================================
 * Public API
 * ========================================================================= */
void ddr_cfg_init_defaults(ddr_cfg_t *cfg);
int  ddr_init_and_train(ddr_cfg_t *cfg);

int  ddr_phy_init(ddr_chan_cfg_t *ch);
int  ddr_mc_init(ddr_chan_cfg_t *ch);
int  ddr_dram_init(ddr_chan_cfg_t *ch);

int  ddr_train_zq(ddr_chan_cfg_t *ch);
int  ddr_train_ca(ddr_chan_cfg_t *ch);
int  ddr_train_wrlvl(ddr_chan_cfg_t *ch);
int  ddr_train_gate(ddr_chan_cfg_t *ch);
int  ddr_train_rd_deskew(ddr_chan_cfg_t *ch);
int  ddr_train_wr_deskew(ddr_chan_cfg_t *ch);
int  ddr_train_vref(ddr_chan_cfg_t *ch);

int  ddr_bist(ddr_chan_cfg_t *ch, uint32_t test_size_mb);

void ddr_calc_timings(ddr_chan_cfg_t *ch);
uint32_t ps_to_clocks(uint32_t ps, uint32_t freq_mhz);

void ddr_dump_phy_results(const ddr_chan_cfg_t *ch);
void ddr_print_timing(const ddr_timing_t *t);

/* Timing table lookup (defined in rk3576_ddr_timing.h) */
const ddr_timing_t *ddr_get_timing(ddr_type_t type, uint32_t freq_mhz);

#endif /* RK3576_DDR_H */
