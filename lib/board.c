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
	"IGEP-X-Loader 2.5.0-1 (" __DATE__ " - " __TIME__ ")";

int print_info(void)
{
#ifdef CFG_PRINTF
    const char clear [] = {0x1b, 0x5b, '2', 'J', 0 };
    printf("%s", clear);
    printf("\n%s\n", version_string);
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
#ifdef CFG_NS16550_SERIAL
 	serial_init,		/* serial communications setup */
#endif
	board_init,		/* basic board dependent setup */
	print_info,
	flash_setup,	     	/* board specific nand init */
  	NULL,
};

/* Main Boot function called from ASM */
void start_armboot (void)
{
  	init_fnc_t **init_fnc_ptr;
 	int i, size;
	uchar *buf;
	int *first_instruction;
	block_dev_desc_t *dev_desc = NULL;
    u8* splash = 0;
#ifdef CONFIG_ONENAND
	unsigned int onenand_features;
#endif
	/* Execute init_sequence */
	i = 0;
   	for (init_fnc_ptr = init_sequence; *init_fnc_ptr; ++init_fnc_ptr) {
		if ((*init_fnc_ptr)() != 0)
			hang ();
	}

/*    splash = malloc (1024 * 768 * 4);
    if(file_fat_read("splash.dat", splash, 0))
        enable_video_buffer(splash);
    else */
    enable_video_color(0x001E90FF);


	/* Execute board specific misc init */
	misc_init_r();

	flash_init();

	/* Initialize MMC */
	mmc_init(1);

#ifdef IGEP00X_ENABLE_MMC_BOOT
	/* Initialize fat dynamic structures */
	init_fat();
#endif

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
