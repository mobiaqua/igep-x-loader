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

#ifdef __GPMC_PREFETCH_ENGINE__
static void gpmc_write_reg(int idx, u32 val)
{
    // struct gpmc* gpmc_base = (struct gpmc *)GPMC_BASE;
	__raw_writel(val, GPMC_BASE + idx);
}

static u32 gpmc_read_reg(int idx)
{
    //struct gpmc* gpmc_base = (struct gpmc *)GPMC_BASE;
	return __raw_readl(GPMC_BASE + idx);
}


/**
 * gpmc_read_status - read access request to get the different gpmc status
 * @cmd: command type
 * @return status
 */
int gpmc_read_status(int cmd)
{
	int	status = -1;
	u32	regval = 0;

	switch (cmd) {
	case GPMC_PREFETCH_FIFO_CNT:
		regval = gpmc_read_reg(GPMC_PREFETCH_STATUS);
		status = GPMC_PREFETCH_STATUS_FIFO_CNT(regval);
		break;

	case GPMC_PREFETCH_COUNT:
		regval = gpmc_read_reg(GPMC_PREFETCH_STATUS);
		status = GPMC_PREFETCH_STATUS_COUNT(regval);
		break;

	default:
            return -1;
	}
	return status;
}


/**
 * gpmc_prefetch_enable - configures and starts prefetch transfer
 * @cs: cs (chip select) number
 * @fifo_th: fifo threshold to be used for read/ write
 * @dma_mode: dma mode enable (1) or disable (0)
 * @u32_count: number of bytes to be transferred
 * @is_write: prefetch read(0) or write post(1) mode
 */
int gpmc_prefetch_enable(int cs, int fifo_th, int dma_mode,
				unsigned int u32_count, int is_write)
{

	if (fifo_th > PREFETCH_FIFOTHRESHOLD_MAX) {
		// pr_err("gpmc: fifo threshold is not supported\n");
		return -1;
	} else if (!(gpmc_read_reg(GPMC_PREFETCH_CONTROL))) {
		/* Set the amount of bytes to be prefetched */
		gpmc_write_reg(GPMC_PREFETCH_CONFIG2, u32_count);

		/* Set dma/mpu mode, the prefetch read / post write and
		 * enable the engine. Set which cs is has requested for.
		 */
		gpmc_write_reg(GPMC_PREFETCH_CONFIG1, ((cs << CS_NUM_SHIFT) |
					PREFETCH_FIFOTHRESHOLD(fifo_th) |
					ENABLE_PREFETCH |
					(dma_mode << DMA_MPU_MODE) |
					(0x1 & is_write)));

		/*  Start the prefetch engine */
		gpmc_write_reg(GPMC_PREFETCH_CONTROL, 0x1);
	} else {
		return -1;
	}

	return 0;
}

/**
 * gpmc_prefetch_reset - disables and stops the prefetch engine
 */
int gpmc_prefetch_reset(int cs)
{
	u32 config1;

	/* check if the same module/cs is trying to reset */
	config1 = gpmc_read_reg(GPMC_PREFETCH_CONFIG1);
	if (((config1 >> CS_NUM_SHIFT) & 0x7) != cs)
		return -1;

	/* Stop the PFPW engine */
	gpmc_write_reg(GPMC_PREFETCH_CONTROL, 0x0);

	/* Reset/disable the PFPW engine */
	gpmc_write_reg(GPMC_PREFETCH_CONFIG1, 0x0);

	return 0;
}
#endif
