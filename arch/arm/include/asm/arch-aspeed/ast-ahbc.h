/*
 * Aspeed AHB helpers
 *
 * Copyright (C) 2012-2020  ASPEED Technology Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 *   History      :
 *      1. 2014/08/03 Ryan Chen create this file
 */

#ifndef __AST_AHBC_H
#define __AST_AHBC_H

void ast_ahbc_boot_remap(void);

#ifdef CONFIG_TARGET_AST_G5
void ast_ahbc_lpc_plus_mapping(u8 enable);
void ast_ahbc_peie_mapping(u8 enable);
#endif

#endif
