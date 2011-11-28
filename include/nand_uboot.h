/*
 *  Header file for OneNAND support for U-Boot
 *
 *  Adaptation from kernel to U-Boot
 *
 *  Copyright (C) 2005-2007 Samsung Electronics
 *  Kyungmin Park <kyungmin.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __UBOOT_NAND_H
#define __UBOOT_NAND_H

#include <linux/types.h>

/* Forward declarations */
struct mtd_info;
struct mtd_oob_ops;
struct erase_info;
struct nand_chip;

extern struct mtd_info *mtd_info;

/* board */
extern void nand_board_init(struct mtd_info *);

/* Functions */
extern void nand_init(void);
extern int nand_read(struct mtd_info *mtd, loff_t from, size_t len,
			size_t * retlen, u_char *buf);
extern int nand_read_oob(struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops);
extern int nand_write(struct mtd_info *mtd, loff_t from, size_t len,
			 size_t * retlen, const u_char * buf);
extern int nand_erase(struct mtd_info *mtd, struct erase_info *instr);

extern char *nand_print_device_info(int device, int version);

#endif /* __UBOOT_NAND_H */
