/*
 * (C) Copyright 2004
 * Texas Instruments
 *
 * (C) Copyright 2000-2004
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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

#ifndef __COMMON_H_
#define __COMMON_H_	1

#define CONFIG_SYS_MALLOC_LEN	32 * (1024 * 1014)

#undef	_LINUX_CONFIG_H
#define _LINUX_CONFIG_H 1	/* avoid reading Linux autoconf.h file	*/

typedef unsigned char		uchar;
typedef volatile unsigned long	vu_long;
typedef volatile unsigned short vu_short;
typedef volatile unsigned char	vu_char;

#include <config.h>
#include <linux/types.h>
#include <stdarg.h>

#ifdef CONFIG_ARM
#define asmlinkage	/* nothing */
#endif


#ifdef CONFIG_ARM
# include <asm/setup.h>
# include <asm/x-load-arm.h>	/* ARM version to be fixed! */
#endif /* CONFIG_ARM */

/*
 * General Purpose Utilities
 */
#define min(X, Y)				\
	({ typeof (X) __x = (X), __y = (Y);	\
		(__x < __y) ? __x : __y; })

#define max(X, Y)				\
	({ typeof (X) __x = (X), __y = (Y);	\
		(__x > __y) ? __x : __y; })

#define MIN(x, y)  min(x, y)
#define MAX(x, y)  max(x, y)


/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})


#ifdef	CFG_PRINTF
#define printf(fmt,args...)	serial_printf (fmt ,##args)
#define getc() serial_getc()
#else
#define printf(fmt,args...)
#define getc() ' '
#endif	/* CFG_PRINTF */

/* board/$(BOARD)/$(BOARD).c */
int 	board_init (void);
int 	flash_setup(void);
int     mmc_boot (unsigned char *buf);
void	board_hang (void);

/* cpu/$(CPU)/cpu.c */
int 	cpu_init (void);
#ifdef  CFG_UDELAY
void 	udelay (unsigned long usec);
#endif

/* nand driver */
#define NAND_CMD_READ0		0
#define NAND_CMD_READ1		1
#define NAND_CMD_READOOB	0x50
#define NAND_CMD_STATUS		0x70
#define NAND_CMD_READID		0x90
#define NAND_CMD_RESET		0xff
#define NAND_CMD_STATUS_ENH 0x78

/* Extended Commands for Large page devices */
#define NAND_CMD_READSTART	0x30
#define NAND_CMD_CACHE_READ	0x31
#define NAND_CMD_CACHE_END  0x3F

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#ifdef CFG_PRINTF

/* serial driver */
int	serial_init   (void);
void	serial_setbrg (void);
void	serial_putc   (const char);
void	serial_puts   (const char *);
int	serial_getc   (void);
int	serial_tstc   (void);

/* lib/printf.c */
void	serial_printf (const char *fmt, ...);
#endif

/* lib/board.c */
void	hang		(void) __attribute__ ((noreturn));
#endif	/* __COMMON_H_ */
