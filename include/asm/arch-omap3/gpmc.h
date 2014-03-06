/*
 * (C) Copyright 2010
 * ISEE <www.iseebcn.com>
 * Manel Caro <general@iseebcn.com>
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

#ifndef _IGEP_GPMC_H_
#define _IGEP_GPMC_H_

#define __GPMC_PREFETCH_ENGINE__

#ifdef __GPMC_PREFETCH_ENGINE__
#define PREFETCH_FIFOTHRESHOLD_MAX      0x40
#define PREFETCH_FIFOTHRESHOLD(val)     ((val) << 8)
#define CS_NUM_SHIFT		24
#define ENABLE_PREFETCH		(0x1 << 7)
#define DMA_MPU_MODE		2
#define GPMC_PREFETCH_CONFIG1	0x1e0
#define GPMC_PREFETCH_CONFIG2	0x1e4
#define GPMC_PREFETCH_CONTROL	0x1ec
#define GPMC_PREFETCH_STATUS	0x1f0
#define GPMC_PREFETCH_STATUS_FIFO_CNT(val)      ((val >> 24) & 0x7F)
#define GPMC_PREFETCH_STATUS_COUNT(val) (val & 0x00003fff)
#define GPMC_PREFETCH_FIFO_CNT  0x00000007 /* bytes available in FIFO for r/w */
#define GPMC_PREFETCH_COUNT     0x00000008 /* remaining bytes to be read/write*/
#define GPMC_STATUS_BUFFER      0x00000009 /* 1: buffer is available to write */
#endif

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

void gpmc_init (void);
void setup_net_chip (unsigned int);
int gpmc_prefetch_enable(int cs, int fifo_th, int dma_mode,
				unsigned int u32_count, int is_write);
int gpmc_prefetch_reset(int cs);
int gpmc_read_status(int cmd);
#endif
