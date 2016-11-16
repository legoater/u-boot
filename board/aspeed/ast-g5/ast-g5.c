/*
 * Copyright 2016 IBM Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <common.h>
#include <netdev.h>

#include <asm/arch/ast-scu.h>
#include <asm/arch/ast-sdmc.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;
	gd->flags = 0;

	return 0;
}

int misc_init_r(void)
{
#ifdef CONFIG_AST_DISABLE_WDT2
	writel(0, AST_WDT_BASE + 0x2C);
#endif
	return 0;
}

int dram_init(void)
{
	u32 vga = ast_scu_get_vga_memsize();
	u32 dram = ast_sdmc_get_mem_size();
	gd->ram_size = dram - vga;

	return 0;
}
