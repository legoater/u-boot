/*
 * Aspeed SCU helpers
 *
 * Copyright (C) 2012-2020  ASPEED Technology Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 *   History      :
 *      1. 2012/08/03 Ryan Chen create this file
 */

#ifndef __AST_SCU_H
#define __AST_SCU_H

void ast_scu_show_system_info(void);
void ast_scu_sys_rest_info(void);
void ast_scu_security_info(void);
u32 ast_scu_revision_id(void);
u32 ast_scu_get_vga_memsize(void);
void ast_scu_get_who_init_dram(void);

u32 ast_get_clk_source(void);
u32 ast_get_h_pll_clk(void);
u32 ast_get_m_pll_clk(void);
u32 ast_get_ahbclk(void);

u32 ast_scu_get_vga_memsize(void);

void ast_scu_init_eth(u8 num);
void ast_scu_multi_func_eth(u8 num);
void ast_scu_multi_func_romcs(u8 num);

#endif
