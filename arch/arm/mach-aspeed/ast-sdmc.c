/*
 * Aspeed SDRAM Memory Controller
 *
 * Copyright (C) 2012-2020  ASPEED Technology Inc.
 * Copyright (c) 2016, IBM Corporation. All rights reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 *   History      :
 *    1. 2013/03/15 Ryan Chen Create
 */

#include <common.h>
#include <asm/processor.h>
#include <asm/io.h>
#include <asm/arch/regs-sdmc.h>
#include <asm/arch/ast-sdmc.h>

/* #define AST_SDMC_LOCK */

static inline u32 ast_sdmc_read(u32 reg)
{
	u32 val = readl(AST_SDMC_BASE + reg);

	debug("ast_sdmc_read : reg = 0x%08x, val = 0x%08x\n", reg, val);
	return val;
}

static inline void ast_sdmc_write(u32 val, u32 reg)
{
	debug("ast_sdmc_write : reg = 0x%08x, val = 0x%08x\n", reg, val);
#ifdef CONFIG_AST_SDMC_LOCK
	writel(SDMC_PROTECT_UNLOCK, AST_SDMC_BASE);
	writel(val, AST_SDMC_BASE + reg);
	writel(0xaa, AST_SDMC_BASE);
#else
	writel(SDMC_PROTECT_UNLOCK, AST_SDMC_BASE);

	writel(val, AST_SDMC_BASE + reg);
#endif
}

u32 ast_sdmc_get_mem_size(void)
{
	u32 size = 0;
	u32 conf = ast_sdmc_read(AST_SDMC_CONFIG);

	switch (SDMC_CONFIG_MEM_GET(conf)) {
	case 0:
		size = 64;
		break;
	case 1:
		size = 128;
		break;
	case 2:
		size = 256;
		break;
	case 3:
		size = 512;
		break;
	default:
		printf("error ddr size\n");
		break;
	}

	if (conf & SDMC_CONFIG_VER_NEW)
		size <<= 1;

	return size * 1024 * 1024;
}

u8 ast_sdmc_get_eec(void)
{
	return ast_sdmc_read(AST_SDMC_CONFIG) & SDMC_CONFIG_EEC_EN;
}

u8 ast_sdmc_get_cache(void)
{
	return ast_sdmc_read(AST_SDMC_CONFIG) & SDMC_CONFIG_CACHE_EN;
}
