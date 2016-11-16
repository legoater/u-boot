/*
 * Copyright (c) 2016, IBM Corporation.  All rights reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#include <common.h>
#include <asm/io.h>

void enable_caches(void)
{
	icache_enable();
	/* TODO */
	dcache_enable();
}
