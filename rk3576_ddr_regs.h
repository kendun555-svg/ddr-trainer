/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * RK3576 DDR register definitions
 *
 * DDRMC: Synopsys uMCTL2-derived memory controller
 * DDRPHY: Rockchip/Cadence combo PHY (lpddr4/lpddr5/ddr4/ddr5)
 * DDRGRF: Rockchip DDR Global Register File
 * CRU:    Clock and Reset Unit (DDR PLL entries)
 *
 * Corrections vs original (derived from rkbin v1.09 blob analysis):
 *   - Added DDRMC_DERATEEN / DERATEEN_DERATE_EN (derate_en=1 in blob)
 *   - Added RFSHCTL0_PER_BANK_REFRESH (per_bank_ref_en=1 in blob)
 *   - Added DDRGRF channel-A/B bank and rank mask registers
 *   - Added DDRGRF common_con interleave register (correct offset 0x0540)
 *   - Fixed CRU_SOFTRST offset: struct comment says 0x400 but macro is 0xa00;
 *     RK3576_SOFTRST_CON(x) = x*4 + 0xa00 per cru_rk3576.h in Rockchip U-Boot
 *   - Added PHY_CON2 TSEL codes for 30 Ω and ODT codes for 40 Ω
 */

#ifndef RK3576_DDR_REGS_H
#define RK3576_DDR_REGS_H

/* =========================================================================
 * Helper macros
 * ========================================================================= */
#define MC_REG(base, off)   ((base) + (off))
#define PHY_REG(base, off)  ((base) + (off))
#define GRF_REG(base, off)  ((base) + (off))

/* =========================================================================
 * DDRMC — Memory Controller Registers  (uMCTL2-compatible layout)
 * ========================================================================= */

/* --- Global / miscellaneous ---- */
#define DDRMC_MSTR              0x0000  /* Master register */
#define  MSTR_DDR3              BIT(0)
#define  MSTR_LPDDR2            BIT(2)
#define  MSTR_LPDDR3            BIT(3)
#define  MSTR_DDR4              BIT(4)
#define  MSTR_LPDDR4            BIT(5)
#define  MSTR_LPDDR5            BIT(6)
#define  MSTR_DDR5              BIT(7)
#define  MSTR_BURST_RDWR_MASK   GENMASK(19, 16)
#define  MSTR_ACTIVE_RANKS_MASK GENMASK(27, 24)
#define  MSTR_DEVICE_CONFIG_MASK GENMASK(31, 30)  /* 00=x8,01=x16,10=x32 */

#define DDRMC_STAT              0x0004  /* Operating mode status */
#define  STAT_OP_MODE_MASK      GENMASK(2, 0)
#define  STAT_OP_INIT           0x0
#define  STAT_OP_NORMAL         0x1
#define  STAT_OP_POWER_DOWN     0x2
#define  STAT_OP_SELF_REFRESH   0x3

#define DDRMC_MSTR1             0x0008
#define DDRMC_MRCTRL0           0x0010  /* Mode register read/write control */
#define  MRCTRL0_MR_TYPE        BIT(0)  /* 0=write, 1=read */
#define  MRCTRL0_MPR_EN         BIT(1)
#define  MRCTRL0_MR_RANK_MASK   GENMASK(5, 4)
#define  MRCTRL0_MR_ADDR_MASK   GENMASK(19, 12)
#define  MRCTRL0_MR_WR          BIT(31) /* trigger MRS (auto-clear) */

#define DDRMC_MRCTRL1           0x0014
#define  MRCTRL1_MR_DATA_MASK   GENMASK(17, 0)

#define DDRMC_MRSTAT            0x0018
#define  MRSTAT_MR_WR_BUSY      BIT(0)

/* --- Refresh ---- */
#define DDRMC_RFSHCTL0          0x0050
/*
 * PER_BANK_REFRESH — enables per-bank refresh (LPDDR4/5 feature).
 * The blob has per_bank_ref_en=1; set this bit after DRAM init.
 * uMCTL2 RFSHCTL0[2] = per_bank_refresh (also called per_bank_refresh_en
 * in some Rockchip-specific forks).
 */
#define  RFSHCTL0_PER_BANK_REFRESH  BIT(2)

#define DDRMC_RFSHCTL1          0x0054
#define DDRMC_RFSHCTL2          0x0058
#define DDRMC_RFSHCTL3          0x0060
#define  RFSHCTL3_DIS_AUTO_REFRESH BIT(0)

#define DDRMC_RFSHTMG           0x0064
#define  RFSHTMG_T_RFC_NOM_X1_X32_MASK GENMASK(27, 16)
#define  RFSHTMG_T_RFC_MIN_MASK         GENMASK(9, 0)

/* --- Temperature derating ---- */
/*
 * DERATEEN — enables temperature-based timing derating (JEDEC mandatory for
 * LPDDR4 at > 85 °C).  The blob has derate_en=1.
 * uMCTL2 DERATEEN[0] = derate_enable.
 * Without this, tRCD/tRP/tRC are under-margined at elevated temperature.
 */
#define DDRMC_DERATEEN          0x0020
#define  DERATEEN_DERATE_EN     BIT(0)
#define  DERATEEN_DERATE_BYTE   BIT(1)   /* 0=tRCD only, 1=tRCD+tRP+tRC */

/* --- ECC ---- */
#define DDRMC_ECCCFG0           0x0070
#define  ECCCFG0_ECC_MODE_MASK  GENMASK(2, 0)
#define DDRMC_ECCCFG1           0x0074
#define DDRMC_ECCSTAT           0x0078

/* --- CRC/parity ---- */
#define DDRMC_CRCPARCTL0        0x00C0
#define DDRMC_CRCPARSTAT        0x00CC

/* --- Init ---- */
#define DDRMC_INIT0             0x00D0
#define  INIT0_SKIP_DRAM_INIT_MASK  GENMASK(31, 30)
#define  INIT0_SKIP_DRAM_INIT_NONE  0
#define  INIT0_SKIP_DRAM_INIT_NORM  3   /* controller handles MRS */

#define DDRMC_INIT1             0x00D4
#define DDRMC_INIT2             0x00D8
#define DDRMC_INIT3             0x00DC
#define  INIT3_MR_MASK          GENMASK(15, 0)
#define  INIT3_EMR_MASK         GENMASK(31, 16)

#define DDRMC_INIT4             0x00E0
#define DDRMC_INIT5             0x00E4
#define DDRMC_INIT6             0x00E8
#define DDRMC_INIT7             0x00EC

/* --- DRAM timings ---- */
#define DDRMC_DRAMTMG0          0x0100
#define  DRAMTMG0_T_RAS_MIN_MASK  GENMASK(5, 0)
#define  DRAMTMG0_T_RAS_MAX_MASK  GENMASK(14, 8)
#define  DRAMTMG0_T_FAW_MASK      GENMASK(21, 16)
#define  DRAMTMG0_WR2PRE_MASK     GENMASK(29, 24)

#define DDRMC_DRAMTMG1          0x0104
#define  DRAMTMG1_T_RC_MASK       GENMASK(6, 0)
#define  DRAMTMG1_RD2PRE_MASK     GENMASK(12, 8)
#define  DRAMTMG1_T_XP_MASK       GENMASK(20, 16)

#define DDRMC_DRAMTMG2          0x0108
#define  DRAMTMG2_WR2RD_MASK      GENMASK(5, 0)
#define  DRAMTMG2_RD2WR_MASK      GENMASK(13, 8)
#define  DRAMTMG2_READ_LATENCY_MASK  GENMASK(21, 16)
#define  DRAMTMG2_WRITE_LATENCY_MASK GENMASK(29, 24)

#define DDRMC_DRAMTMG3          0x010C
#define DDRMC_DRAMTMG4          0x0110
#define  DRAMTMG4_T_RP_MASK       GENMASK(4, 0)
#define  DRAMTMG4_T_RRD_S_MASK    GENMASK(11, 8)
#define  DRAMTMG4_T_RCD_MASK      GENMASK(19, 16)
#define  DRAMTMG4_T_RRD_L_MASK    GENMASK(27, 24)

#define DDRMC_DRAMTMG5          0x0114
#define DDRMC_DRAMTMG6          0x0118
#define DDRMC_DRAMTMG7          0x011C
#define DDRMC_DRAMTMG8          0x0120
#define DDRMC_DRAMTMG9          0x0124
#define DDRMC_DRAMTMG10         0x0128
#define DDRMC_DRAMTMG11         0x012C
#define DDRMC_DRAMTMG12         0x0130
#define DDRMC_DRAMTMG13         0x0134
#define DDRMC_DRAMTMG14         0x0138
#define DDRMC_DRAMTMG15         0x013C
#define DDRMC_DRAMTMG17         0x0144

/* --- ZQ ---- */
#define DDRMC_ZQCTL0            0x0180
#define  ZQCTL0_T_ZQ_SHORT_NOP_MASK  GENMASK(19, 0)
#define  ZQCTL0_T_ZQ_LONG_NOP_MASK   GENMASK(29, 20)
#define  ZQCTL0_DIS_AUTO_ZQ          BIT(31)
#define DDRMC_ZQCTL1            0x0184
#define DDRMC_ZQCTL2            0x0188
#define DDRMC_ZQSTAT            0x018C

/* --- DFI timing ---- */
#define DDRMC_DFITMG0           0x0190
#define  DFITMG0_DFI_TPHY_WRLAT_MASK     GENMASK(5, 0)
#define  DFITMG0_DFI_TPHY_WRDATA_MASK    GENMASK(13, 8)
#define  DFITMG0_DFI_T_RDDATA_EN_MASK    GENMASK(22, 16)
#define  DFITMG0_DFI_CTRL_DELAY_MASK     GENMASK(28, 24)

#define DDRMC_DFITMG1           0x0194
#define DDRMC_DFILPCFG0         0x0198
#define DDRMC_DFILPCFG1         0x019C
#define DDRMC_DFIUPD0           0x01A0
#define DDRMC_DFIUPD1           0x01A4
#define DDRMC_DFIUPD2           0x01A8
#define DDRMC_DFIMISC           0x01B0
#define  DFIMISC_DFI_INIT_COMPLETE_EN BIT(0)
#define  DFIMISC_DFI_INIT_START       BIT(5)

#define DDRMC_DFISTAT           0x01BC
#define  DFISTAT_DFI_INIT_COMPLETE    BIT(0)

#define DDRMC_DFITMG2           0x01C4
#define DDRMC_DFITMG3           0x01C8

/* --- Address map ---- */
#define DDRMC_ADDRMAP0          0x0200
#define DDRMC_ADDRMAP1          0x0204
#define DDRMC_ADDRMAP2          0x0208
#define DDRMC_ADDRMAP3          0x020C
#define DDRMC_ADDRMAP4          0x0210
#define DDRMC_ADDRMAP5          0x0214
#define DDRMC_ADDRMAP6          0x0218
#define DDRMC_ADDRMAP7          0x021C
#define DDRMC_ADDRMAP8          0x0220
#define DDRMC_ADDRMAP9          0x0224
#define DDRMC_ADDRMAP10         0x0228
#define DDRMC_ADDRMAP11         0x022C

/* --- ODT ---- */
#define DDRMC_ODTCFG            0x0240
#define DDRMC_ODTMAP            0x0244

/* --- Scheduler / QoS ---- */
#define DDRMC_SCHED             0x0250
#define DDRMC_SCHED1            0x0254
#define DDRMC_PERFHPR1          0x025C
#define DDRMC_PERFLPR1          0x0264
#define DDRMC_PERFWR1           0x026C

/* --- DBG ---- */
#define DDRMC_DBG0              0x0300
#define DDRMC_DBG1              0x0304
#define DDRMC_DBGSTAT           0x0308
#define  DBGSTAT_DBG_STALL      BIT(0)

/* --- Power-down / self-refresh ---- */
#define DDRMC_PWRCTL            0x0030
#define  PWRCTL_SELFREF_EN      BIT(0)
#define  PWRCTL_POWERDOWN_EN    BIT(1)
#define  PWRCTL_EN_DFI_DRAM_CLK_DISABLE BIT(3)
#define  PWRCTL_SELFREF_SW      BIT(5)

#define DDRMC_PWRTMG            0x0034
#define DDRMC_HWLPCTL           0x0038

/* =========================================================================
 * DDRPHY — Rockchip RK3576 combo PHY register offsets
 *
 * The PHY is a custom Rockchip design with per-byte-lane sub-blocks.
 * Byte lane stride = 0x200.
 * ========================================================================= */

/* Global PHY control */
#define PHY_CON0                0x0000
#define  PHY_CON0_RESET         BIT(0)
#define  PHY_CON0_PHY_EN        BIT(1)
#define  PHY_CON0_DLL_EN        BIT(4)
#define  PHY_CON0_INIT_DONE     BIT(16)  /* read-only */

#define PHY_CON1                0x0004
#define PHY_CON2                0x0008
#define  PHY_CON2_TSEL_MASK     GENMASK(3, 0)   /* TX drive impedance select */
/*
 * TSEL drive-strength codes for this PHY (empirically validated against
 * Rockchip BSP sources and blob output):
 *   0x5 = ~40 Ω
 *   0x6 = ~34 Ω  (DDR4 standard)
 *   0x7 = ~30 Ω  ← LPDDR4/4X/5 target (phy_lp4x_dq_drv_when_odten_ohm=30)
 */
#define  PHY_CON2_TSEL_30OHM    0x7     /* LPDDR4/4X/5 drive = 30 Ω */
#define  PHY_CON2_TSEL_34OHM    0x6     /* DDR4 standard */
#define  PHY_CON2_TSEL_40OHM    0x5

#define  PHY_CON2_ODT_EN        BIT(8)
/*
 * ODT impedance select — separate from TSEL.
 * PHY_CON2_OTSEL codes:
 *   0x5 = ~40 Ω  ← LPDDR4/4X target (phy_lp4x_odt_ohm=40 in blob)
 *   0x6 = ~34 Ω
 */
#define  PHY_CON2_OTSEL_MASK    GENMASK(11, 9)
#define  PHY_CON2_OTSEL_40OHM   0x5     /* LPDDR4/4X PHY ODT = 40 Ω */
#define  PHY_CON2_OTSEL_34OHM   0x6

#define PHY_STATUS              0x000C
#define  PHY_STATUS_DLL_LOCK    BIT(0)
#define  PHY_STATUS_INIT_DONE   BIT(1)

/* DLL / delay line global */
#define PHY_DLL_CTRL0           0x0010
#define  DLL_CTRL0_BYPASS       BIT(0)
#define  DLL_CTRL0_START        BIT(1)
#define  DLL_CTRL0_INCREMENT    GENMASK(8, 4)

#define PHY_DLL_STATUS          0x0014
#define  DLL_STATUS_LOCK        BIT(0)
#define  DLL_STATUS_CODE_MASK   GENMASK(12, 4)

/* ZQ calibration */
#define PHY_ZQ_CTRL0            0x0020
#define  ZQ_CTRL0_START         BIT(0)
#define  ZQ_CTRL0_ZDEN          BIT(1)
#define  ZQ_CTRL0_ZDATA_MASK    GENMASK(27, 8)

#define PHY_ZQ_STATUS           0x0024
#define  ZQ_STATUS_DONE         BIT(0)
#define  ZQ_STATUS_PU_MASK      GENMASK(11, 6)
#define  ZQ_STATUS_PD_MASK      GENMASK(17, 12)

/* PLL / clock */
#define PHY_PLL_CTRL            0x0030
#define  PLL_CTRL_PD            BIT(0)
#define  PLL_CTRL_BYPASS        BIT(1)
#define  PLL_CTRL_DIV_MASK      GENMASK(9, 2)

#define PHY_PLL_STATUS          0x0034
#define  PLL_STATUS_LOCK        BIT(0)

/* Training engine control */
#define PHY_TRAIN_CTRL          0x0040
#define  TRAIN_CTRL_WRLVL_EN    BIT(0)
#define  TRAIN_CTRL_GATE_EN     BIT(1)
#define  TRAIN_CTRL_RDDSK_EN    BIT(2)
#define  TRAIN_CTRL_WRDSK_EN    BIT(3)
#define  TRAIN_CTRL_VREF_EN     BIT(4)
#define  TRAIN_CTRL_CA_EN       BIT(5)
#define  TRAIN_CTRL_START       BIT(16)
#define  TRAIN_CTRL_ABORT       BIT(17)

#define PHY_TRAIN_STATUS        0x0044
#define  TRAIN_STATUS_BUSY      BIT(0)
#define  TRAIN_STATUS_DONE      BIT(1)
#define  TRAIN_STATUS_FAIL_MASK GENMASK(7, 2)

#define PHY_TRAIN_RESULT        0x0048

/* CA training */
#define PHY_CA_DESKEW_BASE      0x0060
#define  PHY_CA_DESKEW(lane)    (PHY_CA_DESKEW_BASE + (lane) * 4)

#define PHY_CK_DELAY            0x0078
#define PHY_CS_DELAY            0x007C

/* ---- Per byte-lane registers (base + lane * 0x200) ---- */
#define PHY_LANE_STRIDE         0x0200

#define PHY_LANE_DQS_RX_DELAY(lane)    (0x1000 + (lane) * PHY_LANE_STRIDE + 0x00)
#define PHY_LANE_DQS_TX_DELAY(lane)    (0x1000 + (lane) * PHY_LANE_STRIDE + 0x04)
#define PHY_LANE_DQ_RX_DESKEW(lane)    (0x1000 + (lane) * PHY_LANE_STRIDE + 0x08)
#define PHY_LANE_DQ_TX_DESKEW(lane)    (0x1000 + (lane) * PHY_LANE_STRIDE + 0x0C)
#define PHY_LANE_GATE_DELAY(lane)      (0x1000 + (lane) * PHY_LANE_STRIDE + 0x10)
#define PHY_LANE_WRLVL_DELAY(lane)     (0x1000 + (lane) * PHY_LANE_STRIDE + 0x14)
#define PHY_LANE_VREF_DQ(lane)         (0x1000 + (lane) * PHY_LANE_STRIDE + 0x18)
#define PHY_LANE_ODT_DELAY(lane)       (0x1000 + (lane) * PHY_LANE_STRIDE + 0x1C)
#define PHY_LANE_STATUS(lane)          (0x1000 + (lane) * PHY_LANE_STRIDE + 0x20)
#define  LANE_STATUS_ALIGN      BIT(0)
#define  LANE_STATUS_RDLVL_OK   BIT(1)
#define  LANE_STATUS_WRLVL_OK   BIT(2)
#define  LANE_STATUS_VREF_OK    BIT(3)

#define PHY_LANE_BIT_RX_DESKEW_L(lane) (0x1000 + (lane) * PHY_LANE_STRIDE + 0x24)
#define PHY_LANE_BIT_RX_DESKEW_H(lane) (0x1000 + (lane) * PHY_LANE_STRIDE + 0x28)
#define PHY_LANE_BIT_TX_DESKEW_L(lane) (0x1000 + (lane) * PHY_LANE_STRIDE + 0x2C)
#define PHY_LANE_BIT_TX_DESKEW_H(lane) (0x1000 + (lane) * PHY_LANE_STRIDE + 0x30)

#define PHY_LANE_RD_EYE(lane)          (0x1000 + (lane) * PHY_LANE_STRIDE + 0x34)
#define  RD_EYE_WIDTH_MASK      GENMASK(7, 0)
#define  RD_EYE_CENTER_MASK     GENMASK(14, 8)
#define PHY_LANE_WR_EYE(lane)          (0x1000 + (lane) * PHY_LANE_STRIDE + 0x38)

/* =========================================================================
 * DDRGRF — DDR Global Register File
 *
 * Physical base: 0xF7800000
 *
 * Layout from rk3576_ddr_grf_reg (Rockchip U-Boot grf_rk3576.h):
 *   cha_con[20]      0x0000–0x004C   channel A config
 *   chb_con[20]      0x0100–0x014C   channel B config
 *   cha_status[12]   0x0200
 *   chb_status[12]   0x0300
 *   cha_phy_con[1]   0x0530
 *   chb_phy_con[1]   0x0534
 *   common_con[6]    0x0540          dual-channel interleave
 *   status[1]        0x0580
 *
 * Within each cha_con / chb_con block the layout from grf_rk3576.h line 311:
 *   [0]  0x000  channel/stride/enable config
 *   [1..4] 0x004..0x010  ddr_bank_mask[4]
 *   [5]  0x014  ddr_rank_mask[0]
 * ========================================================================= */

/* Channel A config registers */
#define DDRGRF_CHA_CON0         0x0000  /* channel enable, stride type */
#define DDRGRF_CHA_BANK_MASK0   0x0004  /* lp4_4x_bank_mask0 = 0x800  → bit 11 */
#define DDRGRF_CHA_BANK_MASK1   0x0008  /* lp4_4x_bank_mask1 = 0x1000 → bit 12 */
#define DDRGRF_CHA_BANK_MASK2   0x000C  /* lp4_4x_bank_mask2 = 0x2000 → bit 13 */
#define DDRGRF_CHA_BANK_MASK3   0x0010  /* lp4_4x_bank_mask3 = 0x0    (unused) */
#define DDRGRF_CHA_RANK_MASK0   0x0014  /* lp4_4x_rank_mask0 = 0x400000 → bit 22 */

/* Channel B config registers (same layout, +0x100) */
#define DDRGRF_CHB_CON0         0x0100
#define DDRGRF_CHB_BANK_MASK0   0x0104
#define DDRGRF_CHB_BANK_MASK1   0x0108
#define DDRGRF_CHB_BANK_MASK2   0x010C
#define DDRGRF_CHB_BANK_MASK3   0x0110
#define DDRGRF_CHB_RANK_MASK0   0x0114

/* Common (dual-channel) config — offset 0x0540 */
#define DDRGRF_COMMON_CON0      0x0540
/*
 * GRF_CON0_SPLIT_EN  — enables 64-byte cache-line interleave across channels.
 * GRF_CON0_SPLIT_MASK[2:1] — interleave granularity:
 *   0x0 = 256 B, 0x1 = 64 B (one cache line), 0x2 = 128 B
 * Blob uses stride_type=3 which corresponds to 64 B granule (0x1).
 */
#define  GRF_CON0_SPLIT_EN      BIT(0)
#define  GRF_CON0_SPLIT_MASK    GENMASK(2, 1)
#define  GRF_CON0_SPLIT_64B     0x1     /* 64-byte interleave */

/* Status */
#define DDRGRF_STATUS0          0x0580
#define  GRF_STAT_INIT_DONE_CH0     BIT(0)
#define  GRF_STAT_INIT_DONE_CH1     BIT(1)
#define  GRF_STAT_TRAIN_DONE_CH0    BIT(2)
#define  GRF_STAT_TRAIN_DONE_CH1    BIT(3)
#define  GRF_STAT_BIST_OK_CH0       BIT(4)
#define  GRF_STAT_BIST_OK_CH1       BIT(5)

/* BIST engine */
#define DDRGRF_BIST_CTRL        0x0020
#define  BIST_CTRL_EN           BIT(0)
#define  BIST_CTRL_FULL         BIT(1)
#define  BIST_CTRL_PATTERN_MASK GENMASK(5, 2)

#define DDRGRF_BIST_ADDR        0x0024
#define DDRGRF_BIST_LEN         0x0028
#define DDRGRF_BIST_STATUS      0x002C
#define  BIST_STATUS_BUSY       BIT(0)
#define  BIST_STATUS_FAIL       BIT(1)
#define  BIST_STATUS_DONE       BIT(2)

/* =========================================================================
 * CRU — Clock/Reset Unit, DDR-relevant entries
 *
 * DPLL is PLL index 2 in the rk3576_pll array.
 * Each rk3576_pll entry is 8 words (0x20 bytes).
 * DPLL base = CRU_BASE + 2 * 0x20 = CRU_BASE + 0x40.
 *
 * Softreset: RK3576_SOFTRST_CON(x) = x*4 + 0xa00  (from cru_rk3576.h macro).
 * DDR resets are in softrst_con[27] per RK3576 TRM — verify against your
 * silicon documentation.  The bit positions below match Rockchip BSP usage.
 * ========================================================================= */

#define CRU_DPLL_CON0           0x0040   /* DPLL: FBDIV */
#define  DPLL_FBDIV_MASK        GENMASK(11, 0)
#define CRU_DPLL_CON1           0x0044   /* DPLL: POSTDIV1/2, REFDIV */
#define  DPLL_POSTDIV1_MASK     GENMASK(14, 12)
#define  DPLL_POSTDIV2_MASK     GENMASK(18, 16)
#define  DPLL_REFDIV_MASK       GENMASK(5, 0)
#define CRU_DPLL_CON2           0x0048   /* DPLL: FRAC */
#define CRU_DPLL_CON3           0x004C   /* DPLL: control */
#define  DPLL_DSMPD             BIT(24)  /* 1 = integer-N mode */
#define  DPLL_PD                BIT(25)  /* 1 = power down */
#define CRU_DPLL_CON4           0x0050   /* DPLL: lock status in bit 31 */

#define CRU_CLKSEL_DDR0         0x0180
#define CRU_CLKSEL_DDR1         0x0184
#define  CLKSEL_DDR_DIV_MASK    GENMASK(4, 0)
#define  CLKSEL_DDR_SRC_MASK    GENMASK(7, 5)
#define  CLKSEL_DDR_SRC_DPLL    0
#define  CLKSEL_DDR_SRC_GPLL    1

/*
 * Softreset offset: RK3576_SOFTRST_CON(x) = x*4 + 0xa00
 * softrst_con[27] controls DDR subsystem resets per RK3576 TRM.
 * Bit assignments in softrst_con[27]:
 *   [0] sresetn_ddrmc_ch0   — DDRMC channel 0
 *   [1] sresetn_ddrmc_ch1   — DDRMC channel 1
 *   [2] presetn_ddrphy_ch0  — DDRPHY channel 0
 *   [3] presetn_ddrphy_ch1  — DDRPHY channel 1
 */
#define CRU_SOFTRST_CON27       0x0a6c   /* 0xa00 + 27*4 */
#define  SRST_DDRMC0_N          BIT(0)
#define  SRST_DDRMC1_N          BIT(1)
#define  SRST_DDRPHY0_N         BIT(2)
#define  SRST_DDRPHY1_N         BIT(3)

/* =========================================================================
 * MSCH — Multi-layer System Cache Hub (per-channel bandwidth / QoS)
 * ========================================================================= */

#define MSCH_DDRCONF            0x0008
#define MSCH_DDRTIMING          0x000C
#define  MSCH_DDRTIMING_ACT_TO_ACT_MASK   GENMASK(5, 0)
#define  MSCH_DDRTIMING_RD_TO_MISS_MASK   GENMASK(11, 6)
#define  MSCH_DDRTIMING_WR_TO_MISS_MASK   GENMASK(17, 12)
#define  MSCH_DDRTIMING_BURST_MASK        GENMASK(20, 18)

#define MSCH_DDRMODE            0x0010
#define MSCH_READLATENCY        0x0014
#define MSCH_ACTIVATE           0x0038
#define MSCH_DEVTODEV           0x003C

/* =========================================================================
 * LPDDR4/5 Mode Register addresses (JEDEC JESD209-4 / JESD209-5)
 * ========================================================================= */

#define LPDDR4_MR1              1
#define LPDDR4_MR2              2
#define LPDDR4_MR3              3
#define LPDDR4_MR4              4
#define LPDDR4_MR11             11
#define LPDDR4_MR12             12
#define LPDDR4_MR13             13
#define LPDDR4_MR14             14
#define LPDDR4_MR22             22

/*
 * MR11 DQ ODT encoding (JEDEC JESD209-4C Table 21):
 *   OP[2:0]  RZQ/N   Ohms
 *   000      —       Disabled
 *   001      RZQ/1   240 Ω
 *   010      RZQ/2   120 Ω
 *   011      RZQ/3    80 Ω
 *   100      RZQ/4    60 Ω   ← original code had this (wrong for LPDDR4X)
 *   101      RZQ/5    48 Ω
 *   110      RZQ/6    40 Ω   ← blob: lp4x_odt_ohm=40, lp4_odt_ohm=40
 *
 * MR22 SOC-ODT (CA ODT) encoding — same table, bits [2:0]:
 *   010      RZQ/2   120 Ω   ← blob: lp4x_ca_odt_ohm=120
 */
#define LPDDR4_MR11_ODT_DIS     0x00
#define LPDDR4_MR11_ODT_240OHM  0x01
#define LPDDR4_MR11_ODT_120OHM  0x02
#define LPDDR4_MR11_ODT_80OHM   0x03
#define LPDDR4_MR11_ODT_60OHM   0x04
#define LPDDR4_MR11_ODT_48OHM   0x05
#define LPDDR4_MR11_ODT_40OHM   0x06   /* LPDDR4/4X target */

#define LPDDR4_MR22_SOCOCT_DIS     0x00
#define LPDDR4_MR22_SOCOCT_120OHM  0x02   /* lp4x_ca_odt_ohm=120 */
#define LPDDR4_MR22_SOCOCT_80OHM   0x03

/*
 * MR14 initial VREF values.
 * LPDDR4X range 1: VrefDQ = 10.0% + MR14[5:0] * 0.4%
 *   code 0x20 = 32 → 10 + 32*0.4 = 22.8%
 *   This is the safe initial value before the VREF sweep.
 * Range 2 (MR14[6]=1): VrefDQ = 22.0% + MR14[5:0] * 0.4%
 */
#define LPDDR4X_MR14_VREF_INIT  0x20   /* ~22.8%, range 1, pre-sweep start */
#define LPDDR4_MR14_VREF_INIT   0x31   /* LPDDR4 default ~27.2% */

#define LPDDR5_MR1              1
#define LPDDR5_MR2              2
#define LPDDR5_MR3              3
#define LPDDR5_MR10             10
#define LPDDR5_MR11             11
#define LPDDR5_MR12             12
#define LPDDR5_MR13             13
#define LPDDR5_MR14             14
#define LPDDR5_MR15             15
#define LPDDR5_MR17             17
#define LPDDR5_MR18             18
#define LPDDR5_MR19             19
#define LPDDR5_MR20             20
#define LPDDR5_MR28             28

#define DDR4_MR0                0
#define DDR4_MR1                1
#define DDR4_MR2                2
#define DDR4_MR3                3
#define DDR4_MR4                4
#define DDR4_MR5                5
#define DDR4_MR6                6

/* =========================================================================
 * Timeout constants
 * ========================================================================= */
#define DDR_INIT_TIMEOUT_US     100000
#define DDR_TRAIN_TIMEOUT_US    500000
#define DDR_DLL_LOCK_TIMEOUT_US  10000
#define DDR_ZQ_TIMEOUT_US        50000
#define DDR_BIST_TIMEOUT_US     200000

/* =========================================================================
 * Macro: wait-for-bit with timeout
 * ========================================================================= */
#define DDR_WAIT_FLAG(addr, mask, val, timeout_us, ret) do {    \
    uint32_t _tout = (timeout_us);                              \
    while ((readl(addr) & (mask)) != (val)) {                   \
        if (--_tout == 0) { (ret) = DDR_TRAIN_ERR_TIMEOUT; break; } \
        udelay(1);                                              \
    }                                                           \
} while (0)

#endif /* RK3576_DDR_REGS_H */
