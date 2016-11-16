/*
 * Copyright (c) 2016, IBM Corporation. All rights reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/io.h>

void reset_cpu(ulong addr)
{
	__raw_writel(0x10 , AST_WDT_BASE+0x04);
	__raw_writel(0x4755, AST_WDT_BASE+0x08);
	__raw_writel(0x3, AST_WDT_BASE+0x0c);

	while (1)
		;
}
