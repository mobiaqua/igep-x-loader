/*
 * (C) Copyright 2005 Samsung Electronis
 * Kyungmin Park <kyungmin.park@samsung.com>
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

#include <asm/string.h>

#include "onenand_regs.h"

#define onenand_readw(a)	(*(volatile unsigned short *)(a))
#define onenand_writew(v, a)	((*(volatile unsigned short *)(a)) = (u16) (v))

#define SAMSUNG_MFR_ID		0xEC
#define NUMONYX_MFR_ID		0x20
#define KFM1G16Q2A_DEV_ID	0x30
#define KFN2G16Q2A_DEV_ID	0x40
#define NAND01GR4E_DEV_ID	0x30
#define NAND02GR4E_DEV_ID	0x40
#define NAND04GR4E_DEV_ID	0x58

#define THIS_ONENAND(a)		(ONENAND_ADDR + (a))

#define READ_INTERRUPT()						\
	onenand_readw(THIS_ONENAND(ONENAND_REG_INTERRUPT))

#define READ_CTRL_STATUS()						\
	onenand_readw(THIS_ONENAND(ONENAND_REG_CTRL_STATUS))

#define READ_ECC_STATUS()						\
	onenand_readw(THIS_ONENAND(ONENAND_REG_ECC_STATUS))

#define SET_EMIFS_CS_CONFIG(v)					\
	(*(volatile unsigned long *)(OMAP_EMIFS_CS_CONFIG) = (v))

#define ONENAND_DEVICE_ID()				\
	(*(volatile unsigned short *)(THIS_ONENAND(ONENAND_REG_DEVICE_ID)))

#define ONENAND_IS_DDP()					\
        (ONENAND_DEVICE_ID() & ONENAND_DEVICE_IS_DDP)

/**
 * onenand_page_address - [DEFAULT] Get page address
 * @param page          the page address
 * @param sector        the sector address
 * @return              combined page and sector address
 *
 * Setup Start Address 8 Register (F107h)
 */
static int onenand_page_address(int page, int sector)
{
        /* Flash Page Address, Flash Sector Address */
        int fpa, fsa;

        fpa = page & ONENAND_FPA_MASK;
        fsa = sector & ONENAND_FSA_MASK;

        return ((fpa << ONENAND_FPA_SHIFT) | fsa);
}

/**
 * onenand_buffer_address - [DEFAULT] Get buffer address
 * @param dataram1      DataRAM index
 * @param sectors       the sector address
 * @param count         the number of sectors
 * @return              the start buffer value
 *
 * Setup Start Buffer Register (F200h)
 */
static int onenand_buffer_address(int dataram1, int sectors, int count)
{
        int bsa, bsc;

        /* BufferRAM Sector Address */
        bsa = sectors & ONENAND_BSA_MASK;
        if (dataram1)
              bsa |= ONENAND_BSA_DATARAM1;    /* DataRAM1 */
        else
              bsa |= ONENAND_BSA_DATARAM0;    /* DataRAM0 */

        /* BufferRAM Sector Count */
        bsc = count & ONENAND_BSC_MASK;

        return ((bsa << ONENAND_BSA_SHIFT) | bsc);
}

/**
 * onenand_get_density - [DEFAULT] Get OneNAND density
 * @param dev_id        OneNAND device ID
 *
 * Get OneNAND density from device ID
 */
static inline int onenand_get_density(int dev_id)
{
        int density = dev_id >> ONENAND_DEVICE_DENSITY_SHIFT;
        return (density & ONENAND_DEVICE_DENSITY_MASK);
}

/**
 * onenand_block_address - [DEFAULT] Get block address
 * @param block         the block
 * @return              translated block address if DDP, otherwise same
 *
 * Setup Start Address 1 Register (F100h)
 */
static int onenand_block_address(int block)
{
	int density, density_mask;

	density = onenand_get_density(ONENAND_DEVICE_ID());
	/* Set density mask. it is used for DDP */
        if (ONENAND_IS_DDP())
                density_mask = (1 << (density + 6));
        else
                density_mask = 0;

        /* Device Flash Core select, NAND Flash Block Address */
        if (block & density_mask)
                return ONENAND_DDP_CHIP1 | (block ^ density_mask);

        return block;
}

/**
 * onenand_bufferram_address - [DEFAULT] Get bufferram address
 * @param block         the block
 * @return              set DBS value if DDP, otherwise 0
 *
 * Setup Start Address 2 Register (F101h) for DDP
 */
static int onenand_bufferram_address(int block)
{
        int density, density_mask;

        density = onenand_get_density(ONENAND_DEVICE_ID());
        /* Set density mask. it is used for DDP */
        if (ONENAND_IS_DDP())
                density_mask = (1 << (density + 6));
        else
                density_mask = 0;

        /* Device BufferRAM Select */
        if (block & density_mask)
                return ONENAND_DDP_CHIP1;

        return ONENAND_DDP_CHIP0;
}

#if defined(CFG_SYNC_BURST_READ) && defined(CONFIG_OMAP1610)
static inline void set_sync_burst_read(void)
{
	unsigned int value;
	value = 0
		| (0x1 << 15)		/* Read Mode: Synchronous */
		| (0x4 << 12)		/* Burst Read Latency: 4 cycles */
		| (0x4 << 9)		/* Burst Length: 8 word */
		| (0x1 << 7)		/* RDY signal plarity */
		| (0x1 << 6)		/* INT signal plarity */
		| (0x1 << 5)		/* I/O buffer enable */
		;
	onenand_writew(value, THIS_ONENAND(ONENAND_REG_SYS_CFG1));

	value = 0
		| (4 << 16)		/* Synchronous Burst Read */
		| (1 << 12)		/* PGWST/WELEN */
		| (1 << 8)		/* WRWST */
		| (4 << 4)		/* RDWST */
		| (1 << 0)		/* FCLKDIV => 48MHz */
		;
	SET_EMIFS_CS_CONFIG(value);
}
static inline void set_async_read(void)
{
	unsigned int value;
	value = 0
		| (0x0 << 15)		/* Read Mode: Asynchronous */
		| (0x4 << 12)		/* Burst Read Latency: 4 cycles */
		| (0x0 << 9)		/* Burst Length: continuous */
		| (0x1 << 7)		/* RDY signal plarity */
		| (0x1 << 6)		/* INT signal plarity */
		| (0x0 << 5)		/* I/O buffer disable */
		;
	onenand_writew(value, THIS_ONENAND(ONENAND_REG_SYS_CFG1));

	value = 0
		| (0 << 16)		/* Asynchronous Read */
		| (1 << 12)		/* PGWST/WELEN */
		| (1 << 8)		/* WRWST */
		| (3 << 4)		/* RDWST */
		| (1 << 0)		/* FCLKDIV => 48MHz */
		;
	SET_EMIFS_CS_CONFIG(value);
}
#else
#define set_sync_burst_read(...)	do { } while (0)
#define set_async_read(...)		do { } while (0)
#endif

int
onenand_chip()
{
	unsigned short mf_id, dev_id;
	mf_id = (*(volatile unsigned short *)(THIS_ONENAND(ONENAND_REG_MANUFACTURER_ID)));
	dev_id = (*(volatile unsigned short *)(THIS_ONENAND(ONENAND_REG_DEVICE_ID)));

	if(mf_id == SAMSUNG_MFR_ID) {
		if (dev_id == KFM1G16Q2A_DEV_ID) {
		printf("Detected Samsung MuxOneNAND1G Flash \r\n");
		return 0;
		} else if (dev_id == KFN2G16Q2A_DEV_ID) {
			printf("Detected Samsung MuxOneNAND2G Flash \r\n");
                        return 0;
		} else {
			printf(" ONENAND Flash unsupported\r\n");
                        return 1;
		}
	} else if(mf_id == NUMONYX_MFR_ID) {
		if (dev_id == NAND01GR4E_DEV_ID) {
			printf("Detected Numonyx OneNAND 1G Flash \r\n");
			return 0;
		} else if (dev_id == NAND02GR4E_DEV_ID) {
			printf("Detected Numonyx OneNAND 2G Flash \r\n");
			return 0;
		} else if (dev_id == NAND04GR4E_DEV_ID) {
			printf("Detected Numonyx OneNAND 4G Flash \r\n");
                        return 0;
		} else {
                        printf(" ONENAND Flash unsupported\r\n");
                        return 1;
		}
	} else {
		printf("ONENAND Flash Unsupported\r\n");
		return 1;
	}
}

/* read a page with ECC */
static inline int onenand_read_page(ulong block, ulong page, u_char *buf, int dataram)
{
	unsigned long *base;
	int sectors = 4, count = 4, retval;

#ifndef __HAVE_ARCH_MEMCPY32
	unsigned int offset, value;
	unsigned long *p;
	unsigned int ctrl, ecc;
	unsigned short bbmarker;
#endif

	/* Write 'DFS, FBA' of Flash */
	retval = onenand_block_address(block);
	onenand_writew(retval, THIS_ONENAND(ONENAND_REG_START_ADDRESS1));

	/* Select DataRAM for DDP */
	retval = onenand_bufferram_address(block);
	onenand_writew(retval, THIS_ONENAND(ONENAND_REG_START_ADDRESS2));

	/* Write 'FPA, FSA' of Flash */
	retval = onenand_page_address(page, sectors);
	onenand_writew(retval, THIS_ONENAND(ONENAND_REG_START_ADDRESS8));

	/* Write 'BSA, BSC' of DataRAM */
        retval = onenand_buffer_address(dataram, sectors, count);
	onenand_writew(retval, THIS_ONENAND(ONENAND_REG_START_BUFFER));

	/* Interrupt clear */
	onenand_writew(ONENAND_INT_CLEAR, THIS_ONENAND(ONENAND_REG_INTERRUPT));

	/* Read command */
	onenand_writew(ONENAND_CMD_READ, THIS_ONENAND(ONENAND_REG_COMMAND));

#ifndef __HAVE_ARCH_MEMCPY32
 	p = (unsigned long *) buf;
#endif
	base = (unsigned long *) (THIS_ONENAND(ONENAND_DATARAM));

	while (!(READ_INTERRUPT() & ONENAND_INT_MASTER))
		continue;

	/* Check for invalid block mark. Bad block markers */
	/* are stored in spare area of 1st or 2nd page */
	if (page < 2 && (onenand_readw(THIS_ONENAND(ONENAND_SPARERAM)) != 0xffff))
		return 1;

#ifdef __HAVE_ARCH_MEMCPY32
	/* 32 bytes boundary memory copy */
	memcpy32(buf, base, ONENAND_PAGE_SIZE);
#else
	for (offset = 0; offset < (ONENAND_PAGE_SIZE >> 2); offset++) {
		value = *(base + offset);
		*p++ = value;
 	}
#endif

	return 0;
}

#define ONENAND_START_PAGE		0
#define ONENAND_PAGES_PER_BLOCK		64

/**
 * onenand_read_block - Read a block data to buf skipping bad blocks
 * @return 0 on sucess
 */ 

int onenand_read_block(unsigned char *buf, ulong block)
{
	int page, offset = 0;

	set_sync_burst_read();

	/* NOTE: you must read page from page 1 of block 0 */
	/* read the block page by page */
	for (page = ONENAND_START_PAGE; page < ONENAND_PAGES_PER_BLOCK; page++) {

		if (onenand_read_page(block, page, buf + offset, 0)) {
		    /* This block is bad. Skip it
                     * and read next block */
		    printf("Skipping Bad block %d\n", block);
		    set_async_read();
		    return 1;
		}

		offset += ONENAND_PAGE_SIZE;

	#ifdef ONENAND_HAS_2PLANE
		/* Is it the odd plane ? */
		if (onenand_read_page(block + 1, page, buf + offset, 0)) {
			/* This block is bad. Skip it
			 * and read next block */
			printf("Skipping Bad block %d\n", block + 1);
			set_async_read();
			return 1;
		}

		offset += ONENAND_PAGE_SIZE;
	#endif

	}

	set_async_read();

	return 0;
}
