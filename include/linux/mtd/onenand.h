/*
 *  linux/include/linux/mtd/onenand.h
 *
 *  Copyright © 2005-2009 Samsung Electronics
 *  Kyungmin Park <kyungmin.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MTD_ONENAND_H
#define __LINUX_MTD_ONENAND_H

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

#define READ_INTERRUPT()						\
	onenand_readw(THIS_ONENAND(ONENAND_REG_INTERRUPT))

#define READ_CTRL_STATUS()						\
	onenand_readw(THIS_ONENAND(ONENAND_REG_CTRL_STATUS))

#define READ_ECC_STATUS()						\
	onenand_readw(THIS_ONENAND(ONENAND_REG_ECC_STATUS))

#define FLEXONENAND()							\
	(ONENAND_DEVICE_ID() & DEVICE_IS_FLEXONENAND)

#define ONENAND_IS_DDP()						\
	(ONENAND_DEVICE_ID() & ONENAND_DEVICE_IS_DDP)

#define ONENAND_IS_MLC()						\
	(ONENAND_TECHNOLOGY() & ONENAND_TECHNOLOGY_IS_MLC)

#ifdef CONFIG_MTD_ONENAND_2X_PROGRAM
#define ONENAND_IS_2PLANE(options)					\
	((options) & ONENAND_HAS_2PLANE)
#else
#define ONENAND_IS_2PLANE(this)			(0)
#endif

/* Check byte access in OneNAND */
#define ONENAND_CHECK_BYTE_ACCESS(addr)		(addr & 0x1)

/*
 * Options bits
 */
#define ONENAND_HAS_CONT_LOCK		(0x0001)
#define ONENAND_HAS_UNLOCK_ALL		(0x0002)
#define ONENAND_HAS_2PLANE		(0x0004)
#define ONENAND_SKIP_UNLOCK_CHECK	(0x0100)
#define ONENAND_PAGEBUF_ALLOC		(0x1000)
#define ONENAND_OOBBUF_ALLOC		(0x2000)

/*
 * OneNAND Flash Manufacturer ID Codes
 */
#define ONENAND_MFR_SAMSUNG	0xec
#define ONENAND_MFR_NUMONYX	0x20

/**
 * struct onenand_manufacturers - NAND Flash Manufacturer ID Structure
 * @name:	Manufacturer name
 * @id:		manufacturer ID code of device.
*/
struct onenand_manufacturers {
        int id;
        char *name;
};

/*
 * OneNAND Flash Devices ID Codes
 */
#define ONENAND_KFM1G16Q2A_DEV_ID	0x30
#define ONENAND_KFN2G16Q2A_DEV_ID	0x40
#define ONENAND_NAND01GR4E_DEV_ID	0x30
#define ONENAND_NAND02GR4E_DEV_ID	0x40
#define ONENAND_NAND04GR4E_DEV_ID	0x58

/* External functions */
void	onenand_print_device_info(int device, int version);
int		onenand_check_maf(int manuf);
unsigned int onenand_check_features();

#endif	/* __LINUX_MTD_ONENAND_H */

