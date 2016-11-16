/*
 * Aspeed AHB Controller
 *
 * Copyright (C) 2012-2020  ASPEED Technology Inc.
 * Copyright (c) 2016, IBM Corporation. All rights reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 *   History      :
 *    1. 2014/03/15 Ryan Chen Create
 */
#include <common.h>
#include <asm/processor.h>
#include <asm/io.h>
#include <asm/arch/regs-ahbc.h>
#include <asm/arch/ast-ahbc.h>

static inline u32 ast_ahbc_read(u32 reg)
{
	u32 val = readl(AST_AHBC_BASE + reg);

	debug("reg = 0x%08x, val = 0x%08x\n", reg, val);
	return val;
}

static inline void ast_ahbc_write(u32 val, u32 reg)
{
	debug("reg = 0x%08x, val = 0x%08x\n", reg, val);

#ifdef CONFIG_AST_AHBC_LOCK
	writel(AHBC_PROTECT_UNLOCK, AST_AHBC_BASE);
	writel(val, AST_AHBC_BASE + reg);
	writel(0xaa, AST_AHBC_BASE);
#else
	writel(AHBC_PROTECT_UNLOCK, AST_AHBC_BASE);
	writel(val, AST_AHBC_BASE + reg);
#endif
}

void ast_ahbc_boot_remap(void)
{
#if !defined(CONFIG_TARGET_AST_G5)
	ast_ahbc_write(ast_ahbc_read(AST_AHBC_ADDR_REMAP) |
		       AHBC_BOOT_REMAP, AST_AHBC_ADDR_REMAP);
#endif
}

#ifdef CONFIG_TARGET_AST_G5
void ast_ahbc_peie_mapping(u8 enable)
{
	if (enable)
		ast_ahbc_write(ast_ahbc_read(AST_AHBC_ADDR_REMAP) |
			       AHBC_PCIE_MAP, AST_AHBC_ADDR_REMAP);
	else
		ast_ahbc_write(ast_ahbc_read(AST_AHBC_ADDR_REMAP) &
			       ~AHBC_PCIE_MAP, AST_AHBC_ADDR_REMAP);
}

void ast_ahbc_lpc_plus_mapping(u8 enable)
{
	if (enable)
		ast_ahbc_write(ast_ahbc_read(AST_AHBC_ADDR_REMAP) |
			       AHBC_LPC_PLUS_MAP, AST_AHBC_ADDR_REMAP);
	else
		ast_ahbc_write(ast_ahbc_read(AST_AHBC_ADDR_REMAP) &
			       ~AHBC_LPC_PLUS_MAP, AST_AHBC_ADDR_REMAP);
}
#endif
