/*
 * Copyright (C) 2012-2020  ASPEED Technology Inc.
 * Copyright (c) 2016, IBM Corporation. All rights reserved.
 *
 * Ryan Chen <ryan_chen@aspeedtech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/io.h>

#if CONFIG_TIMER_CLK_FREQ < CONFIG_SYS_HZ
#error "CONFIG_TIMER_CLK_FREQ must be as large as CONFIG_SYS_HZ"
#endif

#define TIMER_LOAD_VAL 0xffffffff
#define CLK_PER_HZ (CONFIG_TIMER_CLK_FREQ / CONFIG_SYS_HZ)

/* Clock selection for Timer/Counter #1
 * Counter is base on the selected clock to count down
 *
 * 0: APB clock (PCLK)
 * 1: External clock (1 MHz)
 */
#define TIMER_CLK_SELECT	0x2
/* Timer enable for Timer/Counter #1 */
#define TIMER_ENABLE		0x1

/* macro to read the 32 bit timer */
#define READ_CLK		readl(AST_TIMER_BASE)
#define READ_TIMER		(READ_CLK / CLK_PER_HZ)

DECLARE_GLOBAL_DATA_PTR;

#define timestamp	(gd->arch.tbl)
#define lastdec		(gd->arch.lastinc)

int timer_init(void)
{
	writel(TIMER_LOAD_VAL, AST_TIMER_BASE + 0x4);
	writel(TIMER_ENABLE|TIMER_CLK_SELECT, AST_TIMER_BASE + 0x30);

	/* init the timestamp and lastdec value */
	reset_timer_masked();

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

/* delay x useconds AND perserve advance timstamp value */
void __udelay(unsigned long usec)
{
	ulong last = READ_CLK;
	ulong clks;
	ulong elapsed = 0;

	/* translate usec to clocks */
	clks = (usec / 1000) * CLK_PER_HZ;
	clks += (usec % 1000) * CLK_PER_HZ / 1000;

	while (clks > elapsed) {
		ulong now = READ_CLK;
		if (now <= last)
			elapsed += last - now;
		else
			elapsed += TIMER_LOAD_VAL - (now - last);
		last = now;
	}
}

void reset_timer_masked(void)
{
	/* reset time */
	lastdec = READ_TIMER;  /* capure current decrementer value time */
	timestamp = 0;	       /* start "advancing" time stamp from 0 */
}

ulong get_timer_masked(void)
{
	ulong now = READ_TIMER;	/* current tick value */

	if (lastdec >= now) {	/* normal mode (non roll) */
		 /* move stamp fordward with absolute diff ticks */
		timestamp += lastdec - now;
	} else { /* we have overflow of the count down timer */

		/* nts = ts + ld + (TLV - now)
		 * ts=old stamp, ld=time that passed before passing through -1
		 * (TLV-now) amount of time after passing though -1
		 * nts = new "advancing time stamp"... it could also roll and
		 * cause problems.
		 */
		timestamp += lastdec + (TIMER_LOAD_VAL / CLK_PER_HZ) - now;
	}
	lastdec = now;

	return timestamp;
}

/* waits specified delay value and resets timestamp */
void udelay_masked(unsigned long usec)
{
	__udelay(usec);
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
