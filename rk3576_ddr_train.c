// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Rockchip RK3576 DDR Trainer — main implementation
 *
 * Training sequence:
 *  1. CRU: configure DPLL for FSP0 (528 MHz)
 *  2. Release DDR controller and PHY from reset
 *  3. PHY: power-on, DLL lock, ZQ calibration
 *  4. DDRMC: program controller + timing registers
 *  5. DRAM init: JEDEC power-up, mode register writes
 *  6. CA training       (LPDDR4/5)
 *  7. Write leveling
 *  8. Gate training
 *  9. Read DQ deskew
 * 10. Write DQ deskew
 * 11. VREF training     (BIST-verified sweep)
 * 12. BIST verification
 * 13. DDRGRF: program address interleave masks
 *
 * Changes from original (all derived from rkbin v1.09 blob analysis):
 *
 *  ddr_cru_set_pll()
 *    - Fixed DPLL CON offset: 0x80 → 0x40 (DPLL is PLL index 2, each PLL
 *      entry = 0x20 bytes; original 0x80 pointed at GPLL/index 4)
 *    - Fixed PLL lock poll: CON2[31] → CON4[31] (lock status register)
 *
 *  ddr_release_reset()
 *    - Fixed softreset register offset: CRU_SOFTRST_CON1 (0x404) →
 *      CRU_SOFTRST_CON27 (0xa6c) per RK3576_SOFTRST_CON(x)=x*4+0xa00 macro
 *      in Rockchip U-Boot cru_rk3576.h
 *
 *  ddr_phy_init()
 *    - PHY TX drive strength: TSEL 0x6 (~34Ω) → 0x7 (~30Ω) for LPDDR4/4X/5
 *      (blob: phy_lp4x_dq_drv_when_odten_ohm=30, ca_drv=30, clk_drv=30)
 *    - Added PHY ODT impedance: OTSEL → 40Ω for LPDDR4/4X
 *      (blob: phy_lp4x_odt_ohm=40)
 *    - ODT enable is now conditional on odt_en_freq_mhz threshold
 *      (blob: lp4x_dq_odten_freq_mhz=800 — ODT off at FSP0=528 MHz)
 *
 *  ddr_mc_init()
 *    - Added DDRMC_DERATEEN enable (blob: derate_en=1)
 *    - Added RFSHCTL0_PER_BANK_REFRESH (blob: per_bank_ref_en=1)
 *
 *  ddr_dram_init()
 *    - MR11 DQ ODT: 0x04 (60Ω) → LPDDR4_MR11_ODT_40OHM (0x06) for LP4/4X
 *      (blob: lp4x_odt_ohm=40; JEDEC JESD209-4C Table 21: 110=RZQ/6=40Ω)
 *    - MR14 initial VREF: 0x31 → LPDDR4X_MR14_VREF_INIT (0x20) for LPDDR4X
 *      (code 0x20=32 → 10+32×0.4=22.8% range1; safer start for VREF sweep)
 *    - MR22 CA ODT: 0x04 (60Ω) → LPDDR4_MR22_SOCOCT_120OHM (0x02) for LP4X
 *      (blob: lp4x_ca_odt_ohm=120; JEDEC: 010=RZQ/2=120Ω)
 *    - lp4x_odte_cs_en=0: CS ODT disabled for LPDDR4X per blob
 *      (LPDDR4 had it enabled; LPDDR4X does not)
 *
 *  ddr_train_vref()
 *    - Replaced fake hardcoded pass window (step 16..48) with real
 *      ddr_bist() call at each VREF step. This was the most critical bug —
 *      the original always selected code ~32 regardless of actual DRAM response.
 *    - Initial VREF set to LPDDR4X_MR14_VREF_INIT before sweep starts.
 *    - Added tVREFDQE settling delay (150 ns) after each MR14 write.
 *
 *  ddr_init_and_train()
 *    - PLL now set to FSP0 frequency (528 MHz) not target frequency.
 *      Blob: boot_fsp=0 → trains at lp4x_f1=528 MHz.
 *    - Added DDRGRF bank mask and rank mask programming after training.
 *      (blob: lp4_4x_bank_mask0=0x800, mask1=0x1000, mask2=0x2000,
 *             lp4_4x_rank_mask0=0x400000)
 *    - Interleave setup moved to DDRGRF_COMMON_CON0 (correct offset 0x0540).
 */

#include "rk3576_ddr.h"
#include "rk3576_ddr_regs.h"
#include "rk3576_ddr_timing.h"

extern void udelay(uint32_t us);

static void (*ddr_print_fn)(const char *msg) = NULL;
#define DDR_LOG(cfg, lvl, ...) \
    do { if ((cfg)->verbosity >= (lvl) && ddr_print_fn) \
        (void)(__VA_ARGS__); } while (0)

/* =========================================================================
 * Helpers
 * ========================================================================= */
uint32_t ps_to_clocks(uint32_t ps, uint32_t freq_mhz)
{
    uint32_t tck_ps = 1000000u / freq_mhz;
    return (ps + tck_ps - 1) / tck_ps + 1;
}

/* Rockchip GRF write-mask style: bits[31:16] = write-enable for bits[15:0] */
static void grf_write(uintptr_t addr, uint32_t mask, uint32_t val)
{
    writel((mask << 16) | (val & mask), addr);
}

static int poll_flag(uintptr_t addr, uint32_t mask, uint32_t expected,
                     uint32_t timeout_us)
{
    while (timeout_us--) {
        if ((readl(addr) & mask) == expected)
            return DDR_TRAIN_OK;
        udelay(1);
    }
    return DDR_TRAIN_ERR_TIMEOUT;
}

/* =========================================================================
 * Step 0 — Default configuration
 * ========================================================================= */
void ddr_cfg_init_defaults(ddr_cfg_t *cfg)
{
    int i;
    for (i = 0; i < (int)sizeof(*cfg); i++)
        ((uint8_t *)cfg)[i] = 0;

    cfg->active_channels  = 1;
    cfg->do_wrlvl         = true;
    cfg->do_gate          = true;
    cfg->do_rd_deskew     = true;
    cfg->do_wr_deskew     = true;
    cfg->do_vref          = true;
    cfg->do_zq            = true;
    cfg->do_ca_training   = true;
    cfg->per_bank_ref_en  = true;   /* blob: per_bank_ref_en=1 */
    cfg->derate_en        = true;   /* blob: derate_en=1        */
    cfg->verbosity        = 2;

    for (i = 0; i < DDR_MAX_CHANNELS; i++) {
        cfg->chan[i].channel  = (uint8_t)i;
        cfg->chan[i].mc_base  = (i == 0) ? DDRMC0_BASE : DDRMC1_BASE;
        cfg->chan[i].phy_base = (i == 0) ? DDRPHY0_BASE : DDRPHY1_BASE;
        cfg->chan[i].bus_width        = 32;
        cfg->chan[i].ranks            = 1;
        cfg->chan[i].cs_map           = 0x1;
        cfg->chan[i].col_bits         = 10;
        cfg->chan[i].row_bits         = 16;
        cfg->chan[i].bank_bits        = 3;
        cfg->chan[i].bg_bits          = 0;
        cfg->chan[i].odt_en_freq_mhz  = 800; /* blob: lp4x_dq_odten_freq_mhz=800 */

        /* Default FSP table for LPDDR4X (blob values) */
        cfg->chan[i].fsp_freq_mhz[0]  = 528;
        cfg->chan[i].fsp_freq_mhz[1]  = 1068;
        cfg->chan[i].fsp_freq_mhz[2]  = 1560;
        cfg->chan[i].boot_fsp         = 0;
        cfg->chan[i].target_freq_mhz  = 2112;
    }
    cfg->chan[0].enabled = true;
    cfg->chan[1].enabled = false;
}

/* =========================================================================
 * Step 1 — CRU: DPLL configuration
 *
 * FIX: DPLL is PLL index 2. Each rk3576_pll struct = 5 words + 3 reserved
 * = 8 words = 0x20 bytes. DPLL base = 2 × 0x20 = 0x40.
 * Original used 0x80 = PLL index 4 = GPLL — wrong PLL entirely.
 *
 * FIX: Lock status is in CON4[31], not CON2[31].
 *
 * At boot we set the PLL to FSP0 frequency (528 MHz).
 * ========================================================================= */
static int ddr_cru_set_pll(uint32_t freq_mhz)
{
    uint32_t fbdiv, refdiv, postdiv1, postdiv2, val;

    /*
     * Simple divider strategy: refdiv=1, postdiv1=2, postdiv2=1.
     * DPLL_out = 24 × fbdiv / (refdiv × postdiv1 × postdiv2)
     * Works for: 528 (fbdiv=44), 1068 (fbdiv=89), 1560 (fbdiv=130), 2112 (fbdiv=176).
     * Production code should implement a full divider search.
     */
    refdiv   = 1;
    postdiv1 = 2;
    postdiv2 = 1;
    fbdiv    = (freq_mhz * postdiv1 * postdiv2) / 24;

    /* Power down PLL before changing dividers */
    setbits_le32(CRU_BASE + CRU_DPLL_CON3, DPLL_PD);
    udelay(5);

    /* Integer-N mode */
    setbits_le32(CRU_BASE + CRU_DPLL_CON3, DPLL_DSMPD);

    /* REFDIV, POSTDIV1, POSTDIV2 */
    val  = FIELD_PREP(DPLL_POSTDIV1_MASK, postdiv1 - 1);
    val |= FIELD_PREP(DPLL_POSTDIV2_MASK, postdiv2 - 1);
    val |= FIELD_PREP(DPLL_REFDIV_MASK,   refdiv   - 1);
    clrsetbits_le32(CRU_BASE + CRU_DPLL_CON1,
                    DPLL_POSTDIV1_MASK | DPLL_POSTDIV2_MASK | DPLL_REFDIV_MASK,
                    val);

    /* FBDIV */
    clrsetbits_le32(CRU_BASE + CRU_DPLL_CON0,
                    DPLL_FBDIV_MASK,
                    FIELD_PREP(DPLL_FBDIV_MASK, fbdiv));

    /* Power up and wait for lock — lock bit is in CON4[31] */
    clrbits_le32(CRU_BASE + CRU_DPLL_CON3, DPLL_PD);

    return poll_flag(CRU_BASE + CRU_DPLL_CON4, BIT(31), BIT(31),
                     DDR_DLL_LOCK_TIMEOUT_US);
}

/* =========================================================================
 * Step 2 — Release DDR resets
 *
 * FIX: softreset register was CRU_SOFTRST_CON1 (0x404).
 * Per Rockchip U-Boot cru_rk3576.h: RK3576_SOFTRST_CON(x) = x*4 + 0xa00.
 * DDR resets are in softrst_con[27] per RK3576 TRM.
 * Verify bit positions against your TRM before taping out.
 * ========================================================================= */
static void ddr_release_reset(uint8_t ch)
{
    uint32_t mc_bit  = (ch == 0) ? SRST_DDRMC0_N  : SRST_DDRMC1_N;
    uint32_t phy_bit = (ch == 0) ? SRST_DDRPHY0_N : SRST_DDRPHY1_N;

    /* Assert reset */
    clrbits_le32(CRU_BASE + CRU_SOFTRST_CON27, mc_bit | phy_bit);
    udelay(10);
    /* De-assert reset */
    setbits_le32(CRU_BASE + CRU_SOFTRST_CON27, mc_bit | phy_bit);
    udelay(10);
}

/* =========================================================================
 * Step 3 — PHY power-on and DLL lock
 *
 * FIX: Drive strength now type-specific.
 *   LPDDR4/4X/5: TSEL=0x7 (~30 Ω) — blob phy_lp4x_dq_drv_when_odten_ohm=30
 *   DDR4:         TSEL=0x6 (~34 Ω) — DDR4 standard
 *
 * FIX: PHY ODT impedance now set to 40 Ω for LPDDR4/4X.
 *   blob: phy_lp4x_odt_ohm=40
 *
 * FIX: ODT enable is conditional on odt_en_freq_mhz threshold (800 MHz).
 *   At FSP0=528 MHz, ODT is NOT enabled. This is correct and matches the blob.
 *   lp4x_odte_cs_en=0 for LPDDR4X (CS ODT disabled, unlike LPDDR4).
 * ========================================================================= */
int ddr_phy_init(ddr_chan_cfg_t *ch)
{
    uintptr_t phy = ch->phy_base;
    uint32_t tsel, otsel;
    int ret;

    /* Assert PHY reset */
    clrsetbits_le32(PHY_REG(phy, PHY_CON0),
                    PHY_CON0_PHY_EN | PHY_CON0_DLL_EN,
                    PHY_CON0_RESET);
    udelay(10);

    /* De-assert, enable */
    clrsetbits_le32(PHY_REG(phy, PHY_CON0), PHY_CON0_RESET, PHY_CON0_PHY_EN);
    udelay(20);

    setbits_le32(PHY_REG(phy, PHY_CON0), PHY_CON0_DLL_EN);

    /* DLL bypass at low frequency */
    if (ch->timing.freq_mhz > 400) {
        clrbits_le32(PHY_REG(phy, PHY_DLL_CTRL0), DLL_CTRL0_BYPASS);
        setbits_le32(PHY_REG(phy, PHY_DLL_CTRL0), DLL_CTRL0_START);
    } else {
        setbits_le32(PHY_REG(phy, PHY_DLL_CTRL0), DLL_CTRL0_BYPASS);
    }

    ret = poll_flag(PHY_REG(phy, PHY_DLL_STATUS), DLL_STATUS_LOCK,
                    DLL_STATUS_LOCK, DDR_DLL_LOCK_TIMEOUT_US);
    if (ret)
        return ret;

    /*
     * TX drive strength — type specific.
     * blob: phy_lp4x_dq/ca/clk_drv_when_odten_ohm = 30 Ω → TSEL 0x7
     * blob: phy_ddr4 (not populated; keep standard 34 Ω → TSEL 0x6)
     */
    switch (ch->type) {
    case DDR_TYPE_LPDDR4:
    case DDR_TYPE_LPDDR4X:
    case DDR_TYPE_LPDDR5:
        tsel  = PHY_CON2_TSEL_30OHM;   /* 30 Ω — blob target */
        otsel = PHY_CON2_OTSEL_40OHM;  /* 40 Ω — blob phy_lp4x_odt_ohm=40 */
        break;
    default:
        tsel  = PHY_CON2_TSEL_34OHM;
        otsel = PHY_CON2_OTSEL_40OHM;
        break;
    }

    clrsetbits_le32(PHY_REG(phy, PHY_CON2),
                    PHY_CON2_TSEL_MASK | PHY_CON2_OTSEL_MASK,
                    FIELD_PREP(PHY_CON2_TSEL_MASK,  tsel) |
                    FIELD_PREP(PHY_CON2_OTSEL_MASK, otsel));

    /*
     * ODT enable only at or above the ODT threshold frequency.
     * blob: lp4x_dq_odten_freq_mhz=800; at FSP0=528 MHz, ODT stays off.
     * lp4x_odte_cs_en=0 for LPDDR4X (CS ODT disabled).
     */
    if (ch->timing.freq_mhz >= ch->odt_en_freq_mhz)
        setbits_le32(PHY_REG(phy, PHY_CON2), PHY_CON2_ODT_EN);
    else
        clrbits_le32(PHY_REG(phy, PHY_CON2), PHY_CON2_ODT_EN);

    return DDR_TRAIN_OK;
}

/* =========================================================================
 * Step 3b — ZQ calibration
 * ========================================================================= */
int ddr_train_zq(ddr_chan_cfg_t *ch)
{
    uintptr_t phy = ch->phy_base;
    uint32_t status;
    int ret;

    setbits_le32(PHY_REG(phy, PHY_ZQ_CTRL0), ZQ_CTRL0_START);

    ret = poll_flag(PHY_REG(phy, PHY_ZQ_STATUS), ZQ_STATUS_DONE,
                    ZQ_STATUS_DONE, DDR_ZQ_TIMEOUT_US);
    if (ret)
        return DDR_TRAIN_ERR_ZQ;

    status = readl(PHY_REG(phy, PHY_ZQ_STATUS));
    ch->cal.zq_pull_up = FIELD_GET(ZQ_STATUS_PU_MASK, status);
    ch->cal.zq_pull_dn = FIELD_GET(ZQ_STATUS_PD_MASK, status);
    ch->cal.zq_done    = 1;

    clrbits_le32(PHY_REG(phy, PHY_ZQ_CTRL0), ZQ_CTRL0_START);
    return DDR_TRAIN_OK;
}

/* =========================================================================
 * Step 4 — DDRMC: program controller registers
 *
 * FIX: Added DDRMC_DERATEEN enable (blob: derate_en=1).
 *   Without this, tRCD/tRP/tRC are under-margined at temperature > 85 °C.
 *
 * FIX: Added RFSHCTL0_PER_BANK_REFRESH (blob: per_bank_ref_en=1).
 *   Reduces refresh penalty for high-density LPDDR4X dies.
 * ========================================================================= */
int ddr_mc_init(ddr_chan_cfg_t *ch)
{
    uintptr_t mc  = ch->mc_base;
    const ddr_timing_t *t = &ch->timing;
    uint32_t val, freq = t->freq_mhz;
    int ret;

    /* MSTR: DDR type, burst length, ranks, bus width */
    val = 0;
    switch (ch->type) {
    case DDR_TYPE_LPDDR4:
    case DDR_TYPE_LPDDR4X: val = MSTR_LPDDR4; break;
    case DDR_TYPE_LPDDR5:  val = MSTR_LPDDR5; break;
    case DDR_TYPE_DDR4:    val = MSTR_DDR4;   break;
    case DDR_TYPE_DDR5:    val = MSTR_DDR5;   break;
    default: return DDR_TRAIN_ERR_INIT;
    }
    {
        uint32_t bl_field = (ch->type == DDR_TYPE_DDR4) ? 4 : 8;
        val |= FIELD_PREP(MSTR_BURST_RDWR_MASK,    bl_field);
        val |= FIELD_PREP(MSTR_ACTIVE_RANKS_MASK,  ch->cs_map);
        val |= FIELD_PREP(MSTR_DEVICE_CONFIG_MASK, (ch->bus_width == 32) ? 2 : 1);
    }
    writel(val, MC_REG(mc, DDRMC_MSTR));

    /* Disable auto-refresh and auto-ZQ during init */
    setbits_le32(MC_REG(mc, DDRMC_RFSHCTL3), RFSHCTL3_DIS_AUTO_REFRESH);
    setbits_le32(MC_REG(mc, DDRMC_ZQCTL0),   ZQCTL0_DIS_AUTO_ZQ);

    /*
     * Temperature derating — JEDEC mandatory for LPDDR4 above 85 °C.
     * derate_byte=1: derate tRCD, tRP, and tRC (not just tRCD).
     * blob: derate_en=1.
     */
    if (ch->type == DDR_TYPE_LPDDR4  || ch->type == DDR_TYPE_LPDDR4X ||
        ch->type == DDR_TYPE_LPDDR5) {
        setbits_le32(MC_REG(mc, DDRMC_DERATEEN),
                     DERATEEN_DERATE_EN | DERATEEN_DERATE_BYTE);
    }

    /* DRAM timings */
#define CLK(ps)  ps_to_clocks((ps), freq)

    {
        uint32_t wr2pre = t->cwl + t->al + t->bl / 2 + CLK(t->tWR);
        writel(FIELD_PREP(DRAMTMG0_T_RAS_MIN_MASK, CLK(t->tRAS_min)) |
               FIELD_PREP(DRAMTMG0_T_RAS_MAX_MASK,
                          t->tRAS_max / (1024 * (1000000u / freq))) |
               FIELD_PREP(DRAMTMG0_T_FAW_MASK,  CLK(t->tFAW)) |
               FIELD_PREP(DRAMTMG0_WR2PRE_MASK, wr2pre),
               MC_REG(mc, DDRMC_DRAMTMG0));
    }

    {
        uint32_t rd2pre = max(CLK(t->tRTP), 4u);
        writel(FIELD_PREP(DRAMTMG1_T_RC_MASK,   CLK(t->tRC)) |
               FIELD_PREP(DRAMTMG1_RD2PRE_MASK, rd2pre) |
               FIELD_PREP(DRAMTMG1_T_XP_MASK,   CLK(t->tXP)),
               MC_REG(mc, DDRMC_DRAMTMG1));
    }

    {
        uint32_t wr2rd = t->cwl + t->al + t->bl / 2 + CLK(t->tWTR_L) + 2;
        uint32_t rd2wr = t->cl  + t->al + t->bl / 2 - t->cwl + 2;
        writel(FIELD_PREP(DRAMTMG2_WR2RD_MASK,         wr2rd) |
               FIELD_PREP(DRAMTMG2_RD2WR_MASK,         rd2wr) |
               FIELD_PREP(DRAMTMG2_READ_LATENCY_MASK,  t->rl) |
               FIELD_PREP(DRAMTMG2_WRITE_LATENCY_MASK, t->wl),
               MC_REG(mc, DDRMC_DRAMTMG2));
    }

    writel(FIELD_PREP(DRAMTMG4_T_RP_MASK,    CLK(t->tRP))    |
           FIELD_PREP(DRAMTMG4_T_RRD_S_MASK, CLK(t->tRRD_S)) |
           FIELD_PREP(DRAMTMG4_T_RCD_MASK,   CLK(t->tRCD))   |
           FIELD_PREP(DRAMTMG4_T_RRD_L_MASK, CLK(t->tRRD_L)),
           MC_REG(mc, DDRMC_DRAMTMG4));

    writel(FIELD_PREP(RFSHTMG_T_RFC_MIN_MASK,        CLK(t->tRFC)) |
           FIELD_PREP(RFSHTMG_T_RFC_NOM_X1_X32_MASK, CLK(t->tREFI) >> 5),
           MC_REG(mc, DDRMC_RFSHTMG));

    /* DFI timing */
    writel(FIELD_PREP(DFITMG0_DFI_TPHY_WRLAT_MASK,  t->wl - 1) |
           FIELD_PREP(DFITMG0_DFI_TPHY_WRDATA_MASK, t->dfi_t_wrdata) |
           FIELD_PREP(DFITMG0_DFI_T_RDDATA_EN_MASK, t->dfi_t_rddata_en) |
           FIELD_PREP(DFITMG0_DFI_CTRL_DELAY_MASK,  t->dfi_t_ctrl_delay),
           MC_REG(mc, DDRMC_DFITMG0));

    /* Address map */
    writel(0x00000000, MC_REG(mc, DDRMC_ADDRMAP0));
    writel(0x00080808, MC_REG(mc, DDRMC_ADDRMAP1));
    writel(0x00000000, MC_REG(mc, DDRMC_ADDRMAP2));
    writel(0x00000000, MC_REG(mc, DDRMC_ADDRMAP3));
    writel(0x00000F0F, MC_REG(mc, DDRMC_ADDRMAP4));
    writel(0x07070707, MC_REG(mc, DDRMC_ADDRMAP5));
    writel(0x07070707, MC_REG(mc, DDRMC_ADDRMAP6));

    /* ODT map */
    if (ch->type == DDR_TYPE_LPDDR4 || ch->type == DDR_TYPE_LPDDR4X ||
        ch->type == DDR_TYPE_LPDDR5)
        writel(0x00000000, MC_REG(mc, DDRMC_ODTMAP));
    else
        writel(0x00000001, MC_REG(mc, DDRMC_ODTMAP));

    /* Scheduler */
    writel(0x00000F01, MC_REG(mc, DDRMC_SCHED));

    /* DFI init handshake */
    setbits_le32(MC_REG(mc, DDRMC_DFIMISC), DFIMISC_DFI_INIT_START);
    ret = poll_flag(MC_REG(mc, DDRMC_DFISTAT), DFISTAT_DFI_INIT_COMPLETE,
                    DFISTAT_DFI_INIT_COMPLETE, DDR_INIT_TIMEOUT_US);
    if (ret)
        return ret;
    clrbits_le32(MC_REG(mc, DDRMC_DFIMISC), DFIMISC_DFI_INIT_START);
    setbits_le32(MC_REG(mc, DDRMC_DFIMISC), DFIMISC_DFI_INIT_COMPLETE_EN);

    /*
     * Per-bank refresh — enable after DFI init completes.
     * blob: per_bank_ref_en=1.
     */
    if ((ch->type == DDR_TYPE_LPDDR4  || ch->type == DDR_TYPE_LPDDR4X ||
         ch->type == DDR_TYPE_LPDDR5)) {
        setbits_le32(MC_REG(mc, DDRMC_RFSHCTL0), RFSHCTL0_PER_BANK_REFRESH);
    }

    return DDR_TRAIN_OK;
}

/* =========================================================================
 * Step 5 — DRAM initialization: MRS writes
 *
 * FIX (LPDDR4/4X MR11): ODT value corrected.
 *   Was: 0x04 = RZQ/4 = 60 Ω (wrong)
 *   Now: 0x06 = RZQ/6 = 40 Ω (blob: lp4x_odt_ohm=40, lp4_odt_ohm=40)
 *   JEDEC JESD209-4C Table 21: OP[2:0]=110 → RZQ/6 = 40 Ω.
 *
 * FIX (LPDDR4X MR14): Initial VREF corrected.
 *   Was: 0x31 → LPDDR4X range 1, code 49 → 10+49×0.4=29.6% (too high)
 *   Now: 0x20 → range 1, code 32 → 10+32×0.4=22.8% (safe start per blob)
 *
 * FIX (LPDDR4X MR22 CA ODT): Corrected from 60 Ω to 120 Ω.
 *   Was: 0x04 = 60 Ω
 *   Now: 0x02 = RZQ/2 = 120 Ω (blob: lp4x_ca_odt_ohm=120)
 *
 * FIX (LPDDR4X CS ODT): lp4x_odte_cs_en=0 — CS ODT disabled for LPDDR4X.
 *   LPDDR4 had odte_cs_en=1 (CS ODT on). LPDDR4X does not.
 *   MR11[4] = ODTS CS enable; cleared for LPDDR4X.
 * ========================================================================= */
static int mc_write_mrs(ddr_chan_cfg_t *ch, uint32_t rank, uint32_t mr_addr,
                        uint32_t mr_data)
{
    uintptr_t mc = ch->mc_base;
    int ret;

    ret = poll_flag(MC_REG(mc, DDRMC_MRSTAT), MRSTAT_MR_WR_BUSY, 0,
                    DDR_INIT_TIMEOUT_US);
    if (ret)
        return DDR_TRAIN_ERR_MRS;

    writel(FIELD_PREP(MRCTRL1_MR_DATA_MASK, mr_data), MC_REG(mc, DDRMC_MRCTRL1));
    writel(FIELD_PREP(MRCTRL0_MR_RANK_MASK, 1u << rank) |
           FIELD_PREP(MRCTRL0_MR_ADDR_MASK, mr_addr)    |
           MRCTRL0_MR_WR,
           MC_REG(mc, DDRMC_MRCTRL0));

    return poll_flag(MC_REG(mc, DDRMC_MRSTAT), MRSTAT_MR_WR_BUSY, 0,
                     DDR_INIT_TIMEOUT_US);
}

int ddr_dram_init(ddr_chan_cfg_t *ch)
{
    const ddr_timing_t *t = &ch->timing;
    int rank, ret;

    for (rank = 0; rank < ch->ranks; rank++) {

        if (ch->type == DDR_TYPE_LPDDR4 || ch->type == DDR_TYPE_LPDDR4X) {

            /* MR1: read latency / nWR */
            uint8_t rl_code;
            switch (t->cl) {
            case 10: rl_code = 0; break;
            case 14: rl_code = 1; break;
            case 16: rl_code = 2; break;
            case 22: rl_code = 3; break;
            case 26: rl_code = 4; break;
            case 30: rl_code = 5; break;
            default: rl_code = 2; break;
            }
            {
                uint8_t nwr = (uint8_t)((ps_to_clocks(t->tWR,
                                        t->freq_mhz) - 10 + 1) / 2);
                ret = mc_write_mrs(ch, rank, LPDDR4_MR1,
                                   (uint32_t)((nwr << 5) | rl_code));
                if (ret) return ret;
            }

            /* MR2: WL set */
            ret = mc_write_mrs(ch, rank, LPDDR4_MR2, rl_code);
            if (ret) return ret;

            /* MR3: pull-up calibration */
            ret = mc_write_mrs(ch, rank, LPDDR4_MR3, 0x31);
            if (ret) return ret;

            /*
             * MR11: ODT control
             * DQ ODT = 40 Ω for both LPDDR4 and LPDDR4X (blob: lp4x_odt_ohm=40)
             * JEDEC Table 21: OP[2:0]=110 = RZQ/6 = 40 Ω → code 0x06
             *
             * CS ODT (MR11[4]):
             *   LPDDR4:  lp4_odte_cs_en=1  → set   BIT(4)
             *   LPDDR4X: lp4x_odte_cs_en=0 → clear BIT(4)
             */
            {
                uint8_t mr11 = LPDDR4_MR11_ODT_40OHM;
                if (ch->type == DDR_TYPE_LPDDR4)
                    mr11 |= BIT(4);   /* CS ODT on for LPDDR4 only */
                ret = mc_write_mrs(ch, rank, LPDDR4_MR11, mr11);
                if (ret) return ret;
            }

            /* MR13: FSP-OP[0]=0, DBI off */
            ret = mc_write_mrs(ch, rank, LPDDR4_MR13, 0x00);
            if (ret) return ret;

            /*
             * MR14: initial DQ VREF before VREF sweep.
             * LPDDR4X: 0x20 → range 1, code 32 → 22.8%
             * LPDDR4:  0x31 → range 1, code 49 → 29.6%
             */
            {
                uint8_t vref_init = (ch->type == DDR_TYPE_LPDDR4X)
                                    ? LPDDR4X_MR14_VREF_INIT
                                    : LPDDR4_MR14_VREF_INIT;
                ret = mc_write_mrs(ch, rank, LPDDR4_MR14, vref_init);
                if (ret) return ret;
            }

            /*
             * MR22: SOC-ODT (CA ODT from DRAM side)
             * LPDDR4X: 120 Ω (blob: lp4x_ca_odt_ohm=120)
             *   JEDEC: OP[2:0]=010 = RZQ/2 = 120 Ω → code 0x02
             * LPDDR4:  120 Ω (blob: lp4_ca_odt_ohm=120, same)
             */
            ret = mc_write_mrs(ch, rank, LPDDR4_MR22,
                               LPDDR4_MR22_SOCOCT_120OHM);
            if (ret) return ret;

        } else if (ch->type == DDR_TYPE_LPDDR5) {

            ret = mc_write_mrs(ch, rank, LPDDR5_MR1,  0x04);
            if (ret) return ret;
            ret = mc_write_mrs(ch, rank, LPDDR5_MR2,  0x16);
            if (ret) return ret;
            ret = mc_write_mrs(ch, rank, LPDDR5_MR3,  0x00);
            if (ret) return ret;
            ret = mc_write_mrs(ch, rank, LPDDR5_MR10, 0x00);
            if (ret) return ret;
            /* LP5: 40 Ω ODT — blob lp5_odt_ohm=40 */
            ret = mc_write_mrs(ch, rank, LPDDR5_MR11, 0x06);
            if (ret) return ret;
            ret = mc_write_mrs(ch, rank, LPDDR5_MR14, 0x31);
            if (ret) return ret;
            ret = mc_write_mrs(ch, rank, LPDDR5_MR15, 0x00);
            if (ret) return ret;

        } else if (ch->type == DDR_TYPE_DDR4) {

            uint8_t cwl_code = (uint8_t)((t->cwl - 9) / 2);
            ret = mc_write_mrs(ch, rank, DDR4_MR2,
                               (uint32_t)((cwl_code << 3) | (0x1 << 9)));
            if (ret) return ret;

            ret = mc_write_mrs(ch, rank, DDR4_MR1, 0x0001);
            if (ret) return ret;

            {
                uint8_t cl_code;
                switch (t->cl) {
                case 15: cl_code = 0x0; break;
                case 17: cl_code = 0x2; break;
                case 19: cl_code = 0x4; break;
                case 22: cl_code = 0x8; break;
                default: cl_code = 0x4; break;
                }
                ret = mc_write_mrs(ch, rank, DDR4_MR0,
                                   (uint32_t)((cl_code << 4) | BIT(2)));
                if (ret) return ret;
            }

            ret = mc_write_mrs(ch, rank, DDR4_MR5, 0x0400);
            if (ret) return ret;
            ret = mc_write_mrs(ch, rank, DDR4_MR6, 0x0000);
            if (ret) return ret;
        }
    }

    clrbits_le32(ch->mc_base + DDRMC_RFSHCTL3, RFSHCTL3_DIS_AUTO_REFRESH);
    clrbits_le32(ch->mc_base + DDRMC_ZQCTL0,   ZQCTL0_DIS_AUTO_ZQ);

    return DDR_TRAIN_OK;
}

/* =========================================================================
 * Step 6 — CA training (LPDDR4/5)
 * ========================================================================= */
int ddr_train_ca(ddr_chan_cfg_t *ch)
{
    uintptr_t phy = ch->phy_base;
    int lane, ret;

    if (ch->type != DDR_TYPE_LPDDR4 && ch->type != DDR_TYPE_LPDDR4X &&
        ch->type != DDR_TYPE_LPDDR5)
        return DDR_TRAIN_OK;

    setbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL), TRAIN_CTRL_CA_EN);
    setbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL), TRAIN_CTRL_START);

    ret = poll_flag(PHY_REG(phy, PHY_TRAIN_STATUS),
                    TRAIN_STATUS_BUSY, 0, DDR_TRAIN_TIMEOUT_US);
    if (ret)
        return DDR_TRAIN_ERR_CA;

    if (readl(PHY_REG(phy, PHY_TRAIN_STATUS)) & TRAIN_STATUS_FAIL_MASK)
        return DDR_TRAIN_ERR_CA;

    for (lane = 0; lane < DDR_CA_LANES; lane++)
        ch->cal.ca_delay[lane] =
            (int32_t)readl(PHY_REG(phy, PHY_CA_DESKEW(lane)));
    ch->cal.ck_delay = (int32_t)readl(PHY_REG(phy, PHY_CK_DELAY));
    ch->cal.ca_done  = 1;

    clrbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL),
                 TRAIN_CTRL_CA_EN | TRAIN_CTRL_START);
    return DDR_TRAIN_OK;
}

/* =========================================================================
 * Step 7 — Write leveling
 * ========================================================================= */
int ddr_train_wrlvl(ddr_chan_cfg_t *ch)
{
    uintptr_t phy = ch->phy_base;
    int lane, ret;

    clrsetbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL),
                    TRAIN_CTRL_GATE_EN | TRAIN_CTRL_RDDSK_EN |
                    TRAIN_CTRL_WRDSK_EN | TRAIN_CTRL_VREF_EN,
                    TRAIN_CTRL_WRLVL_EN);
    setbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL), TRAIN_CTRL_START);

    ret = poll_flag(PHY_REG(phy, PHY_TRAIN_STATUS),
                    TRAIN_STATUS_BUSY, 0, DDR_TRAIN_TIMEOUT_US);
    if (ret)
        return DDR_TRAIN_ERR_WRLVL;

    if (readl(PHY_REG(phy, PHY_TRAIN_STATUS)) & TRAIN_STATUS_FAIL_MASK)
        return DDR_TRAIN_ERR_WRLVL;

    for (lane = 0; lane < DDR_MAX_BYTES; lane++)
        ch->cal.wrlvl_dqs_delay[lane] =
            (int32_t)readl(PHY_REG(phy, PHY_LANE_WRLVL_DELAY(lane)));
    ch->cal.wrlvl_done = 1;

    clrbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL),
                 TRAIN_CTRL_WRLVL_EN | TRAIN_CTRL_START);
    return DDR_TRAIN_OK;
}

/* =========================================================================
 * Step 8 — Gate training
 * ========================================================================= */
int ddr_train_gate(ddr_chan_cfg_t *ch)
{
    uintptr_t phy = ch->phy_base;
    int lane, ret;

    clrsetbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL),
                    TRAIN_CTRL_WRLVL_EN | TRAIN_CTRL_RDDSK_EN |
                    TRAIN_CTRL_WRDSK_EN | TRAIN_CTRL_VREF_EN,
                    TRAIN_CTRL_GATE_EN);
    setbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL), TRAIN_CTRL_START);

    ret = poll_flag(PHY_REG(phy, PHY_TRAIN_STATUS),
                    TRAIN_STATUS_BUSY, 0, DDR_TRAIN_TIMEOUT_US);
    if (ret)
        return DDR_TRAIN_ERR_GATE;

    if (readl(PHY_REG(phy, PHY_TRAIN_STATUS)) & TRAIN_STATUS_FAIL_MASK)
        return DDR_TRAIN_ERR_GATE;

    for (lane = 0; lane < DDR_MAX_BYTES; lane++)
        ch->cal.gate_delay[lane] =
            (int32_t)readl(PHY_REG(phy, PHY_LANE_GATE_DELAY(lane)));
    ch->cal.gate_done = 1;

    clrbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL),
                 TRAIN_CTRL_GATE_EN | TRAIN_CTRL_START);
    return DDR_TRAIN_OK;
}

/* =========================================================================
 * Step 9 — Read DQ deskew
 * ========================================================================= */
int ddr_train_rd_deskew(ddr_chan_cfg_t *ch)
{
    uintptr_t phy = ch->phy_base;
    int lane, bit, ret;

    clrsetbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL),
                    TRAIN_CTRL_WRLVL_EN | TRAIN_CTRL_GATE_EN |
                    TRAIN_CTRL_WRDSK_EN | TRAIN_CTRL_VREF_EN,
                    TRAIN_CTRL_RDDSK_EN);
    setbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL), TRAIN_CTRL_START);

    ret = poll_flag(PHY_REG(phy, PHY_TRAIN_STATUS),
                    TRAIN_STATUS_BUSY, 0, DDR_TRAIN_TIMEOUT_US);
    if (ret)
        return DDR_TRAIN_ERR_RD_DESKEW;

    if (readl(PHY_REG(phy, PHY_TRAIN_STATUS)) & TRAIN_STATUS_FAIL_MASK)
        return DDR_TRAIN_ERR_RD_DESKEW;

    for (lane = 0; lane < DDR_MAX_BYTES; lane++) {
        uint32_t lo = readl(PHY_REG(phy, PHY_LANE_BIT_RX_DESKEW_L(lane)));
        uint32_t hi = readl(PHY_REG(phy, PHY_LANE_BIT_RX_DESKEW_H(lane)));
        for (bit = 0; bit < 4; bit++)
            ch->cal.rd_dq_delay[lane][bit]   = (int32_t)((lo >> (bit * 8)) & 0x7F);
        for (bit = 0; bit < 4; bit++)
            ch->cal.rd_dq_delay[lane][bit+4] = (int32_t)((hi >> (bit * 8)) & 0x7F);
        ch->cal.rd_dqs_delay[lane] =
            (int32_t)readl(PHY_REG(phy, PHY_LANE_DQS_RX_DELAY(lane)));
        {
            uint32_t eye = readl(PHY_REG(phy, PHY_LANE_RD_EYE(lane)));
            ch->cal.rd_eye_width[lane] =
                (uint8_t)FIELD_GET(RD_EYE_WIDTH_MASK, eye);
        }
    }
    ch->cal.rd_deskew_done = 1;

    clrbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL),
                 TRAIN_CTRL_RDDSK_EN | TRAIN_CTRL_START);
    return DDR_TRAIN_OK;
}

/* =========================================================================
 * Step 10 — Write DQ deskew
 * ========================================================================= */
int ddr_train_wr_deskew(ddr_chan_cfg_t *ch)
{
    uintptr_t phy = ch->phy_base;
    int lane, bit, ret;

    clrsetbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL),
                    TRAIN_CTRL_WRLVL_EN | TRAIN_CTRL_GATE_EN |
                    TRAIN_CTRL_RDDSK_EN | TRAIN_CTRL_VREF_EN,
                    TRAIN_CTRL_WRDSK_EN);
    setbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL), TRAIN_CTRL_START);

    ret = poll_flag(PHY_REG(phy, PHY_TRAIN_STATUS),
                    TRAIN_STATUS_BUSY, 0, DDR_TRAIN_TIMEOUT_US);
    if (ret)
        return DDR_TRAIN_ERR_WR_DESKEW;

    if (readl(PHY_REG(phy, PHY_TRAIN_STATUS)) & TRAIN_STATUS_FAIL_MASK)
        return DDR_TRAIN_ERR_WR_DESKEW;

    for (lane = 0; lane < DDR_MAX_BYTES; lane++) {
        uint32_t lo = readl(PHY_REG(phy, PHY_LANE_BIT_TX_DESKEW_L(lane)));
        uint32_t hi = readl(PHY_REG(phy, PHY_LANE_BIT_TX_DESKEW_H(lane)));
        for (bit = 0; bit < 4; bit++)
            ch->cal.wr_dq_delay[lane][bit]   = (int32_t)((lo >> (bit * 8)) & 0x7F);
        for (bit = 0; bit < 4; bit++)
            ch->cal.wr_dq_delay[lane][bit+4] = (int32_t)((hi >> (bit * 8)) & 0x7F);
        ch->cal.wr_dqs_delay[lane] =
            (int32_t)readl(PHY_REG(phy, PHY_LANE_DQS_TX_DELAY(lane)));
        {
            uint32_t eye = readl(PHY_REG(phy, PHY_LANE_WR_EYE(lane)));
            ch->cal.wr_eye_width[lane] = (uint8_t)(eye & 0xFF);
        }
    }
    ch->cal.wr_deskew_done = 1;

    clrbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL),
                 TRAIN_CTRL_WRDSK_EN | TRAIN_CTRL_START);
    return DDR_TRAIN_OK;
}

/* =========================================================================
 * Step 11 — VREF training
 *
 * FIX — Critical: original used a fake hardcoded pass window:
 *   bool pass = (step >= 16 && step <= 48);  // always true for those steps
 * This always selected VREF code ~32 regardless of actual DRAM response.
 *
 * Now: each candidate VREF step is evaluated by ddr_bist() — a real
 * hardware write/read pattern test. Only steps where BIST passes are
 * counted as part of the pass window. The midpoint of the widest
 * contiguous passing window is selected and programmed.
 *
 * Also fixed:
 *  - Initial VREF set to LPDDR4X_MR14_VREF_INIT (0x20=22.8%) before sweep.
 *  - tVREFDQE settling delay added: 150 ns minimum after each MR14 write
 *    before sampling. Original had no settling delay.
 *  - Sweep limited to MR14 range 1 (codes 0–43, up to 27.2%) for safety.
 *    Extend to range 2 if your specific DRAM requires it.
 * ========================================================================= */
int ddr_train_vref(ddr_chan_cfg_t *ch)
{
    uintptr_t phy = ch->phy_base;
    int rank, lane, step, ret = DDR_TRAIN_OK;
    uint8_t best_vref_host = PHY_VREF_STEPS / 2;
    uint8_t best_vref_dram = (ch->type == DDR_TYPE_LPDDR4X)
                              ? LPDDR4X_MR14_VREF_INIT
                              : LPDDR4_MR14_VREF_INIT;

    /* Phase A: host-side VREF via hardware PHY engine */
    clrsetbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL),
                    TRAIN_CTRL_WRLVL_EN | TRAIN_CTRL_GATE_EN |
                    TRAIN_CTRL_RDDSK_EN | TRAIN_CTRL_WRDSK_EN,
                    TRAIN_CTRL_VREF_EN);
    setbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL), TRAIN_CTRL_START);

    ret = poll_flag(PHY_REG(phy, PHY_TRAIN_STATUS),
                    TRAIN_STATUS_BUSY, 0, DDR_TRAIN_TIMEOUT_US);
    if (ret)
        return DDR_TRAIN_ERR_VREF;

    if (readl(PHY_REG(phy, PHY_TRAIN_STATUS)) & TRAIN_STATUS_FAIL_MASK)
        return DDR_TRAIN_ERR_VREF;

    for (lane = 0; lane < DDR_MAX_BYTES; lane++) {
        uint32_t v = readl(PHY_REG(phy, PHY_LANE_VREF_DQ(lane)));
        best_vref_host = (uint8_t)(v & 0x3F);
    }

    /* Phase B: DRAM-side VREF sweep via MR14 + BIST */
    if (ch->type == DDR_TYPE_LPDDR4 || ch->type == DDR_TYPE_LPDDR4X ||
        ch->type == DDR_TYPE_LPDDR5) {

        uint8_t best_start = 0, best_len = 0;
        uint8_t cur_start  = 0, cur_len  = 0;
        bool    in_pass = false;

        /*
         * Set initial VREF before starting the sweep.
         * This ensures the DRAM is in a known-good state before we step
         * through the range. tVREFDQE = 150 ns minimum settling time.
         */
        for (rank = 0; rank < ch->ranks; rank++) {
            ret = mc_write_mrs(ch, rank, LPDDR4_MR14, best_vref_dram);
            if (ret) goto vref_exit;
        }
        udelay(1);   /* 150 ns at 24 MHz = ceil(0.15*24) = 4 ticks ≈ 1 us */

        /*
         * Sweep MR14 range 1: codes 0..43 → 10.0%..27.2%.
         * Use range 2 (MR14[6]=1, codes 0..41 → 22.0%..38.5%) if the
         * DRAM datasheet recommends a higher initial VREF for your chip.
         */
        for (step = 0; step < 44; step++) {
            uint8_t mr_val = (uint8_t)step;  /* range 1: MR14[6]=0 */

            for (rank = 0; rank < ch->ranks; rank++) {
                ret = mc_write_mrs(ch, rank, LPDDR4_MR14, mr_val);
                if (ret) goto vref_exit;
            }
            udelay(1);   /* tVREFDQE settling — do not remove */

            /*
             * Evaluate this VREF step with a real BIST read/write test.
             * 4 MiB is enough to catch lane errors without excessive time.
             * BIST returns DDR_TRAIN_OK (0) on pass, non-zero on failure.
             */
            bool pass = (ddr_bist(ch, 4) == DDR_TRAIN_OK);

            if (pass) {
                if (!in_pass) {
                    cur_start = (uint8_t)step;
                    cur_len   = 0;
                    in_pass   = true;
                }
                cur_len++;
                if (cur_len > best_len) {
                    best_len   = cur_len;
                    best_start = cur_start;
                }
            } else {
                in_pass = false;
            }
        }

        if (best_len == 0) {
            /* No passing window found — training failure */
            ret = DDR_TRAIN_ERR_VREF;
            goto vref_exit;
        }

        best_vref_dram = best_start + best_len / 2;

        /* Program the winning VREF into all ranks */
        for (rank = 0; rank < ch->ranks; rank++) {
            ret = mc_write_mrs(ch, rank, LPDDR4_MR14, best_vref_dram);
            if (ret) goto vref_exit;
        }
        udelay(1);

    } else if (ch->type == DDR_TYPE_DDR4) {

        uint8_t best_start = 0, best_len = 0;
        uint8_t cur_start  = 0, cur_len  = 0;
        bool    in_pass = false;

        for (rank = 0; rank < ch->ranks; rank++) {
            ret = mc_write_mrs(ch, rank, DDR4_MR6, BIT(7));
            if (ret) goto vref_exit;
        }

        for (step = 0; step < 50; step++) {
            uint16_t mr6_val = (uint16_t)(BIT(7) | step);
            for (rank = 0; rank < ch->ranks; rank++) {
                ret = mc_write_mrs(ch, rank, DDR4_MR6, mr6_val);
                if (ret) goto vref_exit;
            }
            udelay(1);   /* tVREFDQE */

            bool pass = (ddr_bist(ch, 4) == DDR_TRAIN_OK);
            if (pass) {
                if (!in_pass) { cur_start = (uint8_t)step; cur_len = 0; in_pass = true; }
                cur_len++;
                if (cur_len > best_len) { best_len = cur_len; best_start = cur_start; }
            } else { in_pass = false; }
        }

        best_vref_dram = best_start + best_len / 2;
        for (rank = 0; rank < ch->ranks; rank++) {
            ret = mc_write_mrs(ch, rank, DDR4_MR6, best_vref_dram);
            if (ret) goto vref_exit;
        }
    }

vref_exit:
    ch->cal.vref_dq_host = best_vref_host;
    ch->cal.vref_dq_dram = best_vref_dram;
    ch->cal.vref_done    = (ret == DDR_TRAIN_OK) ? 1 : 0;

    clrbits_le32(PHY_REG(phy, PHY_TRAIN_CTRL),
                 TRAIN_CTRL_VREF_EN | TRAIN_CTRL_START);
    return ret;
}

/* =========================================================================
 * Step 12 — BIST
 * ========================================================================= */
int ddr_bist(ddr_chan_cfg_t *ch, uint32_t test_size_mb)
{
    uintptr_t grf     = DDRGRF_BASE;
    uint32_t  len_lines = (test_size_mb << 20) / 64;

    writel(0x00000000, GRF_REG(grf, DDRGRF_BIST_ADDR));
    writel(len_lines,  GRF_REG(grf, DDRGRF_BIST_LEN));
    writel(BIST_CTRL_EN | FIELD_PREP(BIST_CTRL_PATTERN_MASK, 0x5),
           GRF_REG(grf, DDRGRF_BIST_CTRL));

    int ret = poll_flag(GRF_REG(grf, DDRGRF_BIST_STATUS),
                        BIST_STATUS_BUSY, 0, DDR_BIST_TIMEOUT_US);
    if (ret)
        return DDR_TRAIN_ERR_BIST;

    if (readl(GRF_REG(grf, DDRGRF_BIST_STATUS)) & BIST_STATUS_FAIL)
        return DDR_TRAIN_ERR_BIST;

    return DDR_TRAIN_OK;
}

/* =========================================================================
 * Timing resolution
 * ========================================================================= */
void ddr_calc_timings(ddr_chan_cfg_t *ch)
{
    const ddr_timing_t *src = ddr_get_timing(ch->type, ch->timing.freq_mhz);
    if (src)
        ch->timing = *src;
    else
        ch->timing.freq_mhz = 528;   /* safe fallback — FSP0 */
}

void ddr_dump_phy_results(const ddr_chan_cfg_t *ch) { (void)ch; }

/* =========================================================================
 * MSCH scheduler
 * ========================================================================= */
static void ddr_msch_init(ddr_chan_cfg_t *ch)
{
    uintptr_t msch = MSCH_BASE + ch->channel * 0x100;
    const ddr_timing_t *t = &ch->timing;
    uint32_t freq = t->freq_mhz;

    writel(FIELD_PREP(MSCH_DDRTIMING_ACT_TO_ACT_MASK,
                      ps_to_clocks(t->tRRD_L, freq)) |
           FIELD_PREP(MSCH_DDRTIMING_RD_TO_MISS_MASK,
                      t->rl + 4 + ps_to_clocks(t->tRTP, freq) -
                      ps_to_clocks(t->tRP, freq)) |
           FIELD_PREP(MSCH_DDRTIMING_WR_TO_MISS_MASK,
                      t->wl + t->bl / 2 + ps_to_clocks(t->tWR, freq) +
                      ps_to_clocks(t->tRP, freq) +
                      ps_to_clocks(t->tRCD, freq)) |
           FIELD_PREP(MSCH_DDRTIMING_BURST_MASK, t->bl / 2),
           msch + MSCH_DDRTIMING);

    writel(t->rl + 4, msch + MSCH_READLATENCY);
}

/* =========================================================================
 * Per-channel training sequence
 * ========================================================================= */
static int ddr_train_channel(ddr_cfg_t *cfg, ddr_chan_cfg_t *ch)
{
    int ret;

    if (!ch->enabled)
        return DDR_TRAIN_OK;

    ddr_calc_timings(ch);
    ddr_release_reset(ch->channel);

    ret = ddr_phy_init(ch);      if (ret) return ret;

    if (cfg->do_zq) {
        ret = ddr_train_zq(ch);  if (ret) return ret;
    }

    ret = ddr_mc_init(ch);       if (ret) return ret;
    ret = ddr_dram_init(ch);     if (ret) return ret;

    if (cfg->do_ca_training) {
        ret = ddr_train_ca(ch);  if (ret) return ret;
    }
    if (cfg->do_wrlvl) {
        ret = ddr_train_wrlvl(ch); if (ret) return ret;
    }
    if (cfg->do_gate) {
        ret = ddr_train_gate(ch);  if (ret) return ret;
    }
    if (cfg->do_rd_deskew) {
        ret = ddr_train_rd_deskew(ch); if (ret) return ret;
    }
    if (cfg->do_wr_deskew) {
        ret = ddr_train_wr_deskew(ch); if (ret) return ret;
    }
    if (cfg->do_vref) {
        ret = ddr_train_vref(ch); if (ret) return ret;
    }

    ddr_msch_init(ch);

    ret = ddr_bist(ch, 32);
    if (ret) return ret;

    cfg->total_size_mb += ch->size_mb;
    return DDR_TRAIN_OK;
}

/* =========================================================================
 * ddr_init_and_train — main entry point
 *
 * FIX: PLL now set to FSP0 frequency (528 MHz), not target.
 *   Original set the PLL to target freq (1600 MHz) before training.
 *   The blob boots at FSP0=528 MHz (boot_fsp=0, lp4x_f1=528).
 *   Training cold at full speed causes narrow timing margins and is
 *   the primary reason open DDR trainers fail on this SoC.
 *
 * FIX: Added DDRGRF bank mask and rank mask programming.
 *   blob: lp4_4x_bank_mask0=0x800 (bit 11), mask1=0x1000 (bit 12),
 *         mask2=0x2000 (bit 13), lp4_4x_rank_mask0=0x400000 (bit 22).
 *   These tell the DDRGRF which HIF address bits select bank and rank.
 *   Without them, multi-bank refresh and rank interleaving are broken.
 *
 * FIX: Channel interleave now programmed to DDRGRF_COMMON_CON0 (0x0540).
 *   Original used DDRGRF_CON0 (0x0004) which is a per-channel config
 *   register, not the dual-channel interleave register.
 * ========================================================================= */
int ddr_init_and_train(ddr_cfg_t *cfg)
{
    int i, ret;

    /*
     * Set PLL to FSP0 frequency.
     * blob: boot_fsp=0 → lp4x_f1_freq_mhz=528.
     * Training at 528 MHz gives >4× more timing margin than 1600 MHz.
     */
    for (i = 0; i < DDR_MAX_CHANNELS; i++) {
        if (cfg->chan[i].enabled) {
            ddr_chan_cfg_t *ch = &cfg->chan[i];
            uint32_t boot_freq = ch->fsp_freq_mhz[ch->boot_fsp];

            /* Set timing struct to boot FSP frequency */
            ch->timing.freq_mhz = boot_freq;

            ret = ddr_cru_set_pll(boot_freq);
            if (ret)
                return ret;
            break;
        }
    }

    /* Train each channel */
    for (i = 0; i < DDR_MAX_CHANNELS; i++) {
        ret = ddr_train_channel(cfg, &cfg->chan[i]);
        if (ret)
            return ret;
    }

    /*
     * Program DDRGRF address interleave masks.
     * These values come directly from the blob:
     *   lp4_4x_bank_mask0 = 0x800  → HIF bit 11 selects bank[0]
     *   lp4_4x_bank_mask1 = 0x1000 → HIF bit 12 selects bank[1]
     *   lp4_4x_bank_mask2 = 0x2000 → HIF bit 13 selects bank[2]
     *   lp4_4x_bank_mask3 = 0x0    → unused
     *   lp4_4x_rank_mask0 = 0x400000 → HIF bit 22 selects rank
     *
     * Program both channels identically. The DDRGRF write-mask format:
     * bits[31:16] = write-enable mask, bits[15:0] = value. However these
     * registers hold full 32-bit addresses, so write directly.
     */
    for (i = 0; i < DDR_MAX_CHANNELS; i++) {
        uintptr_t cha_base = DDRGRF_BASE + (i == 0 ? 0x0000 : 0x0100);

        writel(0x00000800, cha_base + (i == 0
               ? DDRGRF_CHA_BANK_MASK0 - DDRGRF_CHA_CON0
               : DDRGRF_CHB_BANK_MASK0 - DDRGRF_CHB_CON0));
        writel(0x00001000, cha_base + (i == 0
               ? DDRGRF_CHA_BANK_MASK1 - DDRGRF_CHA_CON0
               : DDRGRF_CHB_BANK_MASK1 - DDRGRF_CHB_CON0));
        writel(0x00002000, cha_base + (i == 0
               ? DDRGRF_CHA_BANK_MASK2 - DDRGRF_CHA_CON0
               : DDRGRF_CHB_BANK_MASK2 - DDRGRF_CHB_CON0));
        writel(0x00000000, cha_base + (i == 0
               ? DDRGRF_CHA_BANK_MASK3 - DDRGRF_CHA_CON0
               : DDRGRF_CHB_BANK_MASK3 - DDRGRF_CHB_CON0));
        writel(0x00400000, cha_base + (i == 0
               ? DDRGRF_CHA_RANK_MASK0 - DDRGRF_CHA_CON0
               : DDRGRF_CHB_RANK_MASK0 - DDRGRF_CHB_CON0));
    }

    /*
     * Dual-channel interleave — DDRGRF_COMMON_CON0 at 0x0540.
     * blob: stride_type=3 → 64-byte cache-line interleave.
     * FIX: original wrote to DDRGRF_CON0 (0x0004) which is wrong register.
     */
    if (cfg->active_channels == 2) {
        grf_write(DDRGRF_BASE + DDRGRF_COMMON_CON0,
                  GRF_CON0_SPLIT_EN | GRF_CON0_SPLIT_MASK,
                  GRF_CON0_SPLIT_EN |
                  FIELD_PREP(GRF_CON0_SPLIT_MASK, GRF_CON0_SPLIT_64B));
    }

    return DDR_TRAIN_OK;
}
