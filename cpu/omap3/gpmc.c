#include <asm/types.h>
#include <asm/arch/cpu.h>
#include <asm/io.h>
#include <asm/arch/mem.h>
#include <asm/arch/omap3.h>
// #include <asm/arch/gpmc.h>

struct gpmc_cs {
	u32 config1;		/* 0x00 */
	u32 config2;		/* 0x04 */
	u32 config3;		/* 0x08 */
	u32 config4;		/* 0x0C */
	u32 config5;		/* 0x10 */
	u32 config6;		/* 0x14 */
	u32 config7;		/* 0x18 */
	u32 nand_cmd;		/* 0x1C */
	u32 nand_adr;		/* 0x20 */
	u32 nand_dat;		/* 0x24 */
	u8 res[8];		/* blow up to 0x30 byte */
};

struct gpmc {
	u8 res1[0x10];
	u32 sysconfig;		/* 0x10 */
	u8 res2[0x4];
	u32 irqstatus;		/* 0x18 */
	u32 irqenable;		/* 0x1C */
	u8 res3[0x20];
	u32 timeout_control; 	/* 0x40 */
	u8 res4[0xC];
	u32 config;		/* 0x50 */
	u32 status;		/* 0x54 */
	u8 res5[0x8];	/* 0x58 */
	struct gpmc_cs cs[8];	/* 0x60, 0x90, .. */
	u8 res6[0x14];		/* 0x1E0 */
	u32 ecc_config;		/* 0x1F4 */
	u32 ecc_control;	/* 0x1F8 */
	u32 ecc_size_config;	/* 0x1FC */
	u32 ecc1_result;	/* 0x200 */
	u32 ecc2_result;	/* 0x204 */
	u32 ecc3_result;	/* 0x208 */
	u32 ecc4_result;	/* 0x20C */
	u32 ecc5_result;	/* 0x210 */
	u32 ecc6_result;	/* 0x214 */
	u32 ecc7_result;	/* 0x218 */
	u32 ecc8_result;	/* 0x21C */
	u32 ecc9_result;	/* 0x220 */
};

struct ctrl {
	u8 res1[0xC0];
	u16 gpmc_nadv_ale;	/* 0xC0 */
	u16 gpmc_noe;		/* 0xC2 */
	u16 gpmc_nwe;		/* 0xC4 */
	u8 res2[0x22A];
	u32 status;		/* 0x2F0 */
	u32 gpstatus;		/* 0x2F4 */
	u8 res3[0x08];
	u32 rpubkey_0;		/* 0x300 */
	u32 rpubkey_1;		/* 0x304 */
	u32 rpubkey_2;		/* 0x308 */
	u32 rpubkey_3;		/* 0x30C */
	u32 rpubkey_4;		/* 0x310 */
	u8 res4[0x04];
	u32 randkey_0;		/* 0x318 */
	u32 randkey_1;		/* 0x31C */
	u32 randkey_2;		/* 0x320 */
	u32 randkey_3;		/* 0x324 */
	u8 res5[0x124];
	u32 ctrl_omap_stat;	/* 0x44C */
};


/*****************************************************
 * gpmc_init(): init gpmc bus
 * Init GPMC for x16, MuxMode (SDRAM in x32).
 * This code can only be executed from SRAM or SDRAM.
 *****************************************************/
void gpmc_init (void)
{
	/* putting a blanket check on GPMC based on ZeBu for now */
	struct gpmc* gpmc_cfg = (struct gpmc *)GPMC_BASE;
	u32 config = 0;

	/* global settings */
	writel(0, &gpmc_cfg->irqenable); /* isr's sources masked */
	writel(0, &gpmc_cfg->timeout_control);/* timeout disable */

	/* Set WAIT polarity actuve low*/
	config = readl(&gpmc_cfg->config);
	config &= (~0xf00);
	writel(config, &gpmc_cfg->config);

	/*
	 * Disable the GPMC0 config set by ROM code
	 * It conflicts with our MPDB (both at 0x08000000)
	 */
	writel(0, &gpmc_cfg->cs[0].config7);
	udelay(1000);
}


/*
 * Routine: setup_net_chip
 * Description: Setting up the configuration GPMC registers specific to the
 *		Ethernet hardware.
 */
void setup_net_chip (u32 processor)
{
    int i;
    struct gpmc* gpmc_cfg = (struct gpmc *)GPMC_BASE;
	struct gpio* gpio3_base = (struct gpio *)OMAP34XX_GPIO3_BASE;
/*	struct gpio* gpio2_base = (struct gpio *)OMAP34XX_GPIO2_BASE; */
	struct ctrl* ctrl_base = (struct ctrl *)OMAP34XX_CTRL_BASE;

	/* Configure GPMC registers */
	if(processor == CPU_OMAP36XX){
        writel(NET_GPMC_CONFIG1, &gpmc_cfg->cs[5].config1);
        writel(NET_GPMC_CONFIG2, &gpmc_cfg->cs[5].config2);
        writel(NET_GPMC_CONFIG3, &gpmc_cfg->cs[5].config3);
        writel(NET_GPMC_CONFIG4, &gpmc_cfg->cs[5].config4);
        writel(NET_GPMC_CONFIG5, &gpmc_cfg->cs[5].config5);
        writel(NET_GPMC_CONFIG6, &gpmc_cfg->cs[5].config6);
        writel(NET_GPMC_CONFIG7, &gpmc_cfg->cs[5].config7);
	}
	else{
        writel(NET_LAN9221_GPMC_CONFIG1, &gpmc_cfg->cs[5].config1);
        writel(NET_LAN9221_GPMC_CONFIG2, &gpmc_cfg->cs[5].config2);
        writel(NET_LAN9221_GPMC_CONFIG3, &gpmc_cfg->cs[5].config3);
        writel(NET_LAN9221_GPMC_CONFIG4, &gpmc_cfg->cs[5].config4);
        writel(NET_LAN9221_GPMC_CONFIG5, &gpmc_cfg->cs[5].config5);
        writel(NET_LAN9221_GPMC_CONFIG6, &gpmc_cfg->cs[5].config6);
        writel(NET_LAN9221_GPMC_CONFIG7, &gpmc_cfg->cs[5].config7);
#ifdef __notdef
        // IGEP Module with DUAL Ethernet
        writel(NET_LAN9221_GPMC_CONFIG1, &gpmc_cfg->cs[4].config1);
        writel(NET_LAN9221_GPMC_CONFIG2, &gpmc_cfg->cs[4].config2);
        writel(NET_LAN9221_GPMC_CONFIG3, &gpmc_cfg->cs[4].config3);
        writel(NET_LAN9221_GPMC_CONFIG4, &gpmc_cfg->cs[4].config4);
        writel(NET_LAN9221_GPMC_CONFIG5, &gpmc_cfg->cs[4].config5);
        writel(NET_LAN9221_GPMC_CONFIG6, &gpmc_cfg->cs[4].config6);
        writel(NET_LAN9221_GPMC_CONFIG7, &gpmc_cfg->cs[4].config7);
#endif
    }

	/* Enable off mode for NWE in PADCONF_GPMC_NWE register */
	writew(readw(&ctrl_base ->gpmc_nwe) | 0x0E00, &ctrl_base->gpmc_nwe);
	/* Enable off mode for NOE in PADCONF_GPMC_NADV_ALE register */
	writew(readw(&ctrl_base->gpmc_noe) | 0x0E00, &ctrl_base->gpmc_noe);
	/* Enable off mode for ALE in PADCONF_GPMC_NADV_ALE register */
	writew(readw(&ctrl_base->gpmc_nadv_ale) | 0x0E00, &ctrl_base->gpmc_nadv_ale);

	/* Make GPIO 64 as output pin */
	writel(readl(&gpio3_base->oe) & ~(GPIO0), &gpio3_base->oe);

	/* Now send a pulse on the GPIO pin */
	writel(GPIO0, &gpio3_base->setdataout);
	udelay(1);
	writel(GPIO0, &gpio3_base->cleardataout);
	udelay(1);
	writel(GPIO0, &gpio3_base->setdataout);

}
