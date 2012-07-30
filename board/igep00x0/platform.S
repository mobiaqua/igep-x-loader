/*
 * Board specific setup info
 *
 * (C) Copyright 2009
 * Integration Software and Electronics Engineering, <www.iseebcn.com>
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

#include <config.h>
#include <asm/arch/cpu.h>
#include <asm/arch/mem.h>
#include <asm/arch/clocks.h>

_TEXT_BASE:
	.word	TEXT_BASE	/* sdram load addr from config.mk */

.globl lowlevel_init
lowlevel_init:
	ldr	sp,	SRAM_STACK
        str     ip,	[sp]    /* stash old link register */
	mov	ip,	lr	/* save link reg across call */
	bl      s_init          /* go setup pll,mux,memory */
        ldr     ip,	[sp]    /* restore save ip */
	mov	lr,	ip	/* restore link reg */

	/* back to arch calling code */
	mov	pc,	lr

	/* the literal pools origin */
	.ltorg

REG_CONTROL_STATUS:
	.word CONTROL_STATUS
SRAM_STACK:
	.word LOW_LEVEL_SRAM_STACK

/* DPLL(1-4) PARAM TABLES */
/* Each of the tables has M, N, FREQSEL, M2 values defined for nominal
 * OPP (1.2V). The fields are defined according to dpll_param struct(clock.c).
 * The values are defined for all possible sysclk and for ES1 and ES2.
 */

mpu_dpll_param:
/* 12MHz */
/* ES1 */
.word 0x0FE
.word 0x07
.word 0x05
.word 0x01
/* ES2 */
.word 0x0FA
.word 0x05
.word 0x07
.word 0x01

/* 13MHz */
/* ES1 */
.word 0x17D
.word 0x0C
.word 0x03
.word 0x01
/* ES2 */
/*.word 0x1F4*/
.word 0x2D0
.word 0x0C
.word 0x03
.word 0x01

/* 19.2MHz */
/* ES1 */
.word 0x179
.word 0x12
.word 0x04
.word 0x01
/* ES2 */
.word 0x271
.word 0x17
.word 0x03
.word 0x01

/* 26MHz */
/* ES1 */
.word 0x17D
.word 0x19
.word 0x03
.word 0x01
/* ES2 */
.word 0x0FA
.word 0x0C
.word 0x07
.word 0x01

/* 38.4MHz */
/* ES1 */
.word 0x1FA
.word 0x32
.word 0x03
.word 0x01
/* ES2 */
.word 0x271
.word 0x2F
.word 0x03
.word 0x01


.globl get_mpu_dpll_param
get_mpu_dpll_param:
	adr r0, mpu_dpll_param
	mov pc, lr

iva_dpll_param:
/* 12MHz */
/* ES1 */
.word 0x07D
.word 0x05
.word 0x07
.word 0x01
/* ES2 */
.word 0x0B4
.word 0x05
.word 0x07
.word 0x01

/* 13MHz */
/* ES1 */
.word 0x0FA
.word 0x0C
.word 0x03
.word 0x01
/* ES2 */
/*.word 0x168*/
.word 0x208
.word 0x0C
.word 0x03
.word 0x01

/* 19.2MHz */
/* ES1 */
.word 0x082
.word 0x09
.word 0x07
.word 0x01
/* ES2 */
.word 0x0E1
.word 0x0B
.word 0x06
.word 0x01

/* 26MHz */
/* ES1 */
.word 0x07D
.word 0x0C
.word 0x07
.word 0x01
/* ES2 */
.word 0x0B4
.word 0x0C
.word 0x07
.word 0x01

/* 38.4MHz */
/* ES1 */
.word 0x13F
.word 0x30
.word 0x03
.word 0x01
/* ES2 */
.word 0x0E1
.word 0x17
.word 0x06
.word 0x01


.globl get_iva_dpll_param
get_iva_dpll_param:
	adr r0, iva_dpll_param
	mov pc, lr

/* Core DPLL targets for L3 at 166 & L133 */
core_dpll_param:
/* 12MHz */
/* ES1 */
.word 0x19F
.word 0x0E
.word 0x03
.word 0x01
/* ES2 */
.word 0x0A6
.word 0x05
.word 0x07
.word 0x01

/* 13MHz */
/* ES1 */
.word 0x1B2
.word 0x10
.word 0x03
.word 0x01
/* ES2 */
.word 0x14C
.word 0x0C
.word 0x03
.word 0x01

/* 19.2MHz */
/* ES1 */
.word 0x19F
.word 0x17
.word 0x03
.word 0x01
/* ES2 */
.word 0x19F
.word 0x17
.word 0x03
.word 0x01

/* 26MHz */
/* ES1 */
.word 0x1B2
.word 0x21
.word 0x03
.word 0x01
/* ES2 */
.word 0x0A6
.word 0x0C
.word 0x07
.word 0x01

/* 38.4MHz */
/* ES1 */
.word 0x19F
.word 0x2F
.word 0x03
.word 0x01
/* ES2 */
.word 0x19F
.word 0x2F
.word 0x03
.word 0x01

.globl get_core_dpll_param
get_core_dpll_param:
	adr r0, core_dpll_param
	mov pc, lr

/* PER DPLL values are same for both ES1 and ES2 */
per_dpll_param:
/* 12MHz */
.word 0xD8
.word 0x05
.word 0x07
.word 0x09

/* 13MHz */
.word 0x1B0
.word 0x0C
.word 0x03
.word 0x09

/* 19.2MHz */
.word 0xE1
.word 0x09
.word 0x07
.word 0x09

/* 26MHz */
.word 0xD8
.word 0x0C
.word 0x07
.word 0x09

/* 38.4MHz */
.word 0xE1
.word 0x13
.word 0x07
.word 0x09

.globl get_per_dpll_param
get_per_dpll_param:
	adr r0, per_dpll_param
	mov pc, lr

/*
 * Tables for 36x/37x devices
 */
mpu_36x_dpll_param:
.word 500, 12, 0, 1
.word 500, 12, 0, 1
.word 500, 12, 0, 1
.word 500, 12, 0, 1

iva_36x_dpll_param:
.word 10, 12, 0, 1
.word 10, 12, 0, 1
.word 10, 12, 0, 1
.word 10, 12, 0, 1

core_36x_dpll_param:
.word 200, 12, 0, 1
.word 200, 12, 0, 1
.word 200, 12, 0, 1
.word 200, 12, 0, 1

per_36x_dpll_param:
/*    SYSCLK    M       N       M2      M3      M4      M5      M6      m2DIV */
.word 26000,    432,    12,     9,      16,     9,      4,      3,      1
.word 26000,    432,    12,     9,      16,     9,      4,      3,      1
.word 26000,    432,    12,     9,      16,     9,      4,      3,      1
.word 26000,    432,    12,     9,      16,     9,      4,      3,      1

.globl get_36x_mpu_dpll_param
get_36x_mpu_dpll_param:
	adr	r0, mpu_36x_dpll_param
	mov	pc, lr

.globl get_36x_iva_dpll_param
get_36x_iva_dpll_param:
	adr	r0, iva_36x_dpll_param
	mov	pc, lr

.globl get_36x_core_dpll_param
get_36x_core_dpll_param:
	adr	r0, core_36x_dpll_param
	mov	pc, lr

.globl get_36x_per_dpll_param
get_36x_per_dpll_param:
	adr	r0, per_36x_dpll_param
 	mov	pc, lr

