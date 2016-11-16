/*
 * Copyright (c) 2016, IBM Corporation.  All rights reserved.
 *
 * SPDX-License-Identifier:	GPL-2.0
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define CONFIG_SYS_LOAD_ADDR		0x83000000

#include <asm/arch/ast_g5_platform.h>
#include <configs/ast-common.h>

/* arm1176/start.S */
#define CONFIG_SYS_UBOOT_BASE		CONFIG_SYS_TEXT_BASE

#endif	/* __CONFIG_H */
