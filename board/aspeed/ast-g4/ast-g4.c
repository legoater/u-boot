/*
 * (C) Copyright 2002 Ryan Chen
 * Copyright 2016 IBM Corporation
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <netdev.h>
#include <asm/arch/platform.h>
#include <asm/arch/ast-sdmc.h>
#include <asm/arch/regs-ahbc.h>
#include <asm/arch/regs-scu.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

#if defined(CONFIG_SHOW_BOOT_PROGRESS)
void show_boot_progress(int progress)
{
    printf("Boot reached stage %d\n", progress);
}
#endif

int board_init(void)
{
	/* The BSP did this in the cpu code */
	icache_enable();
	dcache_enable();

	/* adress of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;
	gd->flags = 0;
	return 0;
}

int misc_init_r(void)
{
	u32 reg;

	/* Unlock AHB controller */
	writel(AHBC_PROTECT_UNLOCK, AST_AHBC_BASE);

	/* Map DRAM to 0x00000000 */
	reg = readl(AST_AHBC_BASE + AST_AHBC_ADDR_REMAP);
	writel(reg | BIT(0), AST_AHBC_BASE + AST_AHBC_ADDR_REMAP);

	/* Unlock SCU */
	writel(SCU_PROTECT_UNLOCK, AST_SCU_BASE);

	/*
	 * The original file contained these comments.
	 * TODO: verify the register write does what it claims
	 *
	 * LHCLK = HPLL/8
	 * PCLK  = HPLL/8
	 * BHCLK = HPLL/8
	 */
	reg = readl(AST_SCU_BASE + AST_SCU_CLK_SEL);
	reg &= 0x1c0fffff;
	reg |= 0x61800000;
	writel(reg, AST_SCU_BASE + AST_SCU_CLK_SEL);

	return 0;
}

int dram_init(void)
{
	/* dram_init must store complete ramsize in gd->ram_size */
	gd->ram_size = ast_sdmc_get_mem_size();

	return 0;
}

int board_eth_init(bd_t *bd)
{
	return aspeednic_initialize(bd);
}
