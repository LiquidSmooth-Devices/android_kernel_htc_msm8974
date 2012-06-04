/*
 * Copyright 2008-2009 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#ifndef __ARCH_ARM_MACH_MX51_CRM_REGS_H__
#define __ARCH_ARM_MACH_MX51_CRM_REGS_H__

#define MX51_CCM_BASE		MX51_IO_ADDRESS(MX51_CCM_BASE_ADDR)
#define MX51_DPLL1_BASE		MX51_IO_ADDRESS(MX51_PLL1_BASE_ADDR)
#define MX51_DPLL2_BASE		MX51_IO_ADDRESS(MX51_PLL2_BASE_ADDR)
#define MX51_DPLL3_BASE		MX51_IO_ADDRESS(MX51_PLL3_BASE_ADDR)
#define MX51_CORTEXA8_BASE	MX51_IO_ADDRESS(MX51_ARM_BASE_ADDR)
#define MX51_GPC_BASE		MX51_IO_ADDRESS(MX51_GPC_BASE_ADDR)

#define MX53_CCM_BASE		MX53_IO_ADDRESS(MX53_CCM_BASE_ADDR)
#define MX53_DPLL1_BASE		MX53_IO_ADDRESS(MX53_PLL1_BASE_ADDR)
#define MX53_DPLL2_BASE		MX53_IO_ADDRESS(MX53_PLL2_BASE_ADDR)
#define MX53_DPLL3_BASE		MX53_IO_ADDRESS(MX53_PLL3_BASE_ADDR)
#define MX53_DPLL4_BASE		MX53_IO_ADDRESS(MX53_PLL4_BASE_ADDR)

#define MXC_PLL_DP_CTL			0x00
#define MXC_PLL_DP_CONFIG		0x04
#define MXC_PLL_DP_OP			0x08
#define MXC_PLL_DP_MFD			0x0C
#define MXC_PLL_DP_MFN			0x10
#define MXC_PLL_DP_MFNMINUS		0x14
#define MXC_PLL_DP_MFNPLUS		0x18
#define MXC_PLL_DP_HFS_OP		0x1C
#define MXC_PLL_DP_HFS_MFD		0x20
#define MXC_PLL_DP_HFS_MFN		0x24
#define MXC_PLL_DP_MFN_TOGC		0x28
#define MXC_PLL_DP_DESTAT		0x2c

#define MXC_PLL_DP_CTL_MUL_CTRL		0x2000
#define MXC_PLL_DP_CTL_DPDCK0_2_EN	0x1000
#define MXC_PLL_DP_CTL_DPDCK0_2_OFFSET	12
#define MXC_PLL_DP_CTL_ADE		0x800
#define MXC_PLL_DP_CTL_REF_CLK_DIV	0x400
#define MXC_PLL_DP_CTL_REF_CLK_SEL_MASK	(3 << 8)
#define MXC_PLL_DP_CTL_REF_CLK_SEL_OFFSET	8
#define MXC_PLL_DP_CTL_HFSM		0x80
#define MXC_PLL_DP_CTL_PRE		0x40
#define MXC_PLL_DP_CTL_UPEN		0x20
#define MXC_PLL_DP_CTL_RST		0x10
#define MXC_PLL_DP_CTL_RCP		0x8
#define MXC_PLL_DP_CTL_PLM		0x4
#define MXC_PLL_DP_CTL_BRM0		0x2
#define MXC_PLL_DP_CTL_LRF		0x1

#define MXC_PLL_DP_CONFIG_BIST		0x8
#define MXC_PLL_DP_CONFIG_SJC_CE	0x4
#define MXC_PLL_DP_CONFIG_AREN		0x2
#define MXC_PLL_DP_CONFIG_LDREQ		0x1

#define MXC_PLL_DP_OP_MFI_OFFSET	4
#define MXC_PLL_DP_OP_MFI_MASK		(0xF << 4)
#define MXC_PLL_DP_OP_PDF_OFFSET	0
#define MXC_PLL_DP_OP_PDF_MASK		0xF

#define MXC_PLL_DP_MFD_OFFSET		0
#define MXC_PLL_DP_MFD_MASK		0x07FFFFFF

#define MXC_PLL_DP_MFN_OFFSET		0x0
#define MXC_PLL_DP_MFN_MASK		0x07FFFFFF

#define MXC_PLL_DP_MFN_TOGC_TOG_DIS	(1 << 17)
#define MXC_PLL_DP_MFN_TOGC_TOG_EN	(1 << 16)
#define MXC_PLL_DP_MFN_TOGC_CNT_OFFSET	0x0
#define MXC_PLL_DP_MFN_TOGC_CNT_MASK	0xFFFF

#define MXC_PLL_DP_DESTAT_TOG_SEL	(1 << 31)
#define MXC_PLL_DP_DESTAT_MFN		0x07FFFFFF

#define MXC_CCM_CCR		(MX51_CCM_BASE + 0x00)
#define MXC_CCM_CCDR		(MX51_CCM_BASE + 0x04)
#define MXC_CCM_CSR		(MX51_CCM_BASE + 0x08)
#define MXC_CCM_CCSR		(MX51_CCM_BASE + 0x0C)
#define MXC_CCM_CACRR		(MX51_CCM_BASE + 0x10)
#define MXC_CCM_CBCDR		(MX51_CCM_BASE + 0x14)
#define MXC_CCM_CBCMR		(MX51_CCM_BASE + 0x18)
#define MXC_CCM_CSCMR1		(MX51_CCM_BASE + 0x1C)
#define MXC_CCM_CSCMR2		(MX51_CCM_BASE + 0x20)
#define MXC_CCM_CSCDR1		(MX51_CCM_BASE + 0x24)
#define MXC_CCM_CS1CDR		(MX51_CCM_BASE + 0x28)
#define MXC_CCM_CS2CDR		(MX51_CCM_BASE + 0x2C)
#define MXC_CCM_CDCDR		(MX51_CCM_BASE + 0x30)
#define MXC_CCM_CHSCDR		(MX51_CCM_BASE + 0x34)
#define MXC_CCM_CSCDR2		(MX51_CCM_BASE + 0x38)
#define MXC_CCM_CSCDR3		(MX51_CCM_BASE + 0x3C)
#define MXC_CCM_CSCDR4		(MX51_CCM_BASE + 0x40)
#define MXC_CCM_CWDR		(MX51_CCM_BASE + 0x44)
#define MXC_CCM_CDHIPR		(MX51_CCM_BASE + 0x48)
#define MXC_CCM_CDCR		(MX51_CCM_BASE + 0x4C)
#define MXC_CCM_CTOR		(MX51_CCM_BASE + 0x50)
#define MXC_CCM_CLPCR		(MX51_CCM_BASE + 0x54)
#define MXC_CCM_CISR		(MX51_CCM_BASE + 0x58)
#define MXC_CCM_CIMR		(MX51_CCM_BASE + 0x5C)
#define MXC_CCM_CCOSR		(MX51_CCM_BASE + 0x60)
#define MXC_CCM_CGPR		(MX51_CCM_BASE + 0x64)
#define MXC_CCM_CCGR0		(MX51_CCM_BASE + 0x68)
#define MXC_CCM_CCGR1		(MX51_CCM_BASE + 0x6C)
#define MXC_CCM_CCGR2		(MX51_CCM_BASE + 0x70)
#define MXC_CCM_CCGR3		(MX51_CCM_BASE + 0x74)
#define MXC_CCM_CCGR4		(MX51_CCM_BASE + 0x78)
#define MXC_CCM_CCGR5		(MX51_CCM_BASE + 0x7C)
#define MXC_CCM_CCGR6		(MX51_CCM_BASE + 0x80)
#define MXC_CCM_CCGR7		(MX51_CCM_BASE + 0x84)

#define MXC_CCM_CMEOR		(MX51_CCM_BASE + 0x84)

#define MXC_CCM_CCR_COSC_EN		(1 << 12)
#define MXC_CCM_CCR_FPM_MULT_MASK	(1 << 11)
#define MXC_CCM_CCR_CAMP2_EN		(1 << 10)
#define MXC_CCM_CCR_CAMP1_EN		(1 << 9)
#define MXC_CCM_CCR_FPM_EN		(1 << 8)
#define MXC_CCM_CCR_OSCNT_OFFSET	(0)
#define MXC_CCM_CCR_OSCNT_MASK	(0xFF)

#define MXC_CCM_CCDR_HSC_HS_MASK	(0x1 << 18)
#define MXC_CCM_CCDR_IPU_HS_MASK	(0x1 << 17)
#define MXC_CCM_CCDR_EMI_HS_MASK	(0x1 << 16)

#define MXC_CCM_CSR_COSR_READY	(1 << 5)
#define MXC_CCM_CSR_LVS_VALUE	(1 << 4)
#define MXC_CCM_CSR_CAMP2_READY	(1 << 3)
#define MXC_CCM_CSR_CAMP1_READY	(1 << 2)
#define MXC_CCM_CSR_FPM_READY	(1 << 1)
#define MXC_CCM_CSR_REF_EN_B	(1 << 0)

#define MXC_CCM_CCSR_LP_APM_SEL		(0x1 << 9)
#define MXC_CCM_CCSR_STEP_SEL_OFFSET	(7)
#define MXC_CCM_CCSR_STEP_SEL_MASK	(0x3 << 7)
#define MXC_CCM_CCSR_STEP_SEL_LP_APM	   0
#define MXC_CCM_CCSR_STEP_SEL_PLL1_BYPASS  1 
#define MXC_CCM_CCSR_STEP_SEL_PLL2_DIVIDED 2
#define MXC_CCM_CCSR_STEP_SEL_PLL3_DIVIDED 3
#define MXC_CCM_CCSR_PLL2_PODF_OFFSET	(5)
#define MXC_CCM_CCSR_PLL2_PODF_MASK	(0x3 << 5)
#define MXC_CCM_CCSR_PLL3_PODF_OFFSET	(3)
#define MXC_CCM_CCSR_PLL3_PODF_MASK	(0x3 << 3)
#define MXC_CCM_CCSR_PLL1_SW_CLK_SEL	(1 << 2) 
#define MXC_CCM_CCSR_PLL2_SW_CLK_SEL	(1 << 1)
#define MXC_CCM_CCSR_PLL3_SW_CLK_SEL	(1 << 0)

#define MXC_CCM_CACRR_ARM_PODF_OFFSET	(0)
#define MXC_CCM_CACRR_ARM_PODF_MASK	(0x7)

#define MXC_CCM_CBCDR_EMI_CLK_SEL		(0x1 << 26)
#define MXC_CCM_CBCDR_PERIPH_CLK_SEL		(0x1 << 25)
#define MXC_CCM_CBCDR_DDR_HF_SEL_OFFSET		(30)
#define MXC_CCM_CBCDR_DDR_HF_SEL		(0x1 << 30)
#define MXC_CCM_CBCDR_DDR_PODF_OFFSET		(27)
#define MXC_CCM_CBCDR_DDR_PODF_MASK		(0x7 << 27)
#define MXC_CCM_CBCDR_EMI_PODF_OFFSET		(22)
#define MXC_CCM_CBCDR_EMI_PODF_MASK		(0x7 << 22)
#define MXC_CCM_CBCDR_AXI_B_PODF_OFFSET		(19)
#define MXC_CCM_CBCDR_AXI_B_PODF_MASK		(0x7 << 19)
#define MXC_CCM_CBCDR_AXI_A_PODF_OFFSET		(16)
#define MXC_CCM_CBCDR_AXI_A_PODF_MASK		(0x7 << 16)
#define MXC_CCM_CBCDR_NFC_PODF_OFFSET		(13)
#define MXC_CCM_CBCDR_NFC_PODF_MASK		(0x7 << 13)
#define MXC_CCM_CBCDR_AHB_PODF_OFFSET		(10)
#define MXC_CCM_CBCDR_AHB_PODF_MASK		(0x7 << 10)
#define MXC_CCM_CBCDR_IPG_PODF_OFFSET		(8)
#define MXC_CCM_CBCDR_IPG_PODF_MASK		(0x3 << 8)
#define MXC_CCM_CBCDR_PERCLK_PRED1_OFFSET	(6)
#define MXC_CCM_CBCDR_PERCLK_PRED1_MASK		(0x3 << 6)
#define MXC_CCM_CBCDR_PERCLK_PRED2_OFFSET	(3)
#define MXC_CCM_CBCDR_PERCLK_PRED2_MASK		(0x7 << 3)
#define MXC_CCM_CBCDR_PERCLK_PODF_OFFSET	(0)
#define MXC_CCM_CBCDR_PERCLK_PODF_MASK		(0x7)

#define MXC_CCM_CBCMR_VPU_AXI_CLK_SEL_OFFSET	(14)
#define MXC_CCM_CBCMR_VPU_AXI_CLK_SEL_MASK	(0x3 << 14)
#define MXC_CCM_CBCMR_PERIPH_CLK_SEL_OFFSET	(12)
#define MXC_CCM_CBCMR_PERIPH_CLK_SEL_MASK	(0x3 << 12)
#define MXC_CCM_CBCMR_DDR_CLK_SEL_OFFSET	(10)
#define MXC_CCM_CBCMR_DDR_CLK_SEL_MASK		(0x3 << 10)
#define MXC_CCM_CBCMR_ARM_AXI_CLK_SEL_OFFSET	(8)
#define MXC_CCM_CBCMR_ARM_AXI_CLK_SEL_MASK	(0x3 << 8)
#define MXC_CCM_CBCMR_IPU_HSP_CLK_SEL_OFFSET	(6)
#define MXC_CCM_CBCMR_IPU_HSP_CLK_SEL_MASK	(0x3 << 6)
#define MXC_CCM_CBCMR_GPU_CLK_SEL_OFFSET	(4)
#define MXC_CCM_CBCMR_GPU_CLK_SEL_MASK		(0x3 << 4)
#define MXC_CCM_CBCMR_GPU2D_CLK_SEL_OFFSET	(14)
#define MXC_CCM_CBCMR_GPU2D_CLK_SEL_MASK	(0x3 << 14)
#define MXC_CCM_CBCMR_PERCLK_LP_APM_CLK_SEL	(0x1 << 1)
#define MXC_CCM_CBCMR_PERCLK_IPG_CLK_SEL	(0x1 << 0)

#define MXC_CCM_CSCMR1_SSI_EXT2_CLK_SEL_OFFSET		(30)
#define MXC_CCM_CSCMR1_SSI_EXT2_CLK_SEL_MASK		(0x3 << 30)
#define MXC_CCM_CSCMR1_SSI_EXT1_CLK_SEL_OFFSET		(28)
#define MXC_CCM_CSCMR1_SSI_EXT1_CLK_SEL_MASK		(0x3 << 28)
#define MXC_CCM_CSCMR1_USB_PHY_CLK_SEL_OFFSET		(26)
#define MXC_CCM_CSCMR1_USB_PHY_CLK_SEL			(0x1 << 26)
#define MXC_CCM_CSCMR1_UART_CLK_SEL_OFFSET		(24)
#define MXC_CCM_CSCMR1_UART_CLK_SEL_MASK		(0x3 << 24)
#define MXC_CCM_CSCMR1_USBOH3_CLK_SEL_OFFSET		(22)
#define MXC_CCM_CSCMR1_USBOH3_CLK_SEL_MASK		(0x3 << 22)
#define MXC_CCM_CSCMR1_ESDHC1_MSHC1_CLK_SEL_OFFSET	(20)
#define MXC_CCM_CSCMR1_ESDHC1_MSHC1_CLK_SEL_MASK	(0x3 << 20)
#define MXC_CCM_CSCMR1_ESDHC3_CLK_SEL			(0x1 << 19)
#define MXC_CCM_CSCMR1_ESDHC2_MSHC2_MX53_CLK_SEL	(0x1 << 19)
#define MXC_CCM_CSCMR1_ESDHC4_CLK_SEL			(0x1 << 18)
#define MXC_CCM_CSCMR1_ESDHC2_MSHC2_CLK_SEL_OFFSET	(16)
#define MXC_CCM_CSCMR1_ESDHC2_MSHC2_CLK_SEL_MASK	(0x3 << 16)
#define MXC_CCM_CSCMR1_ESDHC3_MX53_CLK_SEL_OFFSET	(16)
#define MXC_CCM_CSCMR1_ESDHC3_MX53_CLK_SEL_MASK		(0x3 << 16)
#define MXC_CCM_CSCMR1_SSI1_CLK_SEL_OFFSET		(14)
#define MXC_CCM_CSCMR1_SSI1_CLK_SEL_MASK		(0x3 << 14)
#define MXC_CCM_CSCMR1_SSI2_CLK_SEL_OFFSET		(12)
#define MXC_CCM_CSCMR1_SSI2_CLK_SEL_MASK		(0x3 << 12)
#define MXC_CCM_CSCMR1_SSI3_CLK_SEL			(0x1 << 11)
#define MXC_CCM_CSCMR1_VPU_RCLK_SEL			(0x1 << 10)
#define MXC_CCM_CSCMR1_SSI_APM_CLK_SEL_OFFSET		(8)
#define MXC_CCM_CSCMR1_SSI_APM_CLK_SEL_MASK		(0x3 << 8)
#define MXC_CCM_CSCMR1_TVE_CLK_SEL			(0x1 << 7)
#define MXC_CCM_CSCMR1_TVE_EXT_CLK_SEL			(0x1 << 6)
#define MXC_CCM_CSCMR1_CSPI_CLK_SEL_OFFSET		(4)
#define MXC_CCM_CSCMR1_CSPI_CLK_SEL_MASK		(0x3 << 4)
#define MXC_CCM_CSCMR1_SPDIF_CLK_SEL_OFFSET		(2)
#define MXC_CCM_CSCMR1_SPDIF_CLK_SEL_MASK		(0x3 << 2)
#define MXC_CCM_CSCMR1_SSI_EXT2_COM_CLK_SEL		(0x1 << 1)
#define MXC_CCM_CSCMR1_SSI_EXT1_COM_CLK_SEL		(0x1)

#define MXC_CCM_CSCMR2_DI_CLK_SEL_OFFSET(n)		(26+n*3)
#define MXC_CCM_CSCMR2_DI_CLK_SEL_MASK(n)		(0x7 << (26+n*3))
#define MXC_CCM_CSCMR2_CSI_MCLK2_CLK_SEL_OFFSET		(24)
#define MXC_CCM_CSCMR2_CSI_MCLK2_CLK_SEL_MASK		(0x3 << 24)
#define MXC_CCM_CSCMR2_CSI_MCLK1_CLK_SEL_OFFSET		(22)
#define MXC_CCM_CSCMR2_CSI_MCLK1_CLK_SEL_MASK		(0x3 << 22)
#define MXC_CCM_CSCMR2_ESC_CLK_SEL_OFFSET		(20)
#define MXC_CCM_CSCMR2_ESC_CLK_SEL_MASK			(0x3 << 20)
#define MXC_CCM_CSCMR2_HSC2_CLK_SEL_OFFSET		(18)
#define MXC_CCM_CSCMR2_HSC2_CLK_SEL_MASK		(0x3 << 18)
#define MXC_CCM_CSCMR2_HSC1_CLK_SEL_OFFSET		(16)
#define MXC_CCM_CSCMR2_HSC1_CLK_SEL_MASK		(0x3 << 16)
#define MXC_CCM_CSCMR2_HSI2C_CLK_SEL_OFFSET		(14)
#define MXC_CCM_CSCMR2_HSI2C_CLK_SEL_MASK		(0x3 << 14)
#define MXC_CCM_CSCMR2_FIRI_CLK_SEL_OFFSET		(12)
#define MXC_CCM_CSCMR2_FIRI_CLK_SEL_MASK		(0x3 << 12)
#define MXC_CCM_CSCMR2_SIM_CLK_SEL_OFFSET		(10)
#define MXC_CCM_CSCMR2_SIM_CLK_SEL_MASK			(0x3 << 10)
#define MXC_CCM_CSCMR2_SLIMBUS_COM			(0x1 << 9)
#define MXC_CCM_CSCMR2_SLIMBUS_CLK_SEL_OFFSET		(6)
#define MXC_CCM_CSCMR2_SLIMBUS_CLK_SEL_MASK		(0x7 << 6)
#define MXC_CCM_CSCMR2_SPDIF1_COM			(1 << 5)
#define MXC_CCM_CSCMR2_SPDIF0_COM			(1 << 4)
#define MXC_CCM_CSCMR2_SPDIF1_CLK_SEL_OFFSET		(2)
#define MXC_CCM_CSCMR2_SPDIF1_CLK_SEL_MASK		(0x3 << 2)
#define MXC_CCM_CSCMR2_SPDIF0_CLK_SEL_OFFSET		(0)
#define MXC_CCM_CSCMR2_SPDIF0_CLK_SEL_MASK		(0x3)

#define MXC_CCM_CSCDR1_ESDHC2_MSHC2_CLK_PRED_OFFSET	(22)
#define MXC_CCM_CSCDR1_ESDHC2_MSHC2_CLK_PRED_MASK	(0x7 << 22)
#define MXC_CCM_CSCDR1_ESDHC2_MSHC2_CLK_PODF_OFFSET	(19)
#define MXC_CCM_CSCDR1_ESDHC2_MSHC2_CLK_PODF_MASK	(0x7 << 19)
#define MXC_CCM_CSCDR1_ESDHC3_MX53_CLK_PRED_OFFSET	(22)
#define MXC_CCM_CSCDR1_ESDHC3_MX53_CLK_PRED_MASK	(0x7 << 22)
#define MXC_CCM_CSCDR1_ESDHC3_MX53_CLK_PODF_OFFSET	(19)
#define MXC_CCM_CSCDR1_ESDHC3_MX53_CLK_PODF_MASK	(0x7 << 19)
#define MXC_CCM_CSCDR1_ESDHC1_MSHC1_CLK_PRED_OFFSET	(16)
#define MXC_CCM_CSCDR1_ESDHC1_MSHC1_CLK_PRED_MASK	(0x7 << 16)
#define MXC_CCM_CSCDR1_PGC_CLK_PODF_OFFSET		(14)
#define MXC_CCM_CSCDR1_PGC_CLK_PODF_MASK		(0x3 << 14)
#define MXC_CCM_CSCDR1_ESDHC1_MSHC1_CLK_PODF_OFFSET	(11)
#define MXC_CCM_CSCDR1_ESDHC1_MSHC1_CLK_PODF_MASK	(0x7 << 11)
#define MXC_CCM_CSCDR1_USBOH3_CLK_PRED_OFFSET		(8)
#define MXC_CCM_CSCDR1_USBOH3_CLK_PRED_MASK		(0x7 << 8)
#define MXC_CCM_CSCDR1_USBOH3_CLK_PODF_OFFSET		(6)
#define MXC_CCM_CSCDR1_USBOH3_CLK_PODF_MASK		(0x3 << 6)
#define MXC_CCM_CSCDR1_UART_CLK_PRED_OFFSET		(3)
#define MXC_CCM_CSCDR1_UART_CLK_PRED_MASK		(0x7 << 3)
#define MXC_CCM_CSCDR1_UART_CLK_PODF_OFFSET		(0)
#define MXC_CCM_CSCDR1_UART_CLK_PODF_MASK		(0x7)

#define MXC_CCM_CS1CDR_SSI_EXT1_CLK_PRED_OFFSET		(22)
#define MXC_CCM_CS1CDR_SSI_EXT1_CLK_PRED_MASK		(0x7 << 22)
#define MXC_CCM_CS1CDR_SSI_EXT1_CLK_PODF_OFFSET		(16)
#define MXC_CCM_CS1CDR_SSI_EXT1_CLK_PODF_MASK		(0x3F << 16)
#define MXC_CCM_CS1CDR_SSI1_CLK_PRED_OFFSET		(6)
#define MXC_CCM_CS1CDR_SSI1_CLK_PRED_MASK		(0x7 << 6)
#define MXC_CCM_CS1CDR_SSI1_CLK_PODF_OFFSET		(0)
#define MXC_CCM_CS1CDR_SSI1_CLK_PODF_MASK		(0x3F)

#define MXC_CCM_CS2CDR_SSI_EXT2_CLK_PRED_OFFSET		(22)
#define MXC_CCM_CS2CDR_SSI_EXT2_CLK_PRED_MASK		(0x7 << 22)
#define MXC_CCM_CS2CDR_SSI_EXT2_CLK_PODF_OFFSET		(16)
#define MXC_CCM_CS2CDR_SSI_EXT2_CLK_PODF_MASK		(0x3F << 16)
#define MXC_CCM_CS2CDR_SSI2_CLK_PRED_OFFSET		(6)
#define MXC_CCM_CS2CDR_SSI2_CLK_PRED_MASK		(0x7 << 6)
#define MXC_CCM_CS2CDR_SSI2_CLK_PODF_OFFSET		(0)
#define MXC_CCM_CS2CDR_SSI2_CLK_PODF_MASK		(0x3F)

#define MXC_CCM_CDCDR_TVE_CLK_PRED_OFFSET		(28)
#define MXC_CCM_CDCDR_TVE_CLK_PRED_MASK			(0x7 << 28)
#define MXC_CCM_CDCDR_SPDIF0_CLK_PRED_OFFSET		(25)
#define MXC_CCM_CDCDR_SPDIF0_CLK_PRED_MASK		(0x7 << 25)
#define MXC_CCM_CDCDR_SPDIF0_CLK_PODF_OFFSET		(19)
#define MXC_CCM_CDCDR_SPDIF0_CLK_PODF_MASK		(0x3F << 19)
#define MXC_CCM_CDCDR_SPDIF1_CLK_PRED_OFFSET		(16)
#define MXC_CCM_CDCDR_SPDIF1_CLK_PRED_MASK		(0x7 << 16)
#define MXC_CCM_CDCDR_SPDIF1_CLK_PODF_OFFSET		(9)
#define MXC_CCM_CDCDR_SPDIF1_CLK_PODF_MASK		(0x3F << 9)
#define MXC_CCM_CDCDR_DI_CLK_PRED_OFFSET		(6)
#define MXC_CCM_CDCDR_DI_CLK_PRED_MASK			(0x7 << 6)
#define MXC_CCM_CDCDR_USB_PHY_PRED_OFFSET		(3)
#define MXC_CCM_CDCDR_USB_PHY_PRED_MASK			(0x7 << 3)
#define MXC_CCM_CDCDR_USB_PHY_PODF_OFFSET		(0)
#define MXC_CCM_CDCDR_USB_PHY_PODF_MASK			(0x7)

#define MXC_CCM_CHSCCDR_ESC_CLK_PRED_OFFSET		(12)
#define MXC_CCM_CHSCCDR_ESC_CLK_PRED_MASK		(0x7 << 12)
#define MXC_CCM_CHSCCDR_ESC_CLK_PODF_OFFSET		(6)
#define MXC_CCM_CHSCCDR_ESC_CLK_PODF_MASK		(0x3F << 6)
#define MXC_CCM_CHSCCDR_HSC2_CLK_PODF_OFFSET		(3)
#define MXC_CCM_CHSCCDR_HSC2_CLK_PODF_MASK		(0x7 << 3)
#define MXC_CCM_CHSCCDR_HSC1_CLK_PODF_OFFSET		(0)
#define MXC_CCM_CHSCCDR_HSC1_CLK_PODF_MASK		(0x7)

#define MXC_CCM_CSCDR2_CSPI_CLK_PRED_OFFSET		(25)
#define MXC_CCM_CSCDR2_CSPI_CLK_PRED_MASK		(0x7 << 25)
#define MXC_CCM_CSCDR2_CSPI_CLK_PODF_OFFSET		(19)
#define MXC_CCM_CSCDR2_CSPI_CLK_PODF_MASK		(0x3F << 19)
#define MXC_CCM_CSCDR2_SIM_CLK_PRED_OFFSET		(16)
#define MXC_CCM_CSCDR2_SIM_CLK_PRED_MASK		(0x7 << 16)
#define MXC_CCM_CSCDR2_SIM_CLK_PODF_OFFSET		(9)
#define MXC_CCM_CSCDR2_SIM_CLK_PODF_MASK		(0x3F << 9)
#define MXC_CCM_CSCDR2_SLIMBUS_CLK_PRED_OFFSET		(6)
#define MXC_CCM_CSCDR2_SLIMBUS_PRED_MASK		(0x7 << 6)
#define MXC_CCM_CSCDR2_SLIMBUS_PODF_OFFSET		(0)
#define MXC_CCM_CSCDR2_SLIMBUS_PODF_MASK		(0x3F)

#define MXC_CCM_CSCDR3_HSI2C_CLK_PRED_OFFSET		(16)
#define MXC_CCM_CSCDR3_HSI2C_CLK_PRED_MASK		(0x7 << 16)
#define MXC_CCM_CSCDR3_HSI2C_CLK_PODF_OFFSET		(9)
#define MXC_CCM_CSCDR3_HSI2C_CLK_PODF_MASK		(0x3F << 9)
#define MXC_CCM_CSCDR3_FIRI_CLK_PRED_OFFSET		(6)
#define MXC_CCM_CSCDR3_FIRI_CLK_PRED_MASK		(0x7 << 6)
#define MXC_CCM_CSCDR3_FIRI_CLK_PODF_OFFSET		(0)
#define MXC_CCM_CSCDR3_FIRI_CLK_PODF_MASK		(0x3F)

#define MXC_CCM_CSCDR4_CSI_MCLK2_CLK_PRED_OFFSET	(16)
#define MXC_CCM_CSCDR4_CSI_MCLK2_CLK_PRED_MASK		(0x7 << 16)
#define MXC_CCM_CSCDR4_CSI_MCLK2_CLK_PODF_OFFSET	(9)
#define MXC_CCM_CSCDR4_CSI_MCLK2_CLK_PODF_MASK		(0x3F << 9)
#define MXC_CCM_CSCDR4_CSI_MCLK1_CLK_PRED_OFFSET	(6)
#define MXC_CCM_CSCDR4_CSI_MCLK1_CLK_PRED_MASK		(0x7 << 6)
#define MXC_CCM_CSCDR4_CSI_MCLK1_CLK_PODF_OFFSET	(0)
#define MXC_CCM_CSCDR4_CSI_MCLK1_CLK_PODF_MASK		(0x3F)

#define MXC_CCM_CDHIPR_ARM_PODF_BUSY			(1 << 16)
#define MXC_CCM_CDHIPR_DDR_HF_CLK_SEL_BUSY		(1 << 8)
#define MXC_CCM_CDHIPR_DDR_PODF_BUSY			(1 << 7)
#define MXC_CCM_CDHIPR_EMI_CLK_SEL_BUSY			(1 << 6)
#define MXC_CCM_CDHIPR_PERIPH_CLK_SEL_BUSY		(1 << 5)
#define MXC_CCM_CDHIPR_NFC_IPG_INT_MEM_PODF_BUSY	(1 << 4)
#define MXC_CCM_CDHIPR_AHB_PODF_BUSY			(1 << 3)
#define MXC_CCM_CDHIPR_EMI_PODF_BUSY			(1 << 2)
#define MXC_CCM_CDHIPR_AXI_B_PODF_BUSY			(1 << 1)
#define MXC_CCM_CDHIPR_AXI_A_PODF_BUSY			(1 << 0)

#define MXC_CCM_CDCR_ARM_FREQ_SHIFT_DIVIDER		(0x1 << 2)
#define MXC_CCM_CDCR_PERIPH_CLK_DVFS_PODF_OFFSET	(0)
#define MXC_CCM_CDCR_PERIPH_CLK_DVFS_PODF_MASK		(0x3)

#define MXC_CCM_CLPCR_BYPASS_HSC_LPM_HS		(0x1 << 23)
#define MXC_CCM_CLPCR_BYPASS_SCC_LPM_HS		(0x1 << 22)
#define MX51_CCM_CLPCR_BYPASS_MAX_LPM_HS		(0x1 << 21)
#define MX53_CCM_CLPCR_BYPASS_MAX_LPM_HS		(0x1 << 25)
#define MXC_CCM_CLPCR_BYPASS_SDMA_LPM_HS	(0x1 << 20)
#define MXC_CCM_CLPCR_BYPASS_EMI_LPM_HS		(0x1 << 19)
#define MXC_CCM_CLPCR_BYPASS_IPU_LPM_HS		(0x1 << 18)
#define MXC_CCM_CLPCR_BYPASS_RTIC_LPM_HS	(0x1 << 17)
#define MXC_CCM_CLPCR_BYPASS_RNGC_LPM_HS	(0x1 << 16)
#define MXC_CCM_CLPCR_COSC_PWRDOWN		(0x1 << 11)
#define MXC_CCM_CLPCR_STBY_COUNT_OFFSET		(9)
#define MXC_CCM_CLPCR_STBY_COUNT_MASK		(0x3 << 9)
#define MXC_CCM_CLPCR_VSTBY			(0x1 << 8)
#define MXC_CCM_CLPCR_DIS_REF_OSC		(0x1 << 7)
#define MXC_CCM_CLPCR_SBYOS			(0x1 << 6)
#define MXC_CCM_CLPCR_ARM_CLK_DIS_ON_LPM	(0x1 << 5)
#define MXC_CCM_CLPCR_LPSR_CLK_SEL_OFFSET	(3)
#define MXC_CCM_CLPCR_LPSR_CLK_SEL_MASK		(0x3 << 3)
#define MXC_CCM_CLPCR_LPM_OFFSET		(0)
#define MXC_CCM_CLPCR_LPM_MASK			(0x3)

#define MXC_CCM_CISR_ARM_PODF_LOADED			(0x1 << 25)
#define MXC_CCM_CISR_NFC_IPG_INT_MEM_PODF_LOADED	(0x1 << 21)
#define MXC_CCM_CISR_AHB_PODF_LOADED			(0x1 << 20)
#define MXC_CCM_CISR_EMI_PODF_LOADED			(0x1 << 19)
#define MXC_CCM_CISR_AXI_B_PODF_LOADED			(0x1 << 18)
#define MXC_CCM_CISR_AXI_A_PODF_LOADED			(0x1 << 17)
#define MXC_CCM_CISR_DIVIDER_LOADED			(0x1 << 16)
#define MXC_CCM_CISR_COSC_READY				(0x1 << 6)
#define MXC_CCM_CISR_CKIH2_READY			(0x1 << 5)
#define MXC_CCM_CISR_CKIH_READY				(0x1 << 4)
#define MXC_CCM_CISR_FPM_READY				(0x1 << 3)
#define MXC_CCM_CISR_LRF_PLL3				(0x1 << 2)
#define MXC_CCM_CISR_LRF_PLL2				(0x1 << 1)
#define MXC_CCM_CISR_LRF_PLL1				(0x1)

#define MXC_CCM_CIMR_MASK_ARM_PODF_LOADED		(0x1 << 25)
#define MXC_CCM_CIMR_MASK_NFC_IPG_INT_MEM_PODF_LOADED	(0x1 << 21)
#define MXC_CCM_CIMR_MASK_EMI_PODF_LOADED		(0x1 << 20)
#define MXC_CCM_CIMR_MASK_AXI_C_PODF_LOADED		(0x1 << 19)
#define MXC_CCM_CIMR_MASK_AXI_B_PODF_LOADED		(0x1 << 18)
#define MXC_CCM_CIMR_MASK_AXI_A_PODF_LOADED		(0x1 << 17)
#define MXC_CCM_CIMR_MASK_DIVIDER_LOADED		(0x1 << 16)
#define MXC_CCM_CIMR_MASK_COSC_READY			(0x1 << 5)
#define MXC_CCM_CIMR_MASK_CKIH_READY			(0x1 << 4)
#define MXC_CCM_CIMR_MASK_FPM_READY			(0x1 << 3)
#define MXC_CCM_CIMR_MASK_LRF_PLL3			(0x1 << 2)
#define MXC_CCM_CIMR_MASK_LRF_PLL2			(0x1 << 1)
#define MXC_CCM_CIMR_MASK_LRF_PLL1			(0x1)

#define MXC_CCM_CCOSR_CKO2_EN_OFFSET			(0x1 << 24)
#define MXC_CCM_CCOSR_CKO2_DIV_OFFSET			(21)
#define MXC_CCM_CCOSR_CKO2_DIV_MASK			(0x7 << 21)
#define MXC_CCM_CCOSR_CKO2_SEL_OFFSET			(16)
#define MXC_CCM_CCOSR_CKO2_SEL_MASK			(0x1F << 16)
#define MXC_CCM_CCOSR_CKOL_EN				(0x1 << 7)
#define MXC_CCM_CCOSR_CKOL_DIV_OFFSET			(4)
#define MXC_CCM_CCOSR_CKOL_DIV_MASK			(0x7 << 4)
#define MXC_CCM_CCOSR_CKOL_SEL_OFFSET			(0)
#define MXC_CCM_CCOSR_CKOL_SEL_MASK			(0xF)

#define MXC_CCM_CGPR_EFUSE_PROG_SUPPLY_GATE		(0x1 << 4)
#define MXC_CCM_CGPR_FPM_SEL				(0x1 << 3)
#define MXC_CCM_CGPR_VL_L2BIST_CLKDIV_OFFSET		(0)
#define MXC_CCM_CGPR_VL_L2BIST_CLKDIV_MASK		(0x7)

#define MXC_CCM_CCGRx_CG_MASK				0x3
#define MXC_CCM_CCGRx_MOD_OFF				0x0
#define MXC_CCM_CCGRx_MOD_ON				0x3
#define MXC_CCM_CCGRx_MOD_IDLE				0x1

#define MXC_CCM_CCGRx_CG15_MASK				(0x3 << 30)
#define MXC_CCM_CCGRx_CG14_MASK				(0x3 << 28)
#define MXC_CCM_CCGRx_CG13_MASK				(0x3 << 26)
#define MXC_CCM_CCGRx_CG12_MASK				(0x3 << 24)
#define MXC_CCM_CCGRx_CG11_MASK				(0x3 << 22)
#define MXC_CCM_CCGRx_CG10_MASK				(0x3 << 20)
#define MXC_CCM_CCGRx_CG9_MASK				(0x3 << 18)
#define MXC_CCM_CCGRx_CG8_MASK				(0x3 << 16)
#define MXC_CCM_CCGRx_CG5_MASK				(0x3 << 10)
#define MXC_CCM_CCGRx_CG4_MASK				(0x3 << 8)
#define MXC_CCM_CCGRx_CG3_MASK				(0x3 << 6)
#define MXC_CCM_CCGRx_CG2_MASK				(0x3 << 4)
#define MXC_CCM_CCGRx_CG1_MASK				(0x3 << 2)
#define MXC_CCM_CCGRx_CG0_MASK				(0x3 << 0)

#define MXC_CCM_CCGRx_CG15_OFFSET			30
#define MXC_CCM_CCGRx_CG14_OFFSET			28
#define MXC_CCM_CCGRx_CG13_OFFSET			26
#define MXC_CCM_CCGRx_CG12_OFFSET			24
#define MXC_CCM_CCGRx_CG11_OFFSET			22
#define MXC_CCM_CCGRx_CG10_OFFSET			20
#define MXC_CCM_CCGRx_CG9_OFFSET			18
#define MXC_CCM_CCGRx_CG8_OFFSET			16
#define MXC_CCM_CCGRx_CG7_OFFSET			14
#define MXC_CCM_CCGRx_CG6_OFFSET			12
#define MXC_CCM_CCGRx_CG5_OFFSET			10
#define MXC_CCM_CCGRx_CG4_OFFSET			8
#define MXC_CCM_CCGRx_CG3_OFFSET			6
#define MXC_CCM_CCGRx_CG2_OFFSET			4
#define MXC_CCM_CCGRx_CG1_OFFSET			2
#define MXC_CCM_CCGRx_CG0_OFFSET			0

#define MXC_DPTC_LP_BASE	(MX51_GPC_BASE + 0x80)
#define MXC_DPTC_GP_BASE	(MX51_GPC_BASE + 0x100)
#define MXC_DVFS_CORE_BASE	(MX51_GPC_BASE + 0x180)
#define MXC_DPTC_PER_BASE	(MX51_GPC_BASE + 0x1C0)
#define MXC_PGC_IPU_BASE	(MX51_GPC_BASE + 0x220)
#define MXC_PGC_VPU_BASE	(MX51_GPC_BASE + 0x240)
#define MXC_PGC_GPU_BASE	(MX51_GPC_BASE + 0x260)
#define MXC_SRPG_NEON_BASE	(MX51_GPC_BASE + 0x280)
#define MXC_SRPG_ARM_BASE	(MX51_GPC_BASE + 0x2A0)
#define MXC_SRPG_EMPGC0_BASE	(MX51_GPC_BASE + 0x2C0)
#define MXC_SRPG_EMPGC1_BASE	(MX51_GPC_BASE + 0x2D0)
#define MXC_SRPG_MEGAMIX_BASE	(MX51_GPC_BASE + 0x2E0)
#define MXC_SRPG_EMI_BASE	(MX51_GPC_BASE + 0x300)

#define MXC_CORTEXA8_PLAT_PVID		(MX51_CORTEXA8_BASE + 0x0)
#define MXC_CORTEXA8_PLAT_GPC		(MX51_CORTEXA8_BASE + 0x4)
#define MXC_CORTEXA8_PLAT_PIC		(MX51_CORTEXA8_BASE + 0x8)
#define MXC_CORTEXA8_PLAT_LPC		(MX51_CORTEXA8_BASE + 0xC)
#define MXC_CORTEXA8_PLAT_NEON_LPC	(MX51_CORTEXA8_BASE + 0x10)
#define MXC_CORTEXA8_PLAT_ICGC		(MX51_CORTEXA8_BASE + 0x14)
#define MXC_CORTEXA8_PLAT_AMC		(MX51_CORTEXA8_BASE + 0x18)
#define MXC_CORTEXA8_PLAT_NMC		(MX51_CORTEXA8_BASE + 0x20)
#define MXC_CORTEXA8_PLAT_NMS		(MX51_CORTEXA8_BASE + 0x24)

#define MXC_DVFSTHRS		(MXC_DVFS_CORE_BASE + 0x00)
#define MXC_DVFSCOUN		(MXC_DVFS_CORE_BASE + 0x04)
#define MXC_DVFSSIG1		(MXC_DVFS_CORE_BASE + 0x08)
#define MXC_DVFSSIG0		(MXC_DVFS_CORE_BASE + 0x0C)
#define MXC_DVFSGPC0		(MXC_DVFS_CORE_BASE + 0x10)
#define MXC_DVFSGPC1		(MXC_DVFS_CORE_BASE + 0x14)
#define MXC_DVFSGPBT		(MXC_DVFS_CORE_BASE + 0x18)
#define MXC_DVFSEMAC		(MXC_DVFS_CORE_BASE + 0x1C)
#define MXC_DVFSCNTR		(MXC_DVFS_CORE_BASE + 0x20)
#define MXC_DVFSLTR0_0		(MXC_DVFS_CORE_BASE + 0x24)
#define MXC_DVFSLTR0_1		(MXC_DVFS_CORE_BASE + 0x28)
#define MXC_DVFSLTR1_0		(MXC_DVFS_CORE_BASE + 0x2C)
#define MXC_DVFSLTR1_1		(MXC_DVFS_CORE_BASE + 0x30)
#define MXC_DVFSPT0 		(MXC_DVFS_CORE_BASE + 0x34)
#define MXC_DVFSPT1 		(MXC_DVFS_CORE_BASE + 0x38)
#define MXC_DVFSPT2 		(MXC_DVFS_CORE_BASE + 0x3C)
#define MXC_DVFSPT3 		(MXC_DVFS_CORE_BASE + 0x40)

#define MXC_GPC_CNTR		(MX51_GPC_BASE + 0x0)
#define MXC_GPC_PGR		(MX51_GPC_BASE + 0x4)
#define MXC_GPC_VCR		(MX51_GPC_BASE + 0x8)
#define MXC_GPC_ALL_PU		(MX51_GPC_BASE + 0xC)
#define MXC_GPC_NEON		(MX51_GPC_BASE + 0x10)
#define MXC_GPC_PGR_ARMPG_OFFSET	8
#define MXC_GPC_PGR_ARMPG_MASK		(3 << 8)

#define MXC_PGC_IPU_PGCR	(MXC_PGC_IPU_BASE + 0x0)
#define MXC_PGC_IPU_PGSR	(MXC_PGC_IPU_BASE + 0xC)
#define MXC_PGC_VPU_PGCR	(MXC_PGC_VPU_BASE + 0x0)
#define MXC_PGC_VPU_PGSR	(MXC_PGC_VPU_BASE + 0xC)
#define MXC_PGC_GPU_PGCR	(MXC_PGC_GPU_BASE + 0x0)
#define MXC_PGC_GPU_PGSR	(MXC_PGC_GPU_BASE + 0xC)

#define MXC_PGCR_PCR		1
#define MXC_SRPGCR_PCR		1
#define MXC_EMPGCR_PCR		1
#define MXC_PGSR_PSR		1


#define MXC_CORTEXA8_PLAT_LPC_DSM	(1 << 0)
#define MXC_CORTEXA8_PLAT_LPC_DBG_DSM	(1 << 1)

#define MXC_SRPG_NEON_SRPGCR	(MXC_SRPG_NEON_BASE + 0x0)
#define MXC_SRPG_NEON_PUPSCR	(MXC_SRPG_NEON_BASE + 0x4)
#define MXC_SRPG_NEON_PDNSCR	(MXC_SRPG_NEON_BASE + 0x8)

#define MXC_SRPG_ARM_SRPGCR	(MXC_SRPG_ARM_BASE + 0x0)
#define MXC_SRPG_ARM_PUPSCR	(MXC_SRPG_ARM_BASE + 0x4)
#define MXC_SRPG_ARM_PDNSCR	(MXC_SRPG_ARM_BASE + 0x8)

#define MXC_SRPG_EMPGC0_SRPGCR	(MXC_SRPG_EMPGC0_BASE + 0x0)
#define MXC_SRPG_EMPGC0_PUPSCR	(MXC_SRPG_EMPGC0_BASE + 0x4)
#define MXC_SRPG_EMPGC0_PDNSCR	(MXC_SRPG_EMPGC0_BASE + 0x8)

#define MXC_SRPG_EMPGC1_SRPGCR	(MXC_SRPG_EMPGC1_BASE + 0x0)
#define MXC_SRPG_EMPGC1_PUPSCR	(MXC_SRPG_EMPGC1_BASE + 0x4)
#define MXC_SRPG_EMPGC1_PDNSCR	(MXC_SRPG_EMPGC1_BASE + 0x8)

#define MXC_SRPG_MEGAMIX_SRPGCR		(MXC_SRPG_MEGAMIX_BASE + 0x0)
#define MXC_SRPG_MEGAMIX_PUPSCR		(MXC_SRPG_MEGAMIX_BASE + 0x4)
#define MXC_SRPG_MEGAMIX_PDNSCR		(MXC_SRPG_MEGAMIX_BASE + 0x8)

#define MXC_SRPGC_EMI_SRPGCR	(MXC_SRPGC_EMI_BASE + 0x0)
#define MXC_SRPGC_EMI_PUPSCR	(MXC_SRPGC_EMI_BASE + 0x4)
#define MXC_SRPGC_EMI_PDNSCR	(MXC_SRPGC_EMI_BASE + 0x8)

#endif				
