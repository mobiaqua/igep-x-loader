/*
 * (C) Copyright 2008
 * Texas Instruments
 *
 * Richard Woodruff <r-woodruff2@ti.com>
 * Syed Moahmmed Khasim <khasim@ti.com>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 * Alex Zuepke <azu@sysgo.de>
 *
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <garyj@denx.de>
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
#include <asm/io.h>

 struct gptimer {
    u32 tidr;       /* 0x00 r */
    u8 res[0xc];
    u32 tiocp_cfg;  /* 0x10 rw */
    u32 tistat;     /* 0x14 r */
    u32 tisr;       /* 0x18 rw */
    u32 tier;       /* 0x1c rw */
    u32 twer;       /* 0x20 rw */
    u32 tclr;       /* 0x24 rw */
    u32 tcrr;       /* 0x28 rw */
    u32 tldr;       /* 0x2c rw */
    u32 ttgr;       /* 0x30 rw */
    u32 twpc;       /* 0x34 r*/
    u32 tmar;       /* 0x38 rw*/
    u32 tcar1;      /* 0x3c r */
    u32 tcicr;      /* 0x40 rw */
    u32 tcar2;      /* 0x44 r */
 };

 #define TCLR_ST                 (0x1 << 0)
 #define TCLR_AR                 (0x1 << 1)
 #define TCLR_PRE                (0x1 << 5)

static ulong timestamp;
static ulong lastinc;
static struct gptimer *timer_base = (struct gptimer *) CONFIG_SYS_TIMERBASE;

ulong get_timer_masked(void);

/*
 * Nothing really to do with interrupts, just starts up a counter.
 */

#define TIMER_CLOCK	(V_SCLK / (2 << CONFIG_SYS_PTV))
#define TIMER_LOAD_VAL	0xffffffff

int timer_init(void)
{
	/* start the counter ticking up, reload value on overflow */
	writel(TIMER_LOAD_VAL, &timer_base->tldr);
	/* enable timer */
	writel((CONFIG_SYS_PTV << 2) | TCLR_PRE | TCLR_AR | TCLR_ST,
		&timer_base->tclr);

	reset_timer_masked();	/* init the timestamp and lastinc value */

	return 0;
}

/*
 * timer without interrupts
 */
void reset_timer(void)
{
	reset_timer_masked();
}

ulong get_timer(ulong base)
{
	return get_timer_masked() - base;
}

void set_timer(ulong t)
{
	timestamp = t;
}

/* delay x useconds */
void __udelay(unsigned long usec)
{
	long tmo = usec * (TIMER_CLOCK / 1000) / 1000;
	unsigned long now, last = readl(&timer_base->tcrr);

	while (tmo > 0) {
		now = readl(&timer_base->tcrr);
		if (last > now) /* count up timer overflow */
			tmo -= TIMER_LOAD_VAL - last + now;
		else
			tmo -= now - last;
		last = now;
	}
}

void reset_timer_masked(void)
{
	/* reset time, capture current incrementer value time */
	lastinc = readl(&timer_base->tcrr) / (TIMER_CLOCK / CONFIG_SYS_HZ);
	timestamp = 0;		/* start "advancing" time stamp from 0 */
}

ulong get_timer_masked(void)
{
	/* current tick value */
	ulong now = readl(&timer_base->tcrr) / (TIMER_CLOCK / CONFIG_SYS_HZ);

	if (now >= lastinc)	/* normal mode (non roll) */
		/* move stamp fordward with absoulte diff ticks */
		timestamp += (now - lastinc);
	else	/* we have rollover of incrementer */
		timestamp += ((TIMER_LOAD_VAL / (TIMER_CLOCK / CONFIG_SYS_HZ))
				- lastinc) + now;
	lastinc = now;
	return timestamp;
}

/*
 * This function is derived from PowerPC code (read timebase as long long).
 * On ARM it just returns the timer value.
 */
unsigned long long get_ticks(void)
{
	return get_timer(0);
}

/*
 * This function is derived from PowerPC code (timebase clock frequency).
 * On ARM it returns the number of timer ticks per second.
 */
ulong get_tbclk(void)
{
	return CONFIG_SYS_HZ;
}
