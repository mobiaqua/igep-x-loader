/*
 * Copyright (C) 2010 ISEE
 * Manel Caro, ISEE, mcaro@iseebcn.com.
 *
 * Copyright (C) 2005 Texas Instruments.
 * (C) Copyright 2004
 * Jian Zhang, Texas Instruments, jzhang@ti.com.
 *
 * (C) Copyright 2002
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
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
#include <part.h>
#include <fat.h>
#include <asm/arch/mem.h>

#ifdef CONFIG_ONENAND
#include <linux/mtd/onenand.h>
#endif

const char version_string[] =
	"IGEP-X-Loader 2.1.0-1 (" __DATE__ " - " __TIME__ ")";

int print_info(void)
{
#ifdef CFG_PRINTF
        printf("\n\n%s\n", version_string);
#endif
	return 0;
}

static int init_func_i2c (void)
{
    /* Initialize TPS65950 - i2c Connection */
	i2c_init (CFG_I2C_SPEED, CFG_I2C_SLAVE);
	return 0;
}

typedef int (init_fnc_t) (void);

init_fnc_t *init_sequence[] = {
	cpu_init,		/* basic cpu dependent setup */
	init_func_i2c,
	board_init,		/* basic board dependent setup */
#ifdef CFG_NS16550_SERIAL
 	serial_init,		/* serial communications setup */
#endif
	print_info,
  	nand_init,		/* board specific nand init */
  	NULL,
};

void start_armboot (void)
{
  	init_fnc_t **init_fnc_ptr;
 	int i, size;
	uchar *buf;
	int *first_instruction;
	block_dev_desc_t *dev_desc = NULL;

#ifdef CONFIG_ONENAND
	unsigned int onenand_features;
#endif
	/* Execute init_sequence */
   	for (init_fnc_ptr = init_sequence; *init_fnc_ptr; ++init_fnc_ptr) {
		if ((*init_fnc_ptr)() != 0) {
			hang ();
		}
	}

	/* Execute board specific misc init */
	misc_init_r();

    /* Initialize OneNand */
	onenand_init();

    /* Initialize MMC */
    mmc_init(1);

    /* Initialize fat dynamic structures */
    init_fat();

    /* Load the Linux kernel */
    /* boot_linux() should never return */
    boot_linux();

    /* If boot linux fails hang the board */
    hang();
}

void hang (void)
{
	/* call board specific hang function */
	board_hang();

	/* if board_hang() returns, hang here */
	for (;;);
}
