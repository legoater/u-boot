/*
 * Aspeed AHB registers
 *
 * Copyright (C) 2012-2020  ASPEED Technology Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0
 *
 *   History	  :
 *    1. 2012/12/29 Ryan Chen Create
 */
#ifndef __AST_AHBC_H
#define __AST_AHBC_H

/* Registers for AHBC */
#define AST_AHBC_PROTECT		0x00	/* Protection Key Register */
#define AST_AHBC_PRIORITY_CTRL		0x80	/* Priority Cortrol Register */
#define AST_AHBC_ADDR_REMAP		0x8C	/* Address Remapping Register */

/* AST_AHBC_PROTECT	0x00		Protection Key Register		*/
#define AHBC_PROTECT_UNLOCK		0xAEED1A03

/* AST_AHBC_ADDR_REMAP	0x8C		Address Remapping Register */
#define AHBC_PCI_REMAP1			(1 << 5)
#define AHBC_PCI_REMAP0			(1 << 4)

#if defined(CONFIG_TARGET_AST_G5)
#define AHBC_PCIE_MAP			(1 << 5)
#define AHBC_LPC_PLUS_MAP		(1 << 4)
#else
#define AHBC_BOOT_REMAP			1
#endif


#endif
