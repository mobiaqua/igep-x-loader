/*
 * (C) Copyright 2009 - 2011
 * Integration Software and Electronics Engineering, <www.iseebcn.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <part.h>
#include <fat.h>
#include <asm/arch/cpu.h>
#include <asm/arch/bits.h>
#include <asm/arch/mux.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/sys_info.h>
#include <asm/arch/clocks.h>
#include <asm/arch/mem.h>
#include <asm/arch/gpio.h>
#include <linux/mtd/onenand_regs.h>
#include <linux/mtd/onenand.h>
#include <linux/mtd/nand.h>
#include <asm/arch/gpmc.h>
#include <asm/arch/mux.h>
#include <malloc.h>
#include <jffs2/load_kernel.h>
#include <linux/ctypes.h>
#include <linux/mtd/compat.h>
#include <linux/mtd/mtd.h>
#include <asm/arch/gp_timer.h>

// GPIO_LED_USER0 (led red)
#define GPIO_LED_USER0      27
// GPIO_LED_USER1 (led green)
#define GPIO_LED_USER1      26
// GPIO_LED_USER2 (led power)
#define GPIO_LED_USER2      28


u32 mfr = 0, mid = 0;
struct mtd_info *mtd_info = NULL;
struct onenand_chip *onenand_chip = NULL;
struct nand_chip *nand_chip = NULL;
struct mtd_device *current_mtd_dev = NULL;
u8 current_mtd_partnum = 0;
static __attribute__((unused)) char dev_name[] = "onenand0";
int __malloc_initialized = 0;

/* Used to index into DPLL parameter tables */
struct dpll_param {
	unsigned int m;
	unsigned int n;
	unsigned int fsel;
	unsigned int m2;
};

struct dpll_per_36x_param {
	unsigned int sys_clk;
	unsigned int m;
	unsigned int n;
	unsigned int m2;
	unsigned int m3;
	unsigned int m4;
	unsigned int m5;
	unsigned int m6;
	unsigned int m2div;
};

typedef struct dpll_param dpll_param;

/* Following functions are exported from lowlevel_init.S */
extern dpll_param *get_mpu_dpll_param();
extern dpll_param *get_iva_dpll_param();
extern dpll_param *get_core_dpll_param();
extern dpll_param *get_per_dpll_param();

extern dpll_param *get_36x_mpu_dpll_param(void);
extern dpll_param *get_36x_iva_dpll_param(void);
extern dpll_param *get_36x_core_dpll_param(void);
extern dpll_param *get_36x_per_dpll_param(void);

#define __raw_readl(a)		(*(volatile unsigned int *)(a))
#define __raw_writel(v, a)	(*(volatile unsigned int *)(a) = (v))
#define __raw_readw(a)		(*(volatile unsigned short *)(a))
#define __raw_writew(v, a)	(*(volatile unsigned short *)(a) = (v))
#define __raw_readb(a)		(*(volatile unsigned char *)(a))
#define __raw_writeb(v, a)	(*(volatile unsigned char *)(a) = (v))


/*******************************************************
 * Routine: delay
 * Description: spinning delay to use before udelay works
 ******************************************************/
static inline void delay (unsigned long loops)
{
	__asm__ volatile ("1:\n" "subs %0, %1, #1\n"
			  "bne 1b":"=r" (loops):"0"(loops));
}

void udelay (unsigned long usecs) {
	delay(usecs);
}

int is_malloc_initialized ()
{
    return __malloc_initialized;
}


/*************************************************************
 *  get_device_type(): tell if GP/HS/EMU/TST
 *************************************************************/
u32 get_device_type(void)
{
	int mode;
	mode = __raw_readl(CONTROL_STATUS) & (DEVICE_MASK);
	return mode >>= 8;
}

/************************************************
 * get_sysboot_value(void) - return SYS_BOOT[4:0]
 ************************************************/
u32 get_sysboot_value(void)
{
	return __raw_readl(CONTROL_STATUS) & IGEP00X0_SYSBOOT_MASK;
}

/*************************************************************
 * Routine: get_mem_type(void) - returns the kind of memory connected
 * to GPMC that we are trying to boot form. Uses SYS BOOT settings.
 *************************************************************/
u32 get_mem_type(void)
{
	u32 sb = get_sysboot_value();

	if (sb == IGEP00X0_SYSBOOT_NAND)
		return GPMC_NAND;
	else if (sb == IGEP00X0_SYSBOOT_ONENAND)
		return GPMC_ONENAND;
	else
		return -1;
}

/******************************************
 * get_cpu_rev(void) - extract version info
 ******************************************/
u32 get_cpu_rev(void)
{
	u32 cpuid = 0;
	/* On ES1.0 the IDCODE register is not exposed on L4
	 * so using CPU ID to differentiate
	 * between ES2.0 and ES1.0.
	 */
	__asm__ __volatile__("mrc p15, 0, %0, c0, c0, 0":"=r" (cpuid));
	if ((cpuid  & 0xf) == 0x0)
		return CPU_3430_ES1;
	else
		return CPU_3430_ES2;

}

u32 is_cpu_family(void)
{
	u32 cpuid = 0, cpu_family = 0;
	u16 hawkeye;

	__asm__ __volatile__("mrc p15, 0, %0, c0, c0, 0":"=r"(cpuid));
	if ((cpuid & 0xf) == 0x0) {
		cpu_family = CPU_OMAP34XX;
	} else {
		cpuid = __raw_readl(OMAP34XX_CONTROL_ID);
		hawkeye  = (cpuid >> HAWKEYE_SHIFT) & 0xffff;
		switch (hawkeye) {
			case HAWKEYE_OMAP34XX:
				cpu_family = CPU_OMAP34XX;
				break;
			case HAWKEYE_AM35XX:
				cpu_family = CPU_AM35XX;
				break;
			case HAWKEYE_OMAP36XX:
				cpu_family = CPU_OMAP36XX;
				break;
			default:
				cpu_family = CPU_OMAP34XX;
				break;
		}
	}
	return cpu_family;
}

/*
 * Routine: get_prod_id
 * Description: Get id info from chips
 */

#define PRODUCT_ID_SKUID	0x4830A20C
#define CPU_35XX_PID_MASK	0x0000000F
#define CPU_35XX_600MHZ_DEV	0x0
#define CPU_35XX_720MHZ_DEV	0x8

static u32 get_prod_id(void)
{
	u32 p;

	/* get production ID */
	p = __raw_readl(PRODUCT_ID_SKUID);

	return (p & CPU_35XX_PID_MASK);
}

/*****************************************************************
 * sr32 - clear & set a value in a bit range for a 32 bit address
 *****************************************************************/
void sr32(u32 addr, u32 start_bit, u32 num_bits, u32 value)
{
	u32 tmp, msk = 0;
	msk = 1 << num_bits;
	--msk;
	tmp = __raw_readl(addr) & ~(msk << start_bit);
	tmp |= value << start_bit;
	__raw_writel(tmp, addr);
}

/*********************************************************************
 * wait_on_value() - common routine to allow waiting for changes in
 *   volatile regs.
 *********************************************************************/
u32 wait_on_value(u32 read_bit_mask, u32 match_value, u32 read_addr, u32 bound)
{
	u32 i = 0, val;
	do {
		++i;
		val = __raw_readl(read_addr) & read_bit_mask;
		if (val == match_value)
			return 1;
		if (i == bound)
			return 0;
	} while (1);
}

/*************************************************************
 * get_sys_clk_speed - determine reference oscillator speed
 *  based on known 32kHz clock and gptimer.
 *************************************************************/
u32 get_osc_clk_speed(void)
{
	u32 start, cstart, cend, cdiff, cdiv, val;

	val = __raw_readl(PRM_CLKSRC_CTRL);

	if (val & BIT7)
		cdiv = 2;
	else if (val & BIT6)
		cdiv = 1;
	else
		/*
		 * Should never reach here!
		 * TBD: Add a WARN()/BUG()
		 *      For now, assume divider as 1.
		 */
		cdiv = 1;

	/* enable timer2 */
	val = __raw_readl(CM_CLKSEL_WKUP) | BIT0;
	__raw_writel(val, CM_CLKSEL_WKUP);	/* select sys_clk for GPT1 */

	/* Enable I and F Clocks for GPT1 */
	val = __raw_readl(CM_ICLKEN_WKUP) | BIT0 | BIT2;
	__raw_writel(val, CM_ICLKEN_WKUP);
	val = __raw_readl(CM_FCLKEN_WKUP) | BIT0;
	__raw_writel(val, CM_FCLKEN_WKUP);

	__raw_writel(0, OMAP34XX_GPT1 + TLDR);	/* start counting at 0 */
	__raw_writel(GPT_EN, OMAP34XX_GPT1 + TCLR);     /* enable clock */
	/* enable 32kHz source *//* enabled out of reset */
	/* determine sys_clk via gauging */

	start = 20 + __raw_readl(S32K_CR);	/* start time in 20 cycles */
	while (__raw_readl(S32K_CR) < start);	/* dead loop till start time */
	cstart = __raw_readl(OMAP34XX_GPT1 + TCRR);	/* get start sys_clk count */
	while (__raw_readl(S32K_CR) < (start + 20));	/* wait for 40 cycles */
	cend = __raw_readl(OMAP34XX_GPT1 + TCRR);	/* get end sys_clk count */
	cdiff = cend - cstart;				/* get elapsed ticks */

	if (cdiv == 2)
	{
		cdiff *= 2;
	}

	/* based on number of ticks assign speed */
	if (cdiff > 19000)
		return (S38_4M);
	else if (cdiff > 15200)
		return (S26M);
	else if (cdiff > 13000)
		return (S24M);
	else if (cdiff > 9000)
		return (S19_2M);
	else if (cdiff > 7600)
		return (S13M);
	else
		return (S12M);
}


/******************************************************************************
 * get_sys_clkin_sel() - returns the sys_clkin_sel field value based on
 *   -- input oscillator clock frequency.
 *
 *****************************************************************************/
void get_sys_clkin_sel(u32 osc_clk, u32 *sys_clkin_sel)
{
	if (osc_clk == S38_4M)
		*sys_clkin_sel = 4;
	else if (osc_clk == S26M)
		*sys_clkin_sel = 3;
	else if (osc_clk == S19_2M)
		*sys_clkin_sel = 2;
	else if (osc_clk == S13M)
		*sys_clkin_sel = 1;
	else if (osc_clk == S12M)
		*sys_clkin_sel = 0;
}

/*
 * OMAP34x/35x specific functions
 */
static void dpll3_init_34xx(u32 sil_index, u32 clk_index)
{
	dpll_param *ptr;

	/* Getting the base address of Core DPLL param table*/
	ptr = (dpll_param *)get_core_dpll_param();

	/* Moving it to the right sysclk and ES rev base */
	ptr = ptr + 2*clk_index + sil_index;

	/* CORE DPLL */
	/* Select relock bypass: CM_CLKEN_PLL[0:2] */
	sr32(CM_CLKEN_PLL, 0, 3, PLL_FAST_RELOCK_BYPASS);
	wait_on_value(BIT0, 0, CM_IDLEST_CKGEN, LDELAY);

	/* CM_CLKSEL1_EMU[DIV_DPLL3] */
	sr32(CM_CLKSEL1_EMU, 16, 5, CORE_M3X2);

	/* M2 (CORE_DPLL_CLKOUT_DIV): CM_CLKSEL1_PLL[27:31] */
	sr32(CM_CLKSEL1_PLL, 27, 5, ptr->m2);

	/* M (CORE_DPLL_MULT): CM_CLKSEL1_PLL[16:26] */
	sr32(CM_CLKSEL1_PLL, 16, 11, ptr->m);

	/* N (CORE_DPLL_DIV): CM_CLKSEL1_PLL[8:14] */
	sr32(CM_CLKSEL1_PLL, 8, 7, ptr->n);

	/* Source is the CM_96M_FCLK: CM_CLKSEL1_PLL[6] */
	sr32(CM_CLKSEL1_PLL, 6, 1, 0);

	sr32(CM_CLKSEL_CORE, 8, 4, CORE_SSI_DIV);	/* ssi */
	sr32(CM_CLKSEL_CORE, 4, 2, CORE_FUSB_DIV);	/* fsusb */
	sr32(CM_CLKSEL_CORE, 2, 2, CORE_L4_DIV);	/* l4 */
	sr32(CM_CLKSEL_CORE, 0, 2, CORE_L3_DIV);	/* l3 */

	sr32(CM_CLKSEL_GFX,  0, 3, GFX_DIV_34X);	/* gfx */
	sr32(CM_CLKSEL_WKUP, 1, 2, WKUP_RSM);		/* reset mgr */

	/* FREQSEL (CORE_DPLL_FREQSEL): CM_CLKEN_PLL[4:7] */
	sr32(CM_CLKEN_PLL,   4, 4, ptr->fsel);
	sr32(CM_CLKEN_PLL,   0, 3, PLL_LOCK);		/* lock mode */

	wait_on_value(BIT0, 1, CM_IDLEST_CKGEN, LDELAY);
}

static void dpll4_init_34xx(u32 sil_index, u32 clk_index)
{
	dpll_param *ptr;

	ptr = (dpll_param *)get_per_dpll_param();

	/* Moving it to the right sysclk base */
	ptr = ptr + clk_index;

	/* EN_PERIPH_DPLL: CM_CLKEN_PLL[16:18] */
	sr32(CM_CLKEN_PLL, 16, 3, PLL_STOP);
	wait_on_value(BIT1, 0, CM_IDLEST_CKGEN, LDELAY);

	sr32(CM_CLKSEL1_EMU, 24, 5, PER_M6X2);		/* set M6 */
	sr32(CM_CLKSEL_CAM, 0, 5, PER_M5X2);		/* set M5 */
	sr32(CM_CLKSEL_DSS, 0, 5, PER_M4X2);		/* set M4 */
	sr32(CM_CLKSEL_DSS, 8, 5, PER_M3X2);		/* set M3 */

	/* M2 (DIV_96M): CM_CLKSEL3_PLL[0:4] */
	sr32(CM_CLKSEL3_PLL, 0, 5, ptr->m2);

	/* M (PERIPH_DPLL_MULT): CM_CLKSEL2_PLL[8:18] */
	sr32(CM_CLKSEL2_PLL, 8, 11, ptr->m);

	/* N (PERIPH_DPLL_DIV): CM_CLKSEL2_PLL[0:6] */
	sr32(CM_CLKSEL2_PLL, 0, 7, ptr->n);

	/* FREQSEL (PERIPH_DPLL_FREQSEL): CM_CLKEN_PLL[20:23] */
	sr32(CM_CLKEN_PLL, 20, 4, ptr->fsel);

	/* LOCK MODE (EN_PERIPH_DPLL) : CM_CLKEN_PLL[16:18] */
	sr32(CM_CLKEN_PLL, 16, 3, PLL_LOCK);
	wait_on_value(BIT1, 2, CM_IDLEST_CKGEN, LDELAY);
}

static void mpu_init_34xx(u32 sil_index, u32 clk_index)
{
	dpll_param *ptr;

	/* Getting the base address to MPU DPLL param table*/
	ptr = (dpll_param *)get_mpu_dpll_param();

	/* Moving it to the right sysclk and ES rev base */
	ptr = ptr + 2*clk_index + sil_index;

	/* MPU DPLL (unlocked already) */
	/* M2 (MPU_DPLL_CLKOUT_DIV) : CM_CLKSEL2_PLL_MPU[0:4] */
	sr32(CM_CLKSEL2_PLL_MPU, 0, 5, ptr->m2);

	/* M (MPU_DPLL_MULT) : CM_CLKSEL2_PLL_MPU[8:18] */
	sr32(CM_CLKSEL1_PLL_MPU, 8, 11, ptr->m);

	/* N (MPU_DPLL_DIV) : CM_CLKSEL2_PLL_MPU[0:6] */
	sr32(CM_CLKSEL1_PLL_MPU, 0, 7, ptr->n);

	/* FREQSEL (MPU_DPLL_FREQSEL) : CM_CLKEN_PLL_MPU[4:7] */
	sr32(CM_CLKEN_PLL_MPU, 4, 4, ptr->fsel);
}

static void iva_init_34xx(u32 sil_index, u32 clk_index)
{
	dpll_param *ptr;

	/* Getting the base address to IVA DPLL param table*/
	ptr = (dpll_param *)get_iva_dpll_param();

	/* Moving it to the right sysclk and ES rev base */
	ptr = ptr + 2*clk_index + sil_index;

	/* IVA DPLL */
	/* EN_IVA2_DPLL : CM_CLKEN_PLL_IVA2[0:2] */
	sr32(CM_CLKEN_PLL_IVA2, 0, 3, PLL_STOP);
	wait_on_value(BIT0, 0, CM_IDLEST_PLL_IVA2, LDELAY);

	/* M2 (IVA2_DPLL_CLKOUT_DIV) : CM_CLKSEL2_PLL_IVA2[0:4] */
	sr32(CM_CLKSEL2_PLL_IVA2, 0, 5, ptr->m2);

	/* M (IVA2_DPLL_MULT) : CM_CLKSEL1_PLL_IVA2[8:18] */
	sr32(CM_CLKSEL1_PLL_IVA2, 8, 11, ptr->m);

	/* N (IVA2_DPLL_DIV) : CM_CLKSEL1_PLL_IVA2[0:6] */
	sr32(CM_CLKSEL1_PLL_IVA2, 0, 7, ptr->n);

	/* FREQSEL (IVA2_DPLL_FREQSEL) : CM_CLKEN_PLL_IVA2[4:7] */
	sr32(CM_CLKEN_PLL_IVA2, 4, 4, ptr->fsel);

	/* LOCK MODE (EN_IVA2_DPLL) : CM_CLKEN_PLL_IVA2[0:2] */
	sr32(CM_CLKEN_PLL_IVA2, 0, 3, PLL_LOCK);

	wait_on_value(BIT0, 1, CM_IDLEST_PLL_IVA2, LDELAY);
}

/*
 * OMAP3630 specific functions
 */
static void dpll3_init_36xx(u32 sil_index, u32 clk_index)
{
	dpll_param *ptr;

 	/* Getting the base address of Core DPLL param table*/
	ptr = (dpll_param *)get_36x_core_dpll_param();

 	/* Moving it to the right sysclk and ES rev base */
	ptr += clk_index;

 	/* CORE DPLL */
	/* Select relock bypass: CM_CLKEN_PLL[0:2] */
 	sr32(CM_CLKEN_PLL, 0, 3, PLL_FAST_RELOCK_BYPASS);
 	wait_on_value(BIT0, 0, CM_IDLEST_CKGEN, LDELAY);

	/* CM_CLKSEL1_EMU[DIV_DPLL3] */
	sr32(CM_CLKSEL1_EMU, 16, 5, CORE_M3X2);

	/* M2 (CORE_DPLL_CLKOUT_DIV): CM_CLKSEL1_PLL[27:31] */
	sr32(CM_CLKSEL1_PLL, 27, 5, ptr->m2);

	/* M (CORE_DPLL_MULT): CM_CLKSEL1_PLL[16:26] */
	sr32(CM_CLKSEL1_PLL, 16, 11, ptr->m);

	/* N (CORE_DPLL_DIV): CM_CLKSEL1_PLL[8:14] */
	sr32(CM_CLKSEL1_PLL, 8, 7, ptr->n);

	/* Source is the CM_96M_FCLK: CM_CLKSEL1_PLL[6] */
	sr32(CM_CLKSEL1_PLL, 6, 1, 0);

 	sr32(CM_CLKSEL_CORE, 8, 4, CORE_SSI_DIV);	/* ssi */
 	sr32(CM_CLKSEL_CORE, 4, 2, CORE_FUSB_DIV);	/* fsusb */
 	sr32(CM_CLKSEL_CORE, 2, 2, CORE_L4_DIV);	/* l4 */
 	sr32(CM_CLKSEL_CORE, 0, 2, CORE_L3_DIV);	/* l3 */

	sr32(CM_CLKSEL_GFX,  0, 3, GFX_DIV_36X);	/* gfx */
 	sr32(CM_CLKSEL_WKUP, 1, 2, WKUP_RSM);		/* reset mgr */

	/* FREQSEL (CORE_DPLL_FREQSEL): CM_CLKEN_PLL[4:7] */
	sr32(CM_CLKEN_PLL,   4, 4, ptr->fsel);
	sr32(CM_CLKEN_PLL,   0, 3, PLL_LOCK);		/* lock mode */

 	wait_on_value(BIT0, 1, CM_IDLEST_CKGEN, LDELAY);
}

static void dpll4_init_36xx(u32 sil_index, u32 clk_index)
{
	struct dpll_per_36x_param *ptr;

	ptr = (struct dpll_per_36x_param *)get_36x_per_dpll_param();

	ptr += clk_index;

	/* EN_PERIPH_DPLL: CM_CLKEN_PLL[16:18] */
 	sr32(CM_CLKEN_PLL, 16, 3, PLL_STOP);
 	wait_on_value(BIT1, 0, CM_IDLEST_CKGEN, LDELAY);

	/* M6 (DIV_DPLL4): CM_CLKSEL1_EMU[24:29] */
	sr32(CM_CLKSEL1_EMU, 24, 6, ptr->m6);

	/* M5 (CLKSEL_CAM): CM_CLKSEL1_EMU[0:5] */
	sr32(CM_CLKSEL_CAM, 0, 6, ptr->m5);

	/* M4 (CLKSEL_DSS1): CM_CLKSEL_DSS[0:5] */
	sr32(CM_CLKSEL_DSS, 0, 6, ptr->m4);

	/* M3 (CLKSEL_DSS1): CM_CLKSEL_DSS[8:13] */
	sr32(CM_CLKSEL_DSS, 8, 6, ptr->m3);

	/* M2 (DIV_96M): CM_CLKSEL3_PLL[0:4] */
	sr32(CM_CLKSEL3_PLL, 0, 5, ptr->m2);

	/* M (PERIPH_DPLL_MULT): CM_CLKSEL2_PLL[8:19] */
	sr32(CM_CLKSEL2_PLL, 8, 12, ptr->m);

	/* N (PERIPH_DPLL_DIV): CM_CLKSEL2_PLL[0:6] */
	sr32(CM_CLKSEL2_PLL, 0, 7, ptr->n);

	/* M2DIV (CLKSEL_96M): CM_CLKSEL_CORE[12:13] */
	sr32(CM_CLKSEL_CORE, 12, 2, ptr->m2div);

	/* LOCK MODE (EN_PERIPH_DPLL): CM_CLKEN_PLL[16:18] */
	sr32(CM_CLKEN_PLL, 16, 3, PLL_LOCK);
 	wait_on_value(BIT1, 2, CM_IDLEST_CKGEN, LDELAY);
}

static void mpu_init_36xx(u32 sil_index, u32 clk_index)
{
	dpll_param *ptr;

 	/* Getting the base address to MPU DPLL param table*/
	ptr = (dpll_param *)get_36x_mpu_dpll_param();

 	/* Moving it to the right sysclk and ES rev base */
	// ptr = ptr + (2*clk_index) + sil_index;

 	/* MPU DPLL (unlocked already) */
	/* M2 (MPU_DPLL_CLKOUT_DIV) : CM_CLKSEL2_PLL_MPU[0:4] */
	sr32(CM_CLKSEL2_PLL_MPU, 0, 5, ptr->m2);

	/* M (MPU_DPLL_MULT) : CM_CLKSEL2_PLL_MPU[8:18] */
	sr32(CM_CLKSEL1_PLL_MPU, 8, 11, ptr->m);

	/* N (MPU_DPLL_DIV) : CM_CLKSEL2_PLL_MPU[0:6] */
	sr32(CM_CLKSEL1_PLL_MPU, 0, 7, ptr->n);

	/* LOCK MODE (EN_MPU_DPLL) : CM_CLKEN_PLL_IVA2[0:2] */
	// sr32(CM_CLKEN_PLL_MPU, 0, 3, PLL_LOCK);
 	// wait_on_value(BIT0, 1, CM_IDLEST_PLL_MPU, LDELAY);
}

#ifdef __ENABLE_IVA__
static void iva_init_36xx(u32 sil_index, u32 clk_index)
{
	dpll_param *ptr;

 	/* Getting the base address to IVA DPLL param table*/
	ptr = (dpll_param *)get_36x_iva_dpll_param();

 	/* Moving it to the right sysclk and ES rev base */
	// ptr = ptr + (2*clk_index) + sil_index;

	/* IVA DPLL */
	/* EN_IVA2_DPLL : CM_CLKEN_PLL_IVA2[0:2] */
 	sr32(CM_CLKEN_PLL_IVA2, 0, 3, PLL_STOP);
 	wait_on_value(BIT0, 0, CM_IDLEST_PLL_IVA2, LDELAY);

	/* M2 (IVA2_DPLL_CLKOUT_DIV) : CM_CLKSEL2_PLL_IVA2[0:4] */
	sr32(CM_CLKSEL2_PLL_IVA2, 0, 5, ptr->m2);

	/* M (IVA2_DPLL_MULT) : CM_CLKSEL1_PLL_IVA2[8:18] */
	sr32(CM_CLKSEL1_PLL_IVA2, 8, 11, ptr->m);

	/* N (IVA2_DPLL_DIV) : CM_CLKSEL1_PLL_IVA2[0:6] */
	sr32(CM_CLKSEL1_PLL_IVA2, 0, 7, ptr->n);

	/* LOCK MODE (EN_IVA2_DPLL) : CM_CLKEN_PLL_IVA2[0:2] */
	sr32(CM_CLKEN_PLL_IVA2, 0, 3, PLL_LOCK);

 	wait_on_value(BIT0, 1, CM_IDLEST_PLL_IVA2, LDELAY);
}
#endif

/******************************************************************************
 * prcm_init() - inits clocks for PRCM as defined in clocks.h
 *   -- called from SRAM, or Flash (using temp SRAM stack).
 *****************************************************************************/
void prcm_init(void)
{
	u32 osc_clk=0, sys_clkin_sel;
	u32 clk_index, sil_index;

	/* Gauge the input clock speed and find out the sys_clkin_sel
	 * value corresponding to the input clock.
	 */
	osc_clk = get_osc_clk_speed();
	get_sys_clkin_sel(osc_clk, &sys_clkin_sel);

	sr32(PRM_CLKSEL, 0, 3, sys_clkin_sel); /* set input crystal speed */

	/* If the input clock is greater than 19.2M always divide/2 */
	/*
	 * On OMAP3630, DDR data corruption has been observed on OFF mode
	 * exit if the sys clock was lower than 26M. As a work around,
	 * OMAP3630 is operated at 26M sys clock and this internal division
	 * is not performed.
	 */
	if((is_cpu_family() != CPU_OMAP36XX) && (sys_clkin_sel > 2)) {
		sr32(PRM_CLKSRC_CTRL, 6, 2, 2);/* input clock divider */
		clk_index = sys_clkin_sel/2;
	} else {
		sr32(PRM_CLKSRC_CTRL, 6, 2, 1);/* input clock divider */
		clk_index = sys_clkin_sel;
	}

	if (is_cpu_family() == CPU_OMAP36XX) {
        sr32(CM_CLKEN_PLL_MPU, 0, 3, PLL_LOW_POWER_BYPASS);
        wait_on_value(ST_MPU_CLK, 0, CM_IDLEST_PLL_MPU, LDELAY);

        sr32(PRM_CLKSRC_CTRL, 8, 1, 0);

		dpll3_init_36xx(0, clk_index);
		dpll4_init_36xx(0, clk_index);
		mpu_init_36xx(0, clk_index);
#ifdef __ENABLE_IVA__
		iva_init_36xx(0, clk_index);
#endif
		sr32(CM_CLKEN_PLL_MPU, 0, 3, PLL_LOCK);
		wait_on_value(ST_MPU_CLK, 1, CM_IDLEST_PLL_MPU, LDELAY);
	} else {
		sil_index = get_cpu_rev() - 1;

		/* The DPLL tables are defined according to sysclk value and
		 * silicon revision. The clk_index value will be used to get
		 * the values for that input sysclk from the DPLL param table
		 * and sil_index will get the values for that SysClk for the
		 * appropriate silicon rev.
		 */

		/* Unlock MPU DPLL (slows things down, and needed later) */
		sr32(CM_CLKEN_PLL_MPU, 0, 3, PLL_LOW_POWER_BYPASS);
		wait_on_value(BIT0, 0, CM_IDLEST_PLL_MPU, LDELAY);

		dpll3_init_34xx(sil_index, clk_index);
		dpll4_init_34xx(sil_index, clk_index);
		iva_init_34xx(sil_index, clk_index);
		mpu_init_34xx(sil_index, clk_index);

		/* Lock MPU DPLL to set frequency */
		sr32(CM_CLKEN_PLL_MPU, 0, 3, PLL_LOCK);
		wait_on_value(BIT0, 1, CM_IDLEST_PLL_MPU, LDELAY);
	}

        /* Set up GPTimers to sys_clk source only */
        sr32(CM_CLKSEL_PER, 0, 8, 0xff);
        sr32(CM_CLKSEL_WKUP, 0, 1, 1);

        delay(5000);
}

/*****************************************
 * Routine: secure_unlock
 * Description: Setup security registers for access
 * (GP Device only)
 *****************************************/
void secure_unlock(void)
{
	/* Permission values for registers -Full fledged permissions to all */
#define UNLOCK_1 0xFFFFFFFF
#define UNLOCK_2 0x00000000
#define UNLOCK_3 0x0000FFFF
	/* Protection Module Register Target APE (PM_RT) */
	__raw_writel(UNLOCK_1, RT_REQ_INFO_PERMISSION_1);
	__raw_writel(UNLOCK_1, RT_READ_PERMISSION_0);
	__raw_writel(UNLOCK_1, RT_WRITE_PERMISSION_0);
	__raw_writel(UNLOCK_2, RT_ADDR_MATCH_1);

	__raw_writel(UNLOCK_3, GPMC_REQ_INFO_PERMISSION_0);
	__raw_writel(UNLOCK_3, GPMC_READ_PERMISSION_0);
	__raw_writel(UNLOCK_3, GPMC_WRITE_PERMISSION_0);

	__raw_writel(UNLOCK_3, OCM_REQ_INFO_PERMISSION_0);
	__raw_writel(UNLOCK_3, OCM_READ_PERMISSION_0);
	__raw_writel(UNLOCK_3, OCM_WRITE_PERMISSION_0);
	__raw_writel(UNLOCK_2, OCM_ADDR_MATCH_2);

	/* IVA Changes */
	__raw_writel(UNLOCK_3, IVA2_REQ_INFO_PERMISSION_0);
	__raw_writel(UNLOCK_3, IVA2_READ_PERMISSION_0);
	__raw_writel(UNLOCK_3, IVA2_WRITE_PERMISSION_0);

	__raw_writel(UNLOCK_1, SMS_RG_ATT0);	/* SDRC region 0 public */
}

/**********************************************************
 * Routine: try_unlock_sram()
 * Description: If chip is GP type, unlock the SRAM for
 *  general use.
 ***********************************************************/
void try_unlock_memory(void)
{
	int mode;

	/* if GP device unlock device SRAM for general use */
	/* secure code breaks for Secure/Emulation device - HS/E/T */
	mode = get_device_type();
	if (mode == GP_DEVICE)
		secure_unlock();
	return;
}

void config_sdram(int actima, int actimb)
{
		/* reset sdrc controller */
	__raw_writel(SOFTRESET, SDRC_SYSCONFIG);
	wait_on_value(BIT0, BIT0, SDRC_STATUS, 12000000);
	__raw_writel(0, SDRC_SYSCONFIG);

	/* setup sdrc to ball mux */
	__raw_writel(SDP_SDRC_SHARING, SDRC_SHARING);
	__raw_writel(0x2, SDRC_CS_CFG); /* 256 MB/bank */

	/* CS0 SDRC Mode Register */
	__raw_writel((0x03588019|B_ALL), SDRC_MCFG_0);

	/* CS1 SDRC Mode Register */
	__raw_writel((0x03588019|B_ALL), SDRC_MCFG_1);

	/* Set timings */
	__raw_writel(actima, SDRC_ACTIM_CTRLA_0);
	__raw_writel(actimb, SDRC_ACTIM_CTRLB_0);
	__raw_writel(actima, SDRC_ACTIM_CTRLA_1);
	__raw_writel(actimb, SDRC_ACTIM_CTRLB_1);

	__raw_writel(SDP_SDRC_RFR_CTRL_200, SDRC_RFR_CTRL_0);
	__raw_writel(SDP_SDRC_RFR_CTRL_200, SDRC_RFR_CTRL_1);

	__raw_writel(SDP_SDRC_POWER_POP, SDRC_POWER);

	/* init sequence for mDDR/mSDR using manual commands (DDR is different) */
	__raw_writel(CMD_NOP, SDRC_MANUAL_0);
	__raw_writel(CMD_NOP, SDRC_MANUAL_1);

	delay(5000);

	__raw_writel(CMD_PRECHARGE, SDRC_MANUAL_0);
	__raw_writel(CMD_PRECHARGE, SDRC_MANUAL_1);

	__raw_writel(CMD_AUTOREFRESH, SDRC_MANUAL_0);
	__raw_writel(CMD_AUTOREFRESH, SDRC_MANUAL_1);

	__raw_writel(CMD_AUTOREFRESH, SDRC_MANUAL_0);
	__raw_writel(CMD_AUTOREFRESH, SDRC_MANUAL_1);

	/* set mr0 */
	__raw_writel(SDP_SDRC_MR_0_DDR, SDRC_MR_0);
	__raw_writel(SDP_SDRC_MR_0_DDR, SDRC_MR_1);

	/* set up dll */
	__raw_writel(SDP_SDRC_DLLAB_CTRL, SDRC_DLLA_CTRL);
	delay(0x2000);	/* give time to lock */
}

/*
 * Configure Hynix NAND flash RAM memory module
 */
void config_sdram_hynix(void)
{
	config_sdram(HYNIX_V_ACTIMA_200, HYNIX_V_ACTIMB_200);
}

/*
 * Configure Micron NAND flash RAM memory module
 */
void config_sdram_mt29cxgxxmaxx(void)
{
	config_sdram(MT29CXGXXMAXX_V_ACTIMA_200, MT29CXGXXMAXX_V_ACTIMB_200);
}

/*
 * Config flash storage NAND module
 */
void config_nand_flash(void)
{
	/* global settings */
	__raw_writel(0x10, GPMC_SYSCONFIG);	/* smart idle */
	__raw_writel(0x0, GPMC_IRQENABLE);	/* isr's sources masked */
	__raw_writel(0, GPMC_TIMEOUT_CONTROL);/* timeout disable */
	__raw_writel(0x10, GPMC_CONFIG);	/* disable Write protect */

	/* Set the GPMC Vals . For NAND boot on 3430SDP, NAND is mapped at CS0
         *  , NOR at CS1 and MPDB at CS3. And oneNAND boot, we map oneNAND at CS0.
	 *  We configure only GPMC CS0 with required values. Configiring other devices
	 *  at other CS in done in u-boot anyway. So we don't have to bother doing it here.
         */
	__raw_writel(0 , GPMC_CONFIG7 + GPMC_CONFIG_CS0);
	delay(1000);

	__raw_writel(M_NAND_GPMC_CONFIG1, GPMC_CONFIG1 + GPMC_CONFIG_CS0);
	__raw_writel(M_NAND_GPMC_CONFIG2, GPMC_CONFIG2 + GPMC_CONFIG_CS0);
	__raw_writel(M_NAND_GPMC_CONFIG3, GPMC_CONFIG3 + GPMC_CONFIG_CS0);
	__raw_writel(M_NAND_GPMC_CONFIG4, GPMC_CONFIG4 + GPMC_CONFIG_CS0);
	__raw_writel(M_NAND_GPMC_CONFIG5, GPMC_CONFIG5 + GPMC_CONFIG_CS0);
	__raw_writel(M_NAND_GPMC_CONFIG6, GPMC_CONFIG6 + GPMC_CONFIG_CS0);
	__raw_writel(M_NAND_GPMC_CONFIG7, GPMC_CONFIG7 + GPMC_CONFIG_CS0);

	__raw_writel(4144, GPMC_BASE + GPMC_ECC_CONFIG);
	__raw_writel(0, GPMC_BASE + GPMC_ECC_CONTROL);

	/* Enable the GPMC Mapping */
	__raw_writel((((OMAP34XX_GPMC_CS0_SIZE & 0xF)<<8) |
		      ((NAND_BASE_ADR>>24) & 0x3F) |
		      (1<<6) ),  (GPMC_CONFIG7 + GPMC_CONFIG_CS0));
	delay(2000);
}

/* nand_command: Send a flash command to the flash chip */
static void nand_command (u8 command)
{
	struct gpmc* gpmc_cfg = (struct gpmc *)GPMC_BASE;
	__raw_writeb(command, &gpmc_cfg->cs[0].nand_cmd);
	if (command == NAND_CMD_RESET) {
		unsigned char ret_val;
		__raw_writeb(NAND_CMD_STATUS, &gpmc_cfg->cs[0].nand_cmd);
		do {
			/* Wait until ready */
				ret_val = __raw_readb(&gpmc_cfg->cs[0].nand_dat);
		} while ((ret_val & NAND_STATUS_READY) != NAND_STATUS_READY);
	}
}


void read_nand_manufacturer_id (u32 *m, u32 *i)
{
	struct gpmc* gpmc_cfg = (struct gpmc *)GPMC_BASE;
	nand_command(NAND_CMD_RESET);
	nand_command(NAND_CMD_READID);
	__raw_writeb(0x0, &gpmc_cfg->cs[0].nand_adr);
	delay(2000);
    *m = __raw_readb(&gpmc_cfg->cs[0].nand_dat);
    *i = __raw_readb(&gpmc_cfg->cs[0].nand_dat);
}

/*
 * Helper macros
 */
#define ONENAND_START_PAGE		0
#define ONENAND_PAGES_PER_BLOCK		64

#define onenand_readw(a)	(*(volatile unsigned short *)(a))
#define onenand_writew(v, a)	((*(volatile unsigned short *)(a)) = (u16) (v))

#define THIS_ONENAND(a)		(ONENAND_ADDR + (a))

#define ONENAND_MANUF_ID()		\
  (*(volatile unsigned short *)(THIS_ONENAND(ONENAND_REG_MANUFACTURER_ID)))

#define ONENAND_DEVICE_ID()		\
  (*(volatile unsigned short *)(THIS_ONENAND(ONENAND_REG_DEVICE_ID)))

#define ONENAND_VERSION_ID()	\
  (*(volatile unsigned short *)(THIS_ONENAND(ONENAND_REG_VERSION_ID)))

#define ONENAND_TECHNOLOGY()	\
  (*(volatile unsigned short *)(THIS_ONENAND(ONENAND_REG_TECHNOLOGY)))


/*
 * OneNAND Flash Manufacturer ID Codes
 */
#define ONENAND_MFR_SAMSUNG	0xec
#define ONENAND_MFR_NUMONYX	0x20

/*
 * OneNAND Flash Devices ID Codes
 */
#define ONENAND_KFM1G16Q2A_DEV_ID	0x30
#define ONENAND_KFN2G16Q2A_DEV_ID	0x40
#define ONENAND_NAND01GR4E_DEV_ID	0x30
#define ONENAND_NAND02GR4E_DEV_ID	0x40
#define ONENAND_NAND04GR4E_DEV_ID	0x58

/*********************************************************************
 * config_sdram_m65kx002am() - 2 dice of 2Gb, DDR x32 I/O, 4KB page
 *********************************************************************/
void config_sdram_m65kx002am(unsigned short MemID)
{
	/* M65KX002AM - 2 dice of 2Gb */
	/* reset sdrc controller */
	__raw_writel(SOFTRESET, SDRC_SYSCONFIG);
	wait_on_value(BIT0, BIT0, SDRC_STATUS, 12000000);
	__raw_writel(0, SDRC_SYSCONFIG);

	/* setup sdrc to ball mux */
	__raw_writel(SDP_SDRC_SHARING, SDRC_SHARING);

    switch(MemID){
        case ONENAND_NAND02GR4E_DEV_ID:
            __raw_writel(MK65KX001AM_SDRC_MCDCFG, SDRC_MCFG_0);
            break;
        case ONENAND_NAND04GR4E_DEV_ID:
            __raw_writel(0x2, SDRC_CS_CFG); /* 256 MB/bank */
            __raw_writel(MK65KX002AM_SDRC_MCDCFG, SDRC_MCFG_0);
            __raw_writel(MK65KX002AM_SDRC_MCDCFG, SDRC_MCFG_1);
            break;
        default:
            hang();
    }

    /* Set timings */
    if(is_cpu_family() == CPU_OMAP36XX){
        __raw_writel(NUMONYX_SDRC_ACTIM_CTRLA_200, SDRC_ACTIM_CTRLA_0);
        __raw_writel(NUMONYX_SDRC_ACTIM_CTRLB_200, SDRC_ACTIM_CTRLB_0);
        __raw_writel(NUMONYX_SDRC_ACTIM_CTRLA_200, SDRC_ACTIM_CTRLA_1);
        __raw_writel(NUMONYX_SDRC_ACTIM_CTRLB_200, SDRC_ACTIM_CTRLB_1);
        __raw_writel(SDP_SDRC_RFR_CTRL_200, SDRC_RFR_CTRL_0);
        if(MemID == ONENAND_NAND04GR4E_DEV_ID)
            __raw_writel(SDP_SDRC_RFR_CTRL_200, SDRC_RFR_CTRL_1);
    }
    else{
        __raw_writel(NUMONYX_SDRC_ACTIM_CTRLA_165, SDRC_ACTIM_CTRLA_0);
        __raw_writel(NUMONYX_SDRC_ACTIM_CTRLB_165, SDRC_ACTIM_CTRLB_0);
        __raw_writel(NUMONYX_SDRC_ACTIM_CTRLA_165, SDRC_ACTIM_CTRLA_1);
        __raw_writel(NUMONYX_SDRC_ACTIM_CTRLB_165, SDRC_ACTIM_CTRLB_1);
        __raw_writel(SDP_SDRC_RFR_CTRL_165, SDRC_RFR_CTRL_0);
        if(MemID == ONENAND_NAND04GR4E_DEV_ID)
            __raw_writel(SDP_SDRC_RFR_CTRL_165, SDRC_RFR_CTRL_1);

    }

	__raw_writel(SDP_SDRC_POWER_POP, SDRC_POWER);

	/* init sequence for mDDR/mSDR using manual commands (DDR is different) */
	__raw_writel(CMD_NOP, SDRC_MANUAL_0);
	if(MemID == ONENAND_NAND04GR4E_DEV_ID)
        __raw_writel(CMD_NOP, SDRC_MANUAL_1);

	delay(5000);

	__raw_writel(CMD_PRECHARGE, SDRC_MANUAL_0);
	if(MemID == ONENAND_NAND04GR4E_DEV_ID)
        __raw_writel(CMD_PRECHARGE, SDRC_MANUAL_1);

	__raw_writel(CMD_AUTOREFRESH, SDRC_MANUAL_0);

    if(MemID == ONENAND_NAND04GR4E_DEV_ID)
        __raw_writel(CMD_AUTOREFRESH, SDRC_MANUAL_1);

	__raw_writel(CMD_AUTOREFRESH, SDRC_MANUAL_0);
	if(MemID == ONENAND_NAND04GR4E_DEV_ID)
        __raw_writel(CMD_AUTOREFRESH, SDRC_MANUAL_1);

	/* set mr0 */
	__raw_writel(SDP_SDRC_MR_0_DDR, SDRC_MR_0);
	if(MemID == ONENAND_NAND04GR4E_DEV_ID)
        __raw_writel(SDP_SDRC_MR_0_DDR, SDRC_MR_1);

	/* set up dll */
	__raw_writel(SDP_SDRC_DLLAB_CTRL, SDRC_DLLA_CTRL);
	delay(0x2000);	/* give time to lock */
}

/*********************************************************************
 * config_onenand_nand0xgr4wxa() - 4-Gbit DDP or 2-Gbit OneNAND Flash
 *********************************************************************/
void config_onenand_nand0xgr4wxa(void)
{
	/* global settings */
	__raw_writel(0x10, GPMC_SYSCONFIG);	/* smart idle */
	__raw_writel(0x0, GPMC_IRQENABLE);	/* isr's sources masked */
	__raw_writel(0, GPMC_TIMEOUT_CONTROL);/* timeout disable */

	/* Set the GPMC Vals, NAND is mapped at CS0, oneNAND at CS0.
	 *  We configure only GPMC CS0 with required values. Configuring other devices
	 *  at other CS is done in u-boot. So we don't have to bother doing it here.
	 */
	__raw_writel(0 , GPMC_CONFIG7 + GPMC_CONFIG_CS0);
	delay(1000);

	__raw_writel(ONENAND_GPMC_CONFIG1, GPMC_CONFIG1 + GPMC_CONFIG_CS0);
	__raw_writel(ONENAND_GPMC_CONFIG2, GPMC_CONFIG2 + GPMC_CONFIG_CS0);
	__raw_writel(ONENAND_GPMC_CONFIG3, GPMC_CONFIG3 + GPMC_CONFIG_CS0);
	__raw_writel(ONENAND_GPMC_CONFIG4, GPMC_CONFIG4 + GPMC_CONFIG_CS0);
	__raw_writel(ONENAND_GPMC_CONFIG5, GPMC_CONFIG5 + GPMC_CONFIG_CS0);
	__raw_writel(ONENAND_GPMC_CONFIG6, GPMC_CONFIG6 + GPMC_CONFIG_CS0);

	/* Enable the GPMC Mapping */
	__raw_writel((((OMAP34XX_GPMC_CS0_SIZE & 0xF)<<8) |
		     ((ONENAND_BASE>>24) & 0x3F) |
		     (1<<6)),  (GPMC_CONFIG7 + GPMC_CONFIG_CS0));
	delay(2000);
}

/**********************************************************
 * Routine:
 * Description: Configure MultiChip Package (MCP)
 *  -------------- -------------- ------------
 * | MCP PART. N. |   ONENAND    |  LPSDRAM   |
 * | M39B0RB0A0N1 | NAND02GR4E0A | M65KD001AM |
 * | M39B0RB0A0N1 | NAND02GR4E0A | M65KD001AM |
 *  -------------- -------------- ------------
 **********************************************************/
void config_multichip_package()
{
	// Configure OneNand memory
	config_onenand_nand0xgr4wxa();

	// Configure LPDDR
	config_sdram_m65kx002am(ONENAND_DEVICE_ID());
}

/**********************************************************
 * Routine: s_init
 * Description: Does early system init of muxing and clocks.
 * - Called at time when only stack is available.
 **********************************************************/
int s_init(void)
{
	u32 mem_type;

	watchdog_init();
	try_unlock_memory();
	set_muxconf_regs();
	delay(100);
	prcm_init();
	per_clocks_enable();
	gpmc_init ();

	/* Memory Configuration */
	mem_type = get_mem_type();

	if (mem_type == GPMC_ONENAND){
		config_multichip_package();
	} else if (mem_type == GPMC_NAND) {
		/*
			NOTE: The RAM has to be initialized since some MTD
		    data structures don't fit in the 64 KB memory and
		    are stored in RAM.
		    The problem is that each flash memory have different
		    minimum timings constraints. So we have to use a
		    default RAM configuration until we can get the correct
		    NAND manufacturer id and reconfigure the RAM accordingly.
		    Since the times are a minimum threshold, we use the slower
		    memory configuration by default.
		*/
		config_nand_flash();
	} else
        hang();
    // Initialize GP Timer
    timer_init();
	return 0;
}

/*****************************************
 * Routine: board_init
 * Description: Early hardware init.
 *****************************************/
int board_init(void)
{
    unsigned char data;

    if(is_cpu_family() == CPU_OMAP36XX){
        // Init TPS65950 - Voltage Selection (1.35V)
        // Calculation using this formula:
        // VSel = Vout - 600 / 12.5 ( all values in mili-Volts)
        // VSel = 1350 - 600 / 12.5 = 60 -> Hex = 0x3C
        data = 0x3C;
        // 1.4 V
        // --> data = 0x40;
        i2c_write(0x4B, 0xb9, 1, &data, 1);

        // twl4030_pmrecv_vsel_cfg();

    }
    else {
        if (get_prod_id() == CPU_35XX_720MHZ_DEV){
            // Init TPS65950 - Voltage Selection (1.35V)
            data = 0x3C;
            i2c_write(0x4B, 0xb9, 1, &data, 1);
        }
    }
    // Setup gpmc <-> Ethernet
    setup_net_chip(is_cpu_family());
    if(get_mem_type() == GPMC_NAND){
		read_nand_manufacturer_id(&mfr, &mid);
		if(mfr == NAND_MICRON_ID){
			config_sdram_mt29cxgxxmaxx();
		} else {
			config_sdram_hynix();
		}
		nand_command(NAND_CMD_RESET);
	}
#ifndef __DEBUG_MEMORY_TEST
    // Setup Malloc memory
    mem_malloc_init(XLOADER_MALLOC_IPTR, XLOADER_MALLOC_SIZE);
    __malloc_initialized = 1;
#else
// #define __DEBUG_MEMORY_TEST
	// Do Memory stress
	// Address start 0x80000000 to 0x90000000
	u32 pattern[] = {
		0x10101010,
		0x01010101,
		0xF0F0F0F0,
		0x0F0F0F0F,
		0XFFFFFFFF,
		0x00000000,
	};
	u32 *init_memory = (u32*) 0x80000000;
	u32 *sdie_init_memory = (u32*) 0x90000000;
	u32 i = 0;
	u32 j = 0;

	printf("Memory Init TEST (1)\n");
#ifdef __notdef
	while(pattern [j] != 0){
		for(i=0; i < (256 * 1024 * 1024)/4; i++){
			init_memory[i] = pattern[j];
			if(init_memory[i] != pattern[j]){
				printf("Write first stage Memory Error\n");
			}
		}
		for(i=0; i < (256 * 1024 * 1024)/4; i++){
			if(init_memory[i] != pattern[j]){
				printf("Write second stage Memory Error\n");
			}
		}
		printf("loop %d complete\n", j);
		j++;
	}
#endif
	printf("Memory END TEST (1)\n");
	printf("Memory Init TEST (2)\n");
	j = 0;
    while(pattern [j] != 0){
        printf("stage memset\n");
        memset(init_memory, pattern[j], (256 * 1024 * 1024));
        // memcpy(init_memory, pattern[j], (256 * 1024 * 1024));
        printf("End memset stage\n");
        printf("Start Verify stage (1)\n");
        for(i=0; i < (256 * 1024 * 1024)/4; i++){
			if(init_memory[i] != pattern[j]){
				printf("Verify (memset) Memory Error at (%x)\n", i);
			}
        }
        printf("End Verify stage (1)\n");
        printf("Start memcpy stage (1)\n");
        memcpy(sdie_init_memory, init_memory, (256 * 1024 * 1024));
        for(i=0; i < (256 * 1024 * 1024)/4; i++){
			if(init_memory[i] != sdie_init_memory[j]){
				printf("Verify (memcpy) Memory Error at (%x)\n", i);
			}
        }
        printf("End memcpy Verify stage\n");
        printf("loop %d complete\n", j);
        j++;
    }
	printf("Memory END TEST (2)\n");
    // Setup Malloc memory
    mem_malloc_init(XLOADER_MALLOC_IPTR, XLOADER_MALLOC_SIZE);
    __malloc_initialized = 1;
#endif
	return 0;
}

#define OMAP3730_MAX_RELEASES   4

static char *rev_s[OMAP3730_MAX_RELEASES] = {
  "1.0", /* 0 */
  "1.1", /* 1 */
  "1.2", /* 2 */
  "Unknown",
};

/*******************************************************
 * Routine: misc_init_r
 * Description: Init ethernet (done here so udelay works)
 ********************************************************/
int misc_init_r(void)
{
    u32 cpu_status;
    u16 cpu_id;
    char prod_id[16];
    u32 cpu_release;
    u8 rev;
    u32 mem_type;

    // Turn ON USER0 led
	omap_request_gpio(GPIO_LED_USER0);
	omap_set_gpio_direction(GPIO_LED_USER0, 0);
	omap_set_gpio_dataout(GPIO_LED_USER0, 1);
    // Turn Off USER1 led
	omap_request_gpio(GPIO_LED_USER1);
	omap_set_gpio_direction(GPIO_LED_USER1, 0);
	omap_set_gpio_dataout(GPIO_LED_USER1, 0);
    // Turn On USER2 led
	omap_request_gpio(GPIO_LED_USER2);
	omap_set_gpio_direction(GPIO_LED_USER2, 0);
	omap_set_gpio_dataout(GPIO_LED_USER2, 1);

	// Print Configuration Setup
	if(is_cpu_family() == CPU_OMAP36XX){

        cpu_release = __raw_readl(OMAP34XX_CONTROL_ID);
        rev = (cpu_release >> 28) & 0xff;
#ifdef __DEBUG__
        printf("CPU Release: 0x%x - 0x%x\n", cpu_release, rev);
#endif
        if(rev >= OMAP3730_MAX_RELEASES) rev = OMAP3730_MAX_RELEASES -1;
        cpu_id = __raw_readl(OMAP3XXX_STATUS_ID);
#ifdef __DEBUG__
	    printf("XLoader: CPU status = 0x%x\n", cpu_id);
#endif
	    switch(cpu_id){
            case 0x0c00:
            case 0x0e00: printf("XLoader: Processor DM3730 - ES%s\n", rev_s[rev]); break;
            case 0x5c00:
            case 0x5e00: printf("XLoader: Processor AM3703 - ES%s\n", rev_s[rev]); break;
	    }
    }
    else printf("XLoader: Processor OMAP3530\n");

	// Show Memory Manufacturer
	mem_type = get_mem_type();
	if(mem_type == GPMC_ONENAND)
		printf("XLoader: Memory Manufacturer: %s\n", "Numonyx");
	else
		printf("XLoader: Memory Manufacturer: %s (%x)\n", (mfr == NAND_MICRON_ID) ? "Micron" : "Hynix", mfr);
	return 0;
}

/******************************************************
 * Routine: wait_for_command_complete
 * Description: Wait for posting to finish on watchdog
 ******************************************************/
void wait_for_command_complete(unsigned int wd_base)
{
	int pending = 1;
	do {
		pending = __raw_readl(wd_base + WWPS);
	} while (pending);
}

/****************************************
 * Routine: watchdog_init
 * Description: Shut down watch dogs
 *****************************************/
void watchdog_init(void)
{
	/* There are 3 watch dogs WD1=Secure, WD2=MPU, WD3=IVA. WD1 is
	 * either taken care of by ROM (HS/EMU) or not accessible (GP).
	 * We need to take care of WD2-MPU or take a PRCM reset.  WD3
	 * should not be running and does not generate a PRCM reset.
	 */
	sr32(CM_FCLKEN_WKUP, 5, 1, 1);
	sr32(CM_ICLKEN_WKUP, 5, 1, 1);
	wait_on_value(BIT5, 0x20, CM_IDLEST_WKUP, 5);	/* some issue here */

#ifdef CONFIG_WATCHDOG
	/* Enable WD2 watchdog */
	__raw_writel(WD_UNLOCK3, WD2_BASE + WSPR);
	wait_for_command_complete(WD2_BASE);
	__raw_writel(WD_UNLOCK4, WD2_BASE + WSPR);
#else
	/* Disable WD2 watchdog */
	__raw_writel(WD_UNLOCK1, WD2_BASE + WSPR);
	wait_for_command_complete(WD2_BASE);
	__raw_writel(WD_UNLOCK2, WD2_BASE + WSPR);
#endif
}

/**********************************************
 * Routine: dram_init
 * Description: sets uboots idea of sdram size
 **********************************************/
int dram_init(void)
{
	return 0;
}

/*****************************************************************
 * Routine: peripheral_enable
 * Description: Enable the clks & power for perifs (GPT2, UART1,...)
 ******************************************************************/
void per_clocks_enable(void)
{
	/* Enable GP2 timer. */
	sr32(CM_CLKSEL_PER, 0, 1, 0x1);	/* GPT2 = sys clk */
	sr32(CM_ICLKEN_PER, 3, 1, 0x1);	/* ICKen GPT2 */
	sr32(CM_FCLKEN_PER, 3, 1, 0x1);	/* FCKen GPT2 */

#ifdef CFG_NS16550
	/* UART1 clocks */
	sr32(CM_FCLKEN1_CORE, 13, 1, 0x1);
	sr32(CM_ICLKEN1_CORE, 13, 1, 0x1);

	/* UART 3 Clocks */
	sr32(CM_FCLKEN_PER, 11, 1, 0x1);
	sr32(CM_ICLKEN_PER, 11, 1, 0x1);

#endif

#ifdef CONFIG_DRIVER_OMAP34XX_I2C
	/* Turn on all 3 I2C clocks */
	sr32(CM_FCLKEN1_CORE, 15, 3, 0x7);
	sr32(CM_ICLKEN1_CORE, 15, 3, 0x7);	/* I2C1,2,3 = on */
#endif

    /* Enable MMC1 clocks */
    // sr32(CM_FCLKEN1_CORE, 24, 1, 0x1);
    // sr32(CM_ICLKEN1_CORE, 24, 1, 0x1);

	/* Enable the ICLK for 32K Sync Timer as its used in udelay */
	sr32(CM_ICLKEN_WKUP, 2, 1, 0x1);

	sr32(CM_FCLKEN_IVA2, 0, 32, FCK_IVA2_ON);
	sr32(CM_FCLKEN1_CORE, 0, 32, FCK_CORE1_ON);
	sr32(CM_ICLKEN1_CORE, 0, 32, ICK_CORE1_ON);
	sr32(CM_ICLKEN2_CORE, 0, 32, ICK_CORE2_ON);
	sr32(CM_FCLKEN_WKUP, 0, 32, FCK_WKUP_ON);
	sr32(CM_ICLKEN_WKUP, 0, 32, ICK_WKUP_ON);
	sr32(CM_FCLKEN_DSS, 0, 32, FCK_DSS_ON);
	sr32(CM_ICLKEN_DSS, 0, 32, ICK_DSS_ON);
	sr32(CM_FCLKEN_CAM, 0, 32, FCK_CAM_ON);
	sr32(CM_ICLKEN_CAM, 0, 32, ICK_CAM_ON);
	sr32(CM_FCLKEN_PER, 0, 32, FCK_PER_ON);
	sr32(CM_ICLKEN_PER, 0, 32, ICK_PER_ON);

	delay(1000);
}


/* Set MUX for UART, GPMC, SDRC, GPIO */
#define 	MUX_VAL(OFFSET,VALUE)\
		__raw_writew((VALUE), OMAP34XX_CTRL_BASE + (OFFSET));

#define		CP(x)	(CONTROL_PADCONF_##x)

#define MUX_IGEP0020() \
 MUX_VAL(CP(SDRC_D0),		(IEN  | PTD | DIS | M0)) /* SDRC_D0  */\
 MUX_VAL(CP(SDRC_D1),		(IEN  | PTD | DIS | M0)) /* SDRC_D1  */\
 MUX_VAL(CP(SDRC_D2),		(IEN  | PTD | DIS | M0)) /* SDRC_D2  */\
 MUX_VAL(CP(SDRC_D3),		(IEN  | PTD | DIS | M0)) /* SDRC_D3  */\
 MUX_VAL(CP(SDRC_D4),		(IEN  | PTD | DIS | M0)) /* SDRC_D4  */\
 MUX_VAL(CP(SDRC_D5),		(IEN  | PTD | DIS | M0)) /* SDRC_D5  */\
 MUX_VAL(CP(SDRC_D6),		(IEN  | PTD | DIS | M0)) /* SDRC_D6  */\
 MUX_VAL(CP(SDRC_D7),		(IEN  | PTD | DIS | M0)) /* SDRC_D7  */\
 MUX_VAL(CP(SDRC_D8),		(IEN  | PTD | DIS | M0)) /* SDRC_D8  */\
 MUX_VAL(CP(SDRC_D9),		(IEN  | PTD | DIS | M0)) /* SDRC_D9  */\
 MUX_VAL(CP(SDRC_D10),		(IEN  | PTD | DIS | M0)) /* SDRC_D10 */\
 MUX_VAL(CP(SDRC_D11),		(IEN  | PTD | DIS | M0)) /* SDRC_D11 */\
 MUX_VAL(CP(SDRC_D12),		(IEN  | PTD | DIS | M0)) /* SDRC_D12 */\
 MUX_VAL(CP(SDRC_D13),		(IEN  | PTD | DIS | M0)) /* SDRC_D13 */\
 MUX_VAL(CP(SDRC_D14),		(IEN  | PTD | DIS | M0)) /* SDRC_D14 */\
 MUX_VAL(CP(SDRC_D15),		(IEN  | PTD | DIS | M0)) /* SDRC_D15 */\
 MUX_VAL(CP(SDRC_D16),		(IEN  | PTD | DIS | M0)) /* SDRC_D16 */\
 MUX_VAL(CP(SDRC_D17),		(IEN  | PTD | DIS | M0)) /* SDRC_D17 */\
 MUX_VAL(CP(SDRC_D18),		(IEN  | PTD | DIS | M0)) /* SDRC_D18 */\
 MUX_VAL(CP(SDRC_D19),		(IEN  | PTD | DIS | M0)) /* SDRC_D19 */\
 MUX_VAL(CP(SDRC_D20),		(IEN  | PTD | DIS | M0)) /* SDRC_D20 */\
 MUX_VAL(CP(SDRC_D21),		(IEN  | PTD | DIS | M0)) /* SDRC_D21 */\
 MUX_VAL(CP(SDRC_D22),		(IEN  | PTD | DIS | M0)) /* SDRC_D22 */\
 MUX_VAL(CP(SDRC_D23),		(IEN  | PTD | DIS | M0)) /* SDRC_D23 */\
 MUX_VAL(CP(SDRC_D24),		(IEN  | PTD | DIS | M0)) /* SDRC_D24 */\
 MUX_VAL(CP(SDRC_D25),		(IEN  | PTD | DIS | M0)) /* SDRC_D25 */\
 MUX_VAL(CP(SDRC_D26),		(IEN  | PTD | DIS | M0)) /* SDRC_D26 */\
 MUX_VAL(CP(SDRC_D27),		(IEN  | PTD | DIS | M0)) /* SDRC_D27 */\
 MUX_VAL(CP(SDRC_D28),		(IEN  | PTD | DIS | M0)) /* SDRC_D28 */\
 MUX_VAL(CP(SDRC_D29),		(IEN  | PTD | DIS | M0)) /* SDRC_D29 */\
 MUX_VAL(CP(SDRC_D30),		(IEN  | PTD | DIS | M0)) /* SDRC_D30 */\
 MUX_VAL(CP(SDRC_D31),		(IEN  | PTD | DIS | M0)) /* SDRC_D31 */\
 MUX_VAL(CP(SDRC_CLK),		(IEN  | PTD | DIS | M0)) /* SDRC_CLK */\
 MUX_VAL(CP(SDRC_DQS0),		(IEN  | PTD | DIS | M0)) /* SDRC_DQS0*/\
 MUX_VAL(CP(SDRC_DQS1),		(IEN  | PTD | DIS | M0)) /* SDRC_DQS1*/\
 MUX_VAL(CP(SDRC_DQS2),		(IEN  | PTD | DIS | M0)) /* SDRC_DQS2*/\
 MUX_VAL(CP(SDRC_DQS3),		(IEN  | PTD | DIS | M0)) /* SDRC_DQS3*/\
 /* GPMC - General-Purpose Memory Controller */\
 MUX_VAL(CP(GPMC_A1),		(IDIS | PTU | EN  | M0)) /* GPMC_A1       */\
 MUX_VAL(CP(GPMC_A2),		(IDIS | PTU | EN  | M0)) /* GPMC_A2       */\
 MUX_VAL(CP(GPMC_A3),		(IDIS | PTU | EN  | M0)) /* GPMC_A3       */\
 MUX_VAL(CP(GPMC_A4),		(IDIS | PTU | EN  | M0)) /* GPMC_A4       */\
 MUX_VAL(CP(GPMC_A5),		(IDIS | PTU | EN  | M0)) /* GPMC_A5       */\
 MUX_VAL(CP(GPMC_A6),		(IDIS | PTU | EN  | M0)) /* GPMC_A6       */\
 MUX_VAL(CP(GPMC_A7),		(IDIS | PTU | EN  | M0)) /* GPMC_A7       */\
 MUX_VAL(CP(GPMC_A8),		(IDIS | PTU | EN  | M0)) /* GPMC_A8       */\
 MUX_VAL(CP(GPMC_A9),		(IDIS | PTU | EN  | M0)) /* GPMC_A9       */\
 MUX_VAL(CP(GPMC_A10),		(IDIS | PTU | EN  | M0)) /* GPMC_A10      */\
 MUX_VAL(CP(GPMC_D0),		(IEN  | PTU | EN  | M0)) /* GPMC_D0       */\
 MUX_VAL(CP(GPMC_D1),		(IEN  | PTU | EN  | M0)) /* GPMC_D1       */\
 MUX_VAL(CP(GPMC_D2),		(IEN  | PTU | EN  | M0)) /* GPMC_D2       */\
 MUX_VAL(CP(GPMC_D3),		(IEN  | PTU | EN  | M0)) /* GPMC_D3       */\
 MUX_VAL(CP(GPMC_D4),		(IEN  | PTU | EN  | M0)) /* GPMC_D4       */\
 MUX_VAL(CP(GPMC_D5),		(IEN  | PTU | EN  | M0)) /* GPMC_D5       */\
 MUX_VAL(CP(GPMC_D6),		(IEN  | PTU | EN  | M0)) /* GPMC_D6       */\
 MUX_VAL(CP(GPMC_D7),		(IEN  | PTU | EN  | M0)) /* GPMC_D7       */\
 MUX_VAL(CP(GPMC_D8),		(IEN  | PTU | EN  | M0)) /* GPMC_D8       */\
 MUX_VAL(CP(GPMC_D9),		(IEN  | PTU | EN  | M0)) /* GPMC_D9       */\
 MUX_VAL(CP(GPMC_D10),		(IEN  | PTU | EN  | M0)) /* GPMC_D10      */\
 MUX_VAL(CP(GPMC_D11),		(IEN  | PTU | EN  | M0)) /* GPMC_D11      */\
 MUX_VAL(CP(GPMC_D12),		(IEN  | PTU | EN  | M0)) /* GPMC_D12      */\
 MUX_VAL(CP(GPMC_D13),		(IEN  | PTU | EN  | M0)) /* GPMC_D13      */\
 MUX_VAL(CP(GPMC_D14),		(IEN  | PTU | EN  | M0)) /* GPMC_D14      */\
 MUX_VAL(CP(GPMC_D15),		(IEN  | PTU | EN  | M0)) /* GPMC_D15      */\
 MUX_VAL(CP(GPMC_nCS0),		(IDIS | PTU | EN  | M0)) /* GPMC_nCS0     */\
 MUX_VAL(CP(GPMC_nCS1),		(IDIS | PTU | EN  | M0)) /* GPMC_nCS1     */\
 MUX_VAL(CP(GPMC_nCS2),		(IDIS | PTU | EN  | M0)) /* GPMC_nCS2     */\
 MUX_VAL(CP(GPMC_nCS3),		(IDIS | PTU | EN  | M0)) /* GPMC_nCS3     */\
 MUX_VAL(CP(GPMC_nCS4),		(IDIS | PTU | EN  | M0)) /* GPMC_nCS4     */\
 MUX_VAL(CP(GPMC_nCS5),		(IDIS | PTU | EN  | M0)) /* GPMC_nCS5     */\
 MUX_VAL(CP(GPMC_nCS6),		(IDIS | PTU | EN  | M0)) /* GPMC_nCS6     */\
 MUX_VAL(CP(GPMC_nOE),		(IDIS | PTD | DIS | M0)) /* GPMC_nOE      */\
 MUX_VAL(CP(GPMC_nWE),		(IDIS | PTD | DIS | M0)) /* GPMC_nWE      */\
 \
 MUX_VAL(CP(GPMC_WAIT2),	(IEN  | PTU | EN  | M4)) /* GPIO_64 -ETH_NRESET */\
 \
 MUX_VAL(CP(GPMC_nCS7),		(IEN  | PTU | EN  | M1)) /* SYS_nDMA_REQ3 */\
 MUX_VAL(CP(GPMC_CLK),		(IDIS | PTD | DIS | M0)) /* GPMC_CLK      */\
 MUX_VAL(CP(GPMC_nBE1),		(IEN  | PTD | DIS | M0)) /* GPMC_nBE1     */\
 MUX_VAL(CP(GPMC_nADV_ALE),	(IDIS | PTD | DIS | M0)) /* GPMC_nADV_ALE */\
 MUX_VAL(CP(GPMC_nBE0_CLE),	(IDIS | PTD | DIS | M0)) /* GPMC_nBE0_CLE */\
 MUX_VAL(CP(GPMC_nWP),		(IEN  | PTD | DIS | M0)) /* GPMC_nWP      */\
 MUX_VAL(CP(GPMC_WAIT0),	(IEN  | PTU | EN  | M0)) /* GPMC_WAIT0    */\
 MUX_VAL(CP(GPMC_WAIT1),	(IEN  | PTU | EN  | M0)) /* GPMC_WAIT1    */\
 MUX_VAL(CP(GPMC_WAIT3),	(IEN  | PTU | EN  | M0)) /* GPMC_WAIT3    */\
 /* DSS */\
 MUX_VAL(CP(DSS_PCLK),		(IDIS | PTD | DIS | M0)) /* DSS_PCLK   */\
 MUX_VAL(CP(DSS_HSYNC),		(IDIS | PTD | DIS | M0)) /* DSS_HSYNC  */\
 MUX_VAL(CP(DSS_VSYNC),		(IDIS | PTD | DIS | M0)) /* DSS_VSYNC  */\
 MUX_VAL(CP(DSS_ACBIAS),	(IDIS | PTD | DIS | M0)) /* DSS_ACBIAS */\
 MUX_VAL(CP(DSS_DATA0),		(IDIS | PTD | DIS | M0)) /* DSS_DATA0  */\
 MUX_VAL(CP(DSS_DATA1),		(IDIS | PTD | DIS | M0)) /* DSS_DATA1  */\
 MUX_VAL(CP(DSS_DATA2),		(IDIS | PTD | DIS | M0)) /* DSS_DATA2  */\
 MUX_VAL(CP(DSS_DATA3),		(IDIS | PTD | DIS | M0)) /* DSS_DATA3  */\
 MUX_VAL(CP(DSS_DATA4),		(IDIS | PTD | DIS | M0)) /* DSS_DATA4  */\
 MUX_VAL(CP(DSS_DATA5),		(IDIS | PTD | DIS | M0)) /* DSS_DATA5  */\
 MUX_VAL(CP(DSS_DATA6),		(IDIS | PTD | DIS | M0)) /* DSS_DATA6  */\
 MUX_VAL(CP(DSS_DATA7),		(IDIS | PTD | DIS | M0)) /* DSS_DATA7  */\
 MUX_VAL(CP(DSS_DATA8),		(IDIS | PTD | DIS | M0)) /* DSS_DATA8  */\
 MUX_VAL(CP(DSS_DATA9),		(IDIS | PTD | DIS | M0)) /* DSS_DATA9  */\
 MUX_VAL(CP(DSS_DATA10),	(IDIS | PTD | DIS | M0)) /* DSS_DATA10 */\
 MUX_VAL(CP(DSS_DATA11),	(IDIS | PTD | DIS | M0)) /* DSS_DATA11 */\
 MUX_VAL(CP(DSS_DATA12),	(IDIS | PTD | DIS | M0)) /* DSS_DATA12 */\
 MUX_VAL(CP(DSS_DATA13),	(IDIS | PTD | DIS | M0)) /* DSS_DATA13 */\
 MUX_VAL(CP(DSS_DATA14),	(IDIS | PTD | DIS | M0)) /* DSS_DATA14 */\
 MUX_VAL(CP(DSS_DATA15),	(IDIS | PTD | DIS | M0)) /* DSS_DATA15 */\
 MUX_VAL(CP(DSS_DATA16),	(IDIS | PTD | DIS | M0)) /* DSS_DATA16 */\
 MUX_VAL(CP(DSS_DATA17),	(IDIS | PTD | DIS | M0)) /* DSS_DATA17 */\
 MUX_VAL(CP(DSS_DATA18),	(IDIS | PTD | DIS | M0)) /* DSS_DATA18 */\
 MUX_VAL(CP(DSS_DATA19),	(IDIS | PTD | DIS | M0)) /* DSS_DATA19 */\
 MUX_VAL(CP(DSS_DATA20),	(IDIS | PTD | DIS | M0)) /* DSS_DATA20 */\
 MUX_VAL(CP(DSS_DATA21),	(IDIS | PTD | DIS | M0)) /* DSS_DATA21 */\
 MUX_VAL(CP(DSS_DATA22),	(IDIS | PTD | DIS | M0)) /* DSS_DATA22 */\
 MUX_VAL(CP(DSS_DATA23),	(IDIS | PTD | DIS | M0)) /* DSS_DATA23 */\
 /* Audio Interface */\
 MUX_VAL(CP(McBSP2_FSX),	(IEN  | PTD | DIS | M0)) /* McBSP2_FSX  */\
 MUX_VAL(CP(McBSP2_CLKX),	(IEN  | PTD | DIS | M0)) /* McBSP2_CLKX */\
 MUX_VAL(CP(McBSP2_DR),		(IEN  | PTD | DIS | M0)) /* McBSP2_DR   */\
 MUX_VAL(CP(McBSP2_DX),		(IDIS | PTD | DIS | M0)) /* McBSP2_DX   */\
 /* Expansion card 1 */\
 MUX_VAL(CP(MMC1_CLK),		(IDIS | PTU | EN  | M0)) /* MMC1_CLK  */\
 MUX_VAL(CP(MMC1_CMD),		(IEN  | PTU | EN  | M0)) /* MMC1_CMD  */\
 MUX_VAL(CP(MMC1_DAT0),		(IEN  | PTU | EN  | M0)) /* MMC1_DAT0 */\
 MUX_VAL(CP(MMC1_DAT1),		(IEN  | PTU | EN  | M0)) /* MMC1_DAT1 */\
 MUX_VAL(CP(MMC1_DAT2),		(IEN  | PTU | EN  | M0)) /* MMC1_DAT2 */\
 MUX_VAL(CP(MMC1_DAT3),		(IEN  | PTU | EN  | M0)) /* MMC1_DAT3 */\
 \
 MUX_VAL(CP(MMC1_DAT4),		(IEN  | PTU | EN  | M0)) /* MMC1_DAT4 */\
 MUX_VAL(CP(MMC1_DAT5),		(IEN  | PTU | EN  | M0)) /* MMC1_DAT5 */\
 MUX_VAL(CP(MMC1_DAT6),		(IEN  | PTU | EN  | M0)) /* MMC1_DAT6 */\
 MUX_VAL(CP(MMC1_DAT7),		(IEN  | PTU | EN  | M0)) /* MMC1_DAT7 */\
 /* SDIO Interface to WIFI Module (EXPANSION CONNECTOR) */\
 MUX_VAL(CP(MMC2_CLK),		(IEN  | PTU | EN  | M0)) /* MMC2_CLK  */\
 MUX_VAL(CP(MMC2_CMD),		(IEN  | PTU | EN  | M0)) /* MMC2_CMD  */\
 MUX_VAL(CP(MMC2_DAT0),		(IEN  | PTU | EN  | M0)) /* MMC2_DAT0 */\
 MUX_VAL(CP(MMC2_DAT1),		(IEN  | PTU | EN  | M0)) /* MMC2_DAT1 */\
 MUX_VAL(CP(MMC2_DAT2),		(IEN  | PTU | EN  | M0)) /* MMC2_DAT2 */\
 MUX_VAL(CP(MMC2_DAT3),		(IEN  | PTU | EN  | M0)) /* MMC2_DAT3 */\
 \
 /* GSPI Interface to WIFI Module */ \
/* MUX_VAL(CP(MMC2_CLK),		(IEN  | PTD | DIS | M1)) McSPI3_CLK  */\
/* MUX_VAL(CP(MMC2_CMD),		(IEN  | PTD | DIS | M1)) McSPI3_SIMO */\
/* MUX_VAL(CP(MMC2_DAT0),		(IEN  | PTD | DIS | M1)) McSPI3_SOMI */\
/* MUX_VAL(CP(MMC2_DAT1),		(IEN  | PTD | DIS | M4)) GPIO_133    */\
/* MUX_VAL(CP(MMC2_DAT2),		(IEN  | PTD | DIS | M4)) GPIO_134    */\
/* MUX_VAL(CP(MMC2_DAT3),		(IEN  | PTD | DIS | M4)) GPIO_135 (GPIO-Based CS) */\
 \
 MUX_VAL(CP(CAM_HS),		(IDIS | PTD | DIS | M4)) /* GPIO_94 - PDN (Rev. B) */\
 MUX_VAL(CP(CAM_VS),		(IDIS | PTD | DIS | M4)) /* GPIO_95 - RESET_N_W (Rev. B) */\
 \
 MUX_VAL(CP(MMC2_DAT4),		(IDIS | PTD | DIS | M4)) /* GPIO_136 */\
 MUX_VAL(CP(MMC2_DAT5),		(IDIS | PTD | DIS | M4)) /* GPIO_137 - RESET_N_B */\
 MUX_VAL(CP(MMC2_DAT6),		(IDIS | PTD | DIS | M4)) /* GPIO_138 - PDN (Rev. C)  */\
 MUX_VAL(CP(MMC2_DAT7),		(IDIS | PTD | DIS | M4)) /* GPIO_139 - RESET_N_W (Rev. C) */\
 /* Bluetooth (EXPANSION CONNECTOR) */\
 MUX_VAL(CP(McBSP3_DX),		(IDIS | PTD | DIS | M0)) /* McBSP3_DX */\
 MUX_VAL(CP(McBSP3_DR),		(IEN  | PTD | DIS | M0)) /* McBSP3_DR */\
 MUX_VAL(CP(McBSP3_CLKX),	(IEN  | PTD | DIS | M0)) /* McBSP3_CLKX  */\
 MUX_VAL(CP(McBSP3_FSX),	(IEN  | PTD | DIS | M0)) /* McBSP3_FSX   */\
 MUX_VAL(CP(UART2_CTS),		(IEN  | PTU | EN  | M0)) /* UART2_CTS */\
 MUX_VAL(CP(UART2_RTS),		(IDIS | PTD | DIS | M0)) /* UART2_RTS */\
 MUX_VAL(CP(UART2_TX),		(IDIS | PTD | DIS | M0)) /* UART2_TX  */\
 MUX_VAL(CP(UART2_RX),		(IEN  | PTD | DIS | M0)) /* UART2_RX  */\
 /* 485 Interface */\
 MUX_VAL(CP(UART1_TX),		(IDIS | PTD | DIS | M0)) /* UART1_TX  */\
 MUX_VAL(CP(UART1_RTS),		(IDIS | PTD | DIS | M0)) /* UART1_RTS */\
 MUX_VAL(CP(UART1_CTS),		(IEN  | PTU | DIS | M0)) /* UART1_CTS */\
 MUX_VAL(CP(UART1_RX),		(IEN  | PTD | DIS | M0)) /* UART1_RX  */\
 MUX_VAL(CP(McBSP4_CLKX),	(IDIS | PTD | DIS | M4)) /* GPIO_152  */\
 /* Serial Interface */\
 MUX_VAL(CP(UART3_CTS_RCTX),	(IEN  | PTD | EN  | M0)) /* UART3_CTS_RCTX*/\
 MUX_VAL(CP(UART3_RTS_SD),	(IDIS | PTD | DIS | M0)) /* UART3_RTS_SD */\
 MUX_VAL(CP(UART3_RX_IRRX),	(IEN  | PTD | DIS | M0)) /* UART3_RX_IRRX*/\
 MUX_VAL(CP(UART3_TX_IRTX),	(IDIS | PTD | DIS | M0)) /* UART3_TX_IRTX*/\
 MUX_VAL(CP(HSUSB0_CLK),	(IEN  | PTD | DIS | M0)) /* HSUSB0_CLK*/\
 MUX_VAL(CP(HSUSB0_STP),	(IDIS | PTU | EN  | M0)) /* HSUSB0_STP*/\
 MUX_VAL(CP(HSUSB0_DIR),	(IEN  | PTD | DIS | M0)) /* HSUSB0_DIR*/\
 MUX_VAL(CP(HSUSB0_NXT),	(IEN  | PTD | DIS | M0)) /* HSUSB0_NXT*/\
 MUX_VAL(CP(HSUSB0_DATA0),	(IEN  | PTD | DIS | M0)) /* HSUSB0_DATA0*/\
 MUX_VAL(CP(HSUSB0_DATA1),	(IEN  | PTD | DIS | M0)) /* HSUSB0_DATA1*/\
 MUX_VAL(CP(HSUSB0_DATA2),	(IEN  | PTD | DIS | M0)) /* HSUSB0_DATA2*/\
 MUX_VAL(CP(HSUSB0_DATA3),	(IEN  | PTD | DIS | M0)) /* HSUSB0_DATA3*/\
 MUX_VAL(CP(HSUSB0_DATA4),	(IEN  | PTD | DIS | M0)) /* HSUSB0_DATA4*/\
 MUX_VAL(CP(HSUSB0_DATA5),	(IEN  | PTD | DIS | M0)) /* HSUSB0_DATA5*/\
 MUX_VAL(CP(HSUSB0_DATA6),	(IEN  | PTD | DIS | M0)) /* HSUSB0_DATA6*/\
 MUX_VAL(CP(HSUSB0_DATA7),	(IEN  | PTD | DIS | M0)) /* HSUSB0_DATA7*/\
 MUX_VAL(CP(I2C1_SCL),		(IEN  | PTU | EN  | M0)) /* I2C1_SCL*/\
 MUX_VAL(CP(I2C1_SDA),		(IEN  | PTU | EN  | M0)) /* I2C1_SDA*/\
 MUX_VAL(CP(I2C2_SCL),		(IEN  | PTU | EN  | M0)) /* GPIO_168*/\
 MUX_VAL(CP(I2C2_SDA),		(IEN  | PTU | EN  | M0)) /* GPIO_183*/\
 MUX_VAL(CP(I2C3_SCL),		(IEN  | PTU | EN  | M0)) /* I2C3_SCL*/\
 MUX_VAL(CP(I2C3_SDA),		(IEN  | PTU | EN  | M0)) /* I2C3_SDA*/\
 MUX_VAL(CP(I2C4_SCL),		(IEN  | PTU | EN  | M0)) /* I2C4_SCL*/\
 MUX_VAL(CP(I2C4_SDA),		(IEN  | PTU | EN  | M0)) /* I2C4_SDA*/\
 MUX_VAL(CP(HDQ_SIO),		(IDIS | PTU | EN  | M4)) /* GPIO_170*/\
 /* SPI1 ADC121S101 */ \
 MUX_VAL(CP(McSPI1_CLK),	(IEN  | PTD | DIS | M0)) /* McSPI1_CLK  */\
 MUX_VAL(CP(McSPI1_SIMO),	(IEN  | PTD | DIS | M0)) /* McSPI1_SIMO */\
 MUX_VAL(CP(McSPI1_SOMI),	(IEN  | PTD | DIS | M0)) /* McSPI1_SOMI */\
 MUX_VAL(CP(McSPI1_CS3),	(IDIS | PTD | DIS | M0)) /* McSPI1_CS3  */\
 \
 MUX_VAL(CP(McSPI1_CS0),	(IDIS | PTD | DIS | M0)) /* McSPI1_CS0  */\
 MUX_VAL(CP(McSPI1_CS1),	(IEN  | PTD | DIS | M4)) /* GPIO_175   */\
 MUX_VAL(CP(McSPI1_CS2),	(IEN  | PTD | DIS | M4)) /* GPIO_176   */\
 /* SPI2 (25GHz RF PORT) */ \
 MUX_VAL(CP(McSPI2_CLK),	(IEN  | PTD | DIS | M0)) /* McSPI2_CLK  */\
 MUX_VAL(CP(McSPI2_SIMO),	(IEN  | PTD | DIS | M0)) /* McSPI2_SIMO */\
 MUX_VAL(CP(McSPI2_SOMI),	(IEN  | PTD | DIS | M0)) /* McSPI2_SOMI */\
 MUX_VAL(CP(McSPI2_CS0),	(IDIS | PTD | DIS | M0)) /* McSPI2_CS0  */\
 \
 MUX_VAL(CP(McSPI2_CS1),	(IEN  | PTD | DIS | M4)) /* GPIO_182    */\
 /* Control and debug */\
 MUX_VAL(CP(SYS_32K),		(IEN  | PTD | DIS | M0)) /* SYS_32K*/\
 MUX_VAL(CP(SYS_CLKREQ),	(IEN  | PTD | DIS | M0)) /* SYS_CLKREQ*/\
 MUX_VAL(CP(SYS_nIRQ),		(IEN  | PTU | EN  | M0)) /* SYS_nIRQ*/\
 MUX_VAL(CP(SYS_BOOT0),		(IEN  | PTD | DIS | M4)) /* GPIO_2*/\
 MUX_VAL(CP(SYS_BOOT1),		(IEN  | PTD | DIS | M4)) /* GPIO_3*/\
 MUX_VAL(CP(SYS_BOOT2),		(IEN  | PTD | DIS | M4)) /* GPIO_4 - MMC1_WP*/\
 MUX_VAL(CP(SYS_BOOT3),		(IEN  | PTD | DIS | M4)) /* GPIO_5*/\
 MUX_VAL(CP(SYS_BOOT4),		(IEN  | PTD | DIS | M4)) /* GPIO_6*/\
 MUX_VAL(CP(SYS_BOOT5),		(IEN  | PTD | DIS | M4)) /* GPIO_7*/\
 MUX_VAL(CP(SYS_BOOT6),		(IDIS | PTD | DIS | M4)) /* GPIO_8*/ \
 /* VIO_1V8 */\
 MUX_VAL(CP(SYS_OFF_MODE),	(IEN  | PTD | DIS | M0)) /* SYS_OFF_MODE*/\
 MUX_VAL(CP(SYS_CLKOUT1),	(IEN  | PTD | DIS | M0)) /* SYS_CLKOUT1*/\
 MUX_VAL(CP(SYS_CLKOUT2),	(IEN  | PTU | EN  | M4)) /* GPIO_186*/\
 /* Generic IO (outputs) */\
 MUX_VAL(CP(ETK_D10_ES2),	(IDIS | PTU | DIS | M4)) /* GPIO_24  - USB1HS_nRST */\
 MUX_VAL(CP(ETK_D12_ES2),	(IDIS | PTU | DIS | M4)) /* GPIO_26  - LED1        */\
 MUX_VAL(CP(ETK_D13_ES2),	(IDIS | PTU | DIS | M4)) /* GPIO_27  - LED0        */\
 MUX_VAL(CP(CAM_D6),		(IDIS | PTU | DIS | M4)) /* GPIO_105 - RF_CTRL     */\
 MUX_VAL(CP(CAM_D7),		(IDIS | PTU | DIS | M4)) /* GPIO_106 - RF_STANDBY  */\
 MUX_VAL(CP(CAM_D8),		(IDIS | PTU | DIS | M4)) /* GPIO_107 - RF_INT      */\
 MUX_VAL(CP(CAM_D9),		(IDIS | PTU | DIS | M4)) /* GPIO_108 - RF_SYNCB    */\
 /* Generic IO (inputs) */\
 MUX_VAL(CP(ETK_D11_ES2),	(IEN  | PTD | DIS | M4)) /* GPIO_25   */\
 MUX_VAL(CP(ETK_D14_ES2),	(IEN  | PTD | DIS | M4)) /* GPIO_28   */\
 MUX_VAL(CP(ETK_D15_ES2),	(IEN  | PTD | DIS | M4)) /* GPIO_29   */\
 MUX_VAL(CP(CAM_D0),		(IEN  | PTD | DIS | M4)) /* GPIO_99   */\
 MUX_VAL(CP(CAM_D1),		(IEN  | PTD | DIS | M4)) /* GPIO_100  */\
 MUX_VAL(CP(CSI2_DX0),		(IEN  | PTD | DIS | M4)) /* GPIO_112  */\
 MUX_VAL(CP(CSI2_DY0),		(IEN  | PTD | DIS | M4)) /* GPIO_113  */\
 MUX_VAL(CP(CSI2_DX1),		(IEN  | PTD | DIS | M4)) /* GPIO_114  */\
 MUX_VAL(CP(CSI2_DY1),		(IEN  | PTD | DIS | M4)) /* GPIO_115  */\
 \
 \
 /* LCD_INI */\
 MUX_VAL(CP(McBSP4_DR),		(IDIS | PTD | DIS | M4)) /* GPIO_153  */\
 /* LCD_ENVDD */\
 MUX_VAL(CP(McBSP4_DX),		(IDIS | PTD | DIS | M4)) /* GPIO_154 */\
 /* LCD_QVGA/nVGA */\
 MUX_VAL(CP(McBSP4_FSX),	(IDIS | PTD | DIS | M4)) /* GPIO_155 */\
 /* LCD_RESB */\
 MUX_VAL(CP(McBSP1_CLKR),	(IDIS | PTD | DIS | M4)) /* GPIO_156 */\
 MUX_VAL(CP(McBSP1_FSR),	(IDIS | PTU | EN  | M4)) /* GPIO_157 */\
 MUX_VAL(CP(McBSP1_DX),		(IDIS | PTD | DIS | M4)) /* GPIO_158 */\
 MUX_VAL(CP(McBSP1_DR),		(IDIS | PTD | DIS | M4)) /* GPIO_159 */\
 MUX_VAL(CP(McBSP_CLKS),	(IEN  | PTU | DIS | M0)) /* McBSP_CLKS */\
 MUX_VAL(CP(McBSP1_FSX),	(IDIS | PTD | DIS | M4)) /* GPIO_161 */\
 MUX_VAL(CP(McBSP1_CLKX),	(IDIS | PTD | DIS | M4)) /* GPIO_162 */\
 \
 /* CAMERA */\
 MUX_VAL(CP(CAM_XCLKA),		(IDIS | PTD | DIS | M0)) /* CAM_XCLKA */\
 MUX_VAL(CP(CAM_PCLK),		(IEN  | PTU | EN  | M0)) /* CAM_PCLK  */\
 MUX_VAL(CP(CAM_FLD),		(IDIS | PTD | DIS | M4)) /* GPIO_98   */\
 MUX_VAL(CP(CAM_D2),		(IEN  | PTD | DIS | M0)) /* CAM_D2    */\
 MUX_VAL(CP(CAM_D3),		(IEN  | PTD | DIS | M0)) /* CAM_D3    */\
 MUX_VAL(CP(CAM_D4),		(IEN  | PTD | DIS | M0)) /* CAM_D4    */\
 MUX_VAL(CP(CAM_D5),		(IEN  | PTD | DIS | M0)) /* CAM_D5    */\
 MUX_VAL(CP(CAM_D10),		(IEN  | PTD | DIS | M0)) /* CAM_D10   */\
 MUX_VAL(CP(CAM_D11),		(IEN  | PTD | DIS | M0)) /* CAM_D11   */\
 MUX_VAL(CP(CAM_XCLKB),		(IDIS | PTD | DIS | M0)) /* CAM_XCLKB */\
 MUX_VAL(CP(CAM_WEN),		(IEN  | PTD | DIS | M4)) /* GPIO_167  */\
 MUX_VAL(CP(CAM_STROBE),	(IDIS | PTD | DIS | M0)) /* CAM_STROBE*/\
 \
 MUX_VAL(CP(d2d_mcad1),		(IEN  | PTD | EN  | M0)) /*d2d_mcad1*/\
 MUX_VAL(CP(d2d_mcad2),		(IEN  | PTD | EN  | M0)) /*d2d_mcad2*/\
 MUX_VAL(CP(d2d_mcad3),		(IEN  | PTD | EN  | M0)) /*d2d_mcad3*/\
 MUX_VAL(CP(d2d_mcad4),		(IEN  | PTD | EN  | M0)) /*d2d_mcad4*/\
 MUX_VAL(CP(d2d_mcad5),		(IEN  | PTD | EN  | M0)) /*d2d_mcad5*/\
 MUX_VAL(CP(d2d_mcad6),		(IEN  | PTD | EN  | M0)) /*d2d_mcad6*/\
 MUX_VAL(CP(d2d_mcad7),		(IEN  | PTD | EN  | M0)) /*d2d_mcad7*/\
 MUX_VAL(CP(d2d_mcad8),		(IEN  | PTD | EN  | M0)) /*d2d_mcad8*/\
 MUX_VAL(CP(d2d_mcad9),		(IEN  | PTD | EN  | M0)) /*d2d_mcad9*/\
 MUX_VAL(CP(d2d_mcad10),	(IEN  | PTD | EN  | M0)) /*d2d_mcad10*/\
 MUX_VAL(CP(d2d_mcad11),	(IEN  | PTD | EN  | M0)) /*d2d_mcad11*/\
 MUX_VAL(CP(d2d_mcad12),	(IEN  | PTD | EN  | M0)) /*d2d_mcad12*/\
 MUX_VAL(CP(d2d_mcad13),	(IEN  | PTD | EN  | M0)) /*d2d_mcad13*/\
 MUX_VAL(CP(d2d_mcad14),	(IEN  | PTD | EN  | M0)) /*d2d_mcad14*/\
 MUX_VAL(CP(d2d_mcad15),	(IEN  | PTD | EN  | M0)) /*d2d_mcad15*/\
 MUX_VAL(CP(d2d_mcad16),	(IEN  | PTD | EN  | M0)) /*d2d_mcad16*/\
 MUX_VAL(CP(d2d_mcad17),	(IEN  | PTD | EN  | M0)) /*d2d_mcad17*/\
 MUX_VAL(CP(d2d_mcad18),	(IEN  | PTD | EN  | M0)) /*d2d_mcad18*/\
 MUX_VAL(CP(d2d_mcad19),	(IEN  | PTD | EN  | M0)) /*d2d_mcad19*/\
 MUX_VAL(CP(d2d_mcad20),	(IEN  | PTD | EN  | M0)) /*d2d_mcad20*/\
 MUX_VAL(CP(d2d_mcad21),	(IEN  | PTD | EN  | M0)) /*d2d_mcad21*/\
 MUX_VAL(CP(d2d_mcad22),	(IEN  | PTD | EN  | M0)) /*d2d_mcad22*/\
 MUX_VAL(CP(d2d_mcad23),	(IEN  | PTD | EN  | M0)) /*d2d_mcad23*/\
 MUX_VAL(CP(d2d_mcad24),	(IEN  | PTD | EN  | M0)) /*d2d_mcad24*/\
 MUX_VAL(CP(d2d_mcad25),	(IEN  | PTD | EN  | M0)) /*d2d_mcad25*/\
 MUX_VAL(CP(d2d_mcad26),	(IEN  | PTD | EN  | M0)) /*d2d_mcad26*/\
 MUX_VAL(CP(d2d_mcad27),	(IEN  | PTD | EN  | M0)) /*d2d_mcad27*/\
 MUX_VAL(CP(d2d_mcad28),	(IEN  | PTD | EN  | M0)) /*d2d_mcad28*/\
 MUX_VAL(CP(d2d_mcad29),	(IEN  | PTD | EN  | M0)) /*d2d_mcad29*/\
 MUX_VAL(CP(d2d_mcad30),	(IEN  | PTD | EN  | M0)) /*d2d_mcad30*/\
 MUX_VAL(CP(d2d_mcad31),	(IEN  | PTD | EN  | M0)) /*d2d_mcad31*/\
 MUX_VAL(CP(d2d_mcad32),	(IEN  | PTD | EN  | M0)) /*d2d_mcad32*/\
 MUX_VAL(CP(d2d_mcad33),	(IEN  | PTD | EN  | M0)) /*d2d_mcad33*/\
 MUX_VAL(CP(d2d_mcad34),	(IEN  | PTD | EN  | M0)) /*d2d_mcad34*/\
 MUX_VAL(CP(d2d_mcad35),	(IEN  | PTD | EN  | M0)) /*d2d_mcad35*/\
 MUX_VAL(CP(d2d_mcad36),	(IEN  | PTD | EN  | M0)) /*d2d_mcad36*/\
 MUX_VAL(CP(d2d_clk26mi),	(IEN  | PTD | DIS | M0)) /*d2d_clk26mi*/\
 MUX_VAL(CP(d2d_nrespwron),	(IEN  | PTD | EN  | M0)) /*d2d_nrespwron*/\
 MUX_VAL(CP(d2d_nreswarm),	(IEN  | PTU | EN  | M0)) /*d2d_nreswarm */\
 MUX_VAL(CP(d2d_arm9nirq),	(IEN  | PTD | DIS | M0)) /*d2d_arm9nirq */\
 MUX_VAL(CP(d2d_uma2p6fiq),	(IEN  | PTD | DIS | M0)) /*d2d_uma2p6fiq*/\
 MUX_VAL(CP(d2d_spint),		(IEN  | PTD | EN  | M0)) /*d2d_spint*/\
 MUX_VAL(CP(d2d_frint),		(IEN  | PTD | EN  | M0)) /*d2d_frint*/\
 MUX_VAL(CP(d2d_dmareq0),	(IEN  | PTD | DIS | M0)) /*d2d_dmareq0*/\
 MUX_VAL(CP(d2d_dmareq1),	(IEN  | PTD | DIS | M0)) /*d2d_dmareq1*/\
 MUX_VAL(CP(d2d_dmareq2),	(IEN  | PTD | DIS | M0)) /*d2d_dmareq2*/\
 MUX_VAL(CP(d2d_dmareq3),	(IEN  | PTD | DIS | M0)) /*d2d_dmareq3*/\
 MUX_VAL(CP(d2d_n3gtrst),	(IEN  | PTD | DIS | M0)) /*d2d_n3gtrst*/\
 MUX_VAL(CP(d2d_n3gtdi),	(IEN  | PTD | DIS | M0)) /*d2d_n3gtdi*/\
 MUX_VAL(CP(d2d_n3gtdo),	(IEN  | PTD | DIS | M0)) /*d2d_n3gtdo*/\
 MUX_VAL(CP(d2d_n3gtms),	(IEN  | PTD | DIS | M0)) /*d2d_n3gtms*/\
 MUX_VAL(CP(d2d_n3gtck),	(IEN  | PTD | DIS | M0)) /*d2d_n3gtck*/\
 MUX_VAL(CP(d2d_n3grtck),	(IEN  | PTD | DIS | M0)) /*d2d_n3grtck*/\
 MUX_VAL(CP(d2d_mstdby),	(IEN  | PTU | EN  | M0)) /*d2d_mstdby*/\
 MUX_VAL(CP(d2d_swakeup),	(IEN  | PTD | EN  | M0)) /*d2d_swakeup*/\
 MUX_VAL(CP(d2d_idlereq),	(IEN  | PTD | DIS | M0)) /*d2d_idlereq*/\
 MUX_VAL(CP(d2d_idleack),	(IEN  | PTU | EN  | M0)) /*d2d_idleack*/\
 MUX_VAL(CP(d2d_mwrite),	(IEN  | PTD | DIS | M0)) /*d2d_mwrite*/\
 MUX_VAL(CP(d2d_swrite),	(IEN  | PTD | DIS | M0)) /*d2d_swrite*/\
 MUX_VAL(CP(d2d_mread),		(IEN  | PTD | DIS | M0)) /*d2d_mread*/\
 MUX_VAL(CP(d2d_sread),		(IEN  | PTD | DIS | M0)) /*d2d_sread*/\
 MUX_VAL(CP(d2d_mbusflag),	(IEN  | PTD | DIS | M0)) /*d2d_mbusflag*/\
 MUX_VAL(CP(d2d_sbusflag),	(IEN  | PTD | DIS | M0)) /*d2d_sbusflag*/\
 MUX_VAL(CP(sdrc_cke0),		(IDIS | PTU | EN  | M0)) /*sdrc_cke0*/\
 MUX_VAL(CP(sdrc_cke1),		(IDIS | PTD | DIS | M7)) /*sdrc_cke1*/

/**********************************************************
 * Routine: set_muxconf_regs
 * Description: Setting up the configuration Mux registers
 *              specific to the hardware. Many pins need
 *              to be moved from protect to primary mode.
 *********************************************************/
void set_muxconf_regs(void)
{
	MUX_IGEP0020();
}

static inline u32 get_part_sector_size_onenand(void)
{
#if defined(CONFIG_CMD_ONENAND)
	struct mtd_info *mtd;

	mtd = mtd_info;

	return mtd->erasesize;
#else
	BUG();
	return 0;
#endif
}

static inline u32 get_part_sector_size_nand(void)
{
#if defined(CONFIG_CMD_NAND)
	struct mtd_info *mtd;

	mtd = mtd_info;

	return mtd->erasesize;
#else
	BUG();
	return 0;
#endif
}

static inline u32 get_part_sector_size(struct mtdids *id, struct part_info *part)
{
	if (id->type == MTD_DEV_TYPE_ONENAND)
		return get_part_sector_size_onenand();
	else if (id->type == MTD_DEV_TYPE_NAND)
		return get_part_sector_size_nand();
	else
		printf("Error: Unknown device type.\n");

	return 0;
}

static int mtd_device_validate(u8 type, u8 num, u32 *size)
{
	if (type == MTD_DEV_TYPE_NOR) {
#if defined(CONFIG_CMD_FLASH)
		if (num < CONFIG_SYS_MAX_FLASH_BANKS) {
			extern flash_info_t flash_info[];
			*size = flash_info[num].size;

			return 0;
		}

		printf("no such FLASH device: %s%d (valid range 0 ... %d\n",
				MTD_DEV_TYPE(type), num, CONFIG_SYS_MAX_FLASH_BANKS - 1);
#else
		printf("support for FLASH devices not present\n");
#endif
	} else if (type == MTD_DEV_TYPE_NAND) {
#if defined(CONFIG_CMD_NAND)
			*size = mtd_info->size;
			return 0;

		printf("no such NAND device: %s%d \n", MTD_DEV_TYPE(type), num);
#else
		printf("support for NAND devices not present\n");
		#endif
	} else if (type == MTD_DEV_TYPE_ONENAND) {
#if defined(CONFIG_CMD_ONENAND)
		*size = mtd_info->size;
		return 0;
#else
		printf("support for OneNAND devices not present\n");
#endif
	} else
		printf("Unknown device type %d\n", type);

	return 1;
}


/**
 * Parse device id string <dev-id> := 'nand'|'nor'|'onenand'<dev-num>,
 * return device type and number.
 *
 * @param id string describing device id
 * @param ret_id output pointer to next char after parse completes (output)
 * @param dev_type parsed device type (output)
 * @param dev_num parsed device number (output)
 * @return 0 on success, 1 otherwise
 */
static int mtd_id_parse(const char *id, const char **ret_id, u8 *dev_type, u8 *dev_num)
{
	const char *p = id;

	*dev_type = 0;
	if (strncmp(p, "nand", 4) == 0) {
		*dev_type = MTD_DEV_TYPE_NAND;
		p += 4;
	} else if (strncmp(p, "nor", 3) == 0) {
		*dev_type = MTD_DEV_TYPE_NOR;
		p += 3;
	} else if (strncmp(p, "onenand", 7) == 0) {
		*dev_type = MTD_DEV_TYPE_ONENAND;
		p += 7;
	} else {
		// printf("incorrect device type in %s\n", id);
		return 1;
	}

	if (!isdigit(*p)) {
		// printf("incorrect device number in %s\n", id);
		return 1;
	}

	// *dev_num = simple_strtoul(p, (char **)&p, 0);
	*dev_num = 0;
	/* if (ret_id)
		*ret_id = p; */
	return 0;
}

void flash_init (void)
{
    struct mtdids *id;
    struct part_info *part;
    char *dev_name;
    u32 size;
    u32 mem_type;
    int nand_maf_id, nand_dev_id;

    mtd_info = malloc(sizeof(struct mtd_info));
    memset(mtd_info, 0, sizeof(struct mtd_info));

    mem_type = get_mem_type();

    if (mem_type == GPMC_ONENAND) {
	    onenand_chip = malloc(sizeof(struct onenand_chip));
	    memset(onenand_chip, 0, sizeof(struct onenand_chip));
	    onenand_chip->base = (void *)CONFIG_SYS_ONENAND_BASE;
	    mtd_info->priv = onenand_chip;
	    dev_name ="onenand0";
	    onenand_scan(mtd_info, 1);
    } else if (mem_type == GPMC_NAND) {
	    nand_chip = malloc(sizeof(struct nand_chip));
	    memset(nand_chip, 0, sizeof(struct nand_chip));
	    nand_chip->IO_ADDR_R = (void *)GPMC_NAND_DATA_0;
	    nand_chip->options |= NAND_BUSWIDTH_16;
	    mtd_info->priv = nand_chip;
	    dev_name ="nand0";
	    nand_scan(mtd_info, 1, &nand_maf_id, &nand_dev_id);
    } else {
	    // printf("IGEP: Flash: unsupported sysboot sequence found\n");
	    hang();
    }

#ifdef __DEBUG__
	printf("Flash Size: ");
	print_size(mtd_info->size, "\n");
#endif

	/*
	 * Add MTD device so that we can reference it later
	 * via the mtdcore infrastructure (e.g. ubi).
	 */
	mtd_info->name = dev_name;
	// add_mtd_device(onenand_mtd);

	/* jffs2 */

    current_mtd_dev = (struct mtd_device *) malloc(sizeof(struct mtd_device) +
                                            sizeof(struct part_info) +
                                            sizeof(struct mtdids));
    memset(current_mtd_dev, 0, sizeof(struct mtd_device) +
		       sizeof(struct part_info) + sizeof(struct mtdids));

    id = (struct mtdids *)(current_mtd_dev + 1);
    part = (struct part_info *)(id + 1);

    /* id */
    id->mtd_id = "single part";

    if ((mtd_id_parse(dev_name, NULL, &id->type, &id->num) != 0) ||
            (mtd_device_validate(id->type, id->num, &size) != 0)) {
			// printf("incorrect device: %s%d\n", MTD_DEV_TYPE(id->type), id->num);
			free(current_mtd_dev);
			return 1;
    }
    id->size = size;
    INIT_LIST_HEAD(&id->link);
#ifdef __DEBUG__
    printf("dev id: type = %d, num = %d, size = 0x%08lx, mtd_id = %s\n",
				id->type, id->num, id->size, id->mtd_id);
#endif
    /* partition */
    part->name = "static";
    part->auto_name = 0;

#if defined(CONFIG_JFFS2_PART_SIZE)
    part->size = CONFIG_JFFS2_PART_SIZE;
#else
	part->size = SIZE_REMAINING;
#endif

#if defined(CONFIG_JFFS2_PART_OFFSET)
    part->offset = CONFIG_JFFS2_PART_OFFSET;
#else
    part->offset = 0x00000000;
#endif

    part->dev = current_mtd_dev;
    INIT_LIST_HEAD(&part->link);

    /* recalculate size if needed */
    // if (part->size == SIZE_REMAINING)
        // part->size = id->size - part->offset;

    part->sector_size = get_part_sector_size(id, part);
#ifdef __DEBUG__
    printf("part  : name = %s, size = 0x%08lx, offset = 0x%08lx\n",
				part->name, part->size, part->offset);
#endif
		/* device */
    current_mtd_dev->id = id;
    INIT_LIST_HEAD(&current_mtd_dev->link);
    current_mtd_dev->num_parts = 1;
    INIT_LIST_HEAD(&current_mtd_dev->parts);
    list_add(&part->link, &current_mtd_dev->parts);

	return 0;

}

#ifdef IGEP00X_ENABLE_FLASH_BOOT
int load_jffs2_file (const char* filename, char* dest)
{
    struct mtdids *id;
    struct part_info *part;
    id = (struct mtdids *)(current_mtd_dev + 1);
    part = (struct part_info *)(id + 1);
    return jffs2_1pass_load(dest , part, filename);
}
#endif

/**********************************************************
 * Routine: flash_setup
 * Description: Set up flash, NAND and OneNAND
 *********************************************************/
int flash_setup(void)
{
#ifdef __DEBUG__
	onenand_check_maf(ONENAND_MANUF_ID());
	onenand_print_device_info(ONENAND_DEVICE_ID(), ONENAND_VERSION_ID());
#endif
	return 0;
}

/* optionally do something */
void board_hang(void)
{
    reset_timer();
    while(1){
        omap_set_gpio_dataout(GPIO_LED_USER0, 1);
        __udelay(500000);
        omap_set_gpio_dataout(GPIO_LED_USER0, 0);
        __udelay(500000);
    }
}

/******************************************************************************
 * Dummy function to handle errors for EABI incompatibility
 *****************************************************************************/
void raise(void)
{
}

/******************************************************************************
 * Dummy function to handle errors for EABI incompatibility
 *****************************************************************************/
void abort(void)
{
}
