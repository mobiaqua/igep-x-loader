/*
 * (C) Copyright 2009
 * Integration Software and Electronics Engineering, <www.iseebcn.com>
 *
 * X-Loader Configuration settings for the IGEP0020 board.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __CONFIG_H
#define __CONFIG_H

/* serial printf facility takes about 3.5K */
#define CFG_PRINTF
//#undef CFG_PRINTF

/*
 * High Level Configuration Options
 */
#define CONFIG_ARMCORTEXA8	1	/* This is an ARM V7 CPU core */
#define CONFIG_OMAP		1	/* in a TI OMAP core */
#define CONFIG_OMAP34XX		1	/* which is a 34XX */
#define CONFIG_OMAP3430		1	/* which is in a 3430 */
#define CONFIG_OMAP3_IGEP0020	1	/* working with IGEP0020 */

/* Enable the below macro if MMC boot support is required */
#define CONFIG_MMC	1
#if defined(CONFIG_MMC)
	#define CFG_CMD_MMC		1
	#define CFG_CMD_FAT		1
	#define CFG_I2C_SPEED		100000
	#define CFG_I2C_SLAVE		1
	#define CFG_I2C_BUS		0
	#define CFG_I2C_BUS_SELECT	1
	#define CONFIG_DRIVER_OMAP34XX_I2C 1
#endif

#include <asm/arch/cpu.h>        /* get chip and board defs */

/* uncomment it if you need timer based udelay(). it takes about 250 bytes */
//#define CFG_UDELAY

/* Clock Defines */
#define V_OSCK	26000000  /* Clock output from T2 */

#if (V_OSCK > 19200000)
#define V_SCLK	(V_OSCK >> 1)
#else
#define V_SCLK	V_OSCK
#endif

//#define PRCM_CLK_CFG2_266MHZ	1	/* VDD2=1.15v - 133MHz DDR */
#define PRCM_CLK_CFG2_332MHZ	1	/* VDD2=1.15v - 166MHz DDR */
#define PRCM_PCLK_OPP2		1	/* ARM=381MHz - VDD1=1.20v */

/* Memory type */
//#define CONFIG_SDRAM_M65KX001AM 1	/* 1Gb, DDR x32, 4KB page */
#define CONFIG_SDRAM_M65KX002AM 1	/* 2 dice of 2Gb, DDR x32, 4KB page */

/* The actual register values are defined in u-boot- mem.h */
/* SDRAM Bank Allocation method */
//#define SDRC_B_R_C		1
//#define SDRC_B1_R_B0_C	1
#define SDRC_R_B_C		1

#define OMAP34XX_GPMC_CS0_SIZE GPMC_SIZE_128M

#ifdef CFG_PRINTF

#define CFG_NS16550
#define CFG_NS16550_SERIAL
#define CFG_NS16550_REG_SIZE	-4
#define CFG_NS16550_CLK		48000000
#define CFG_NS16550_COM3	OMAP34XX_UART3

/*
 * select serial console configuration
 */
#define CONFIG_SERIAL1		3	/* use UART3 */
#define CONFIG_CONS_INDEX	3

#define CONFIG_BAUDRATE		115200
#define CFG_PBSIZE		256

#endif /* CFG_PRINTF */

/*
 * Miscellaneous configurable options
 */
#define CFG_LOADADDR		0x80008000

#undef	CFG_CLKS_IN_HZ		/* everything, incl board info, in Hz */

/*
 * Stack sizes
 *
 * The stack sizes are set up in start.S using the settings below
 */
#define CONFIG_STACKSIZE	(128*1024) /* regular stack */

/*
 * Board oneNAND Info.
 */
#define CONFIG_CMD_ONENAND              1
#define CONFIG_ONENAND                  1
#define CONFIG_MTD_ONENAND_2X_PROGRAM   1

#define ONENAND_BASE	ONENAND_MAP
#define ONENAND_ADDR	ONENAND_BASE
#define CONFIG_SYS_ONENAND_BASE		ONENAND_MAP

#define ONENAND_START_BLOCK 	4	 /* 0x00080000 */
#define ONENAND_END_BLOCK	16	 /* 0x00200000 */
#define ONENAND_PAGE_SIZE	2048     /* 2KB */
#define ONENAND_BLOCK_SIZE	0x20000  /* 128KB */

#define CONFIG_NR_DRAM_BANKS        2

// #define CONFIG_JFFS2_PART_SIZE      0xA00000
#define CONFIG_JFFS2_PART_SIZE      0xC00000
#define CONFIG_JFFS2_PART_OFFSET    0x80000
// #define CONFIG_JFFS2_PART_OFFSET    0x580000

/* Memory work */
#define XLOADER_CFG_GLOBAL_PTR      0x90000000
#define XLOADER_KERNEL_PARAMS       0x80000100                  /* Kernel params */
#define XLOADER_KERNEL_MEMORY       XLOADER_CFG_GLOBAL_PTR      /* struct Linux_Memory_Layout (Reserved 32K) */
#define XLOADER_CFG_FILE            XLOADER_KERNEL_MEMORY + (32 * 1024)      /* Configuration file : Reserved 32K */
#define XLOADER_MALLOC_IPTR         XLOADER_CFG_FILE + (32 * 1024)  /* Malloc Initial Pointer */
#define XLOADER_MALLOC_SIZE         32 * (1024 * 1014)  /* Malloc space size = 32 M Bytes */

#endif /* __CONFIG_H */

