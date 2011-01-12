#ifdef __notdef
#include <asm/arch/cpu.h>
#include <asm/io.h>
#include <asm/arch/mem.h>


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
#endif
