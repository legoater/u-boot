// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2016 Google, Inc
 */
#include <common.h>

#include <asm/arch/scu_ast2500.h>
#include <linux/err.h>

void board_quiesce_devices(void)
{
	struct ast2500_scu *scu = ast_get_scu();
	if (IS_ERR(scu)) {
		printf("Could not unlock SCU, Linux will not work\n");
		return;
	}

	ast_scu_unlock(scu);
}
