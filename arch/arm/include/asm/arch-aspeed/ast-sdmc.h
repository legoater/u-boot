/*
 * Aspeed SDMC helpers
 *
 * Copyright (C) 2012-2020  ASPEED Technology Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 *   History      :
 *      1. 2012/08/03 Ryan Chen create this file
 */

#ifndef __AST_SDMC_H
#define __AST_SDMC_H

u32 ast_sdmc_get_mem_size(void);
u8 ast_sdmc_get_eec(void);
u8 ast_sdmc_get_cache(void);

#endif
