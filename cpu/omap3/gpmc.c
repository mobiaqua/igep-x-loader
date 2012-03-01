#include <asm/types.h>
#include <asm/arch/cpu.h>
#include <asm/io.h>
#include <asm/arch/mem.h>
#include <asm/arch/omap3.h>
#include <asm/arch/gpmc.h>

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
