// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2018, IBM Corporation.
 */

#ifndef _ASM_ARCH_ASPEED_SPI_H
#define _ASM_ARCH_ASPEED_SPI_H

/* CE Type Setting Register */
#define CONF_ENABLE_W2			BIT(18)
#define CONF_ENABLE_W1			BIT(17)
#define CONF_ENABLE_W0			BIT(16)
#define CONF_FLASH_TYPE2		4
#define CONF_FLASH_TYPE1		2	/* Hardwired to SPI */
#define CONF_FLASH_TYPE0		0	/* Hardwired to SPI */
#define	  CONF_FLASH_TYPE_NOR		0x0
#define	  CONF_FLASH_TYPE_SPI		0x2

/* CE Control Register */
#define CTRL_EXTENDED2			BIT(2)	/* 32 bit addressing for SPI */
#define CTRL_EXTENDED1			BIT(1)	/* 32 bit addressing for SPI */
#define CTRL_EXTENDED0			BIT(0)	/* 32 bit addressing for SPI */

/* Interrupt Control and Status Register */
#define INTR_CTRL_DMA_STATUS		BIT(11)
#define INTR_CTRL_CMD_ABORT_STATUS	BIT(10)
#define INTR_CTRL_WRITE_PROTECT_STATUS	BIT(9)
#define INTR_CTRL_DMA_EN		BIT(3)
#define INTR_CTRL_CMD_ABORT_EN		BIT(2)
#define INTR_CTRL_WRITE_PROTECT_EN	BIT(1)

/* CEx Control Register */
#define CE_CTRL_IO_MODE_MASK		GENMASK(30, 28)
#define CE_CTRL_IO_DUAL_DATA		BIT(29)
#define CE_CTRL_IO_DUAL_ADDR_DATA	(BIT(29) | BIT(28))
#define CE_CTRL_CMD_SHIFT		16
#define CE_CTRL_CMD_MASK		0xff
#define CE_CTRL_CMD(cmd)					\
	(((cmd) & CE_CTRL_CMD_MASK) << CE_CTRL_CMD_SHIFT)
#define CE_CTRL_DUMMY_HIGH_SHIFT	14
#define CE_CTRL_DUMMY_HIGH_MASK		0x1
#define CE_CTRL_CLOCK_FREQ_SHIFT	8
#define CE_CTRL_CLOCK_FREQ_MASK		0xf
#define CE_CTRL_CLOCK_FREQ(div)						\
	(((div) & CE_CTRL_CLOCK_FREQ_MASK) << CE_CTRL_CLOCK_FREQ_SHIFT)
#define CE_CTRL_DUMMY_LOW_SHIFT		6 /* 2 bits [7:6] */
#define CE_CTRL_DUMMY_LOW_MASK		0x3
#define CE_CTRL_DUMMY(dummy)						\
	(((((dummy) >> 2) & CE_CTRL_DUMMY_HIGH_MASK)			\
	  << CE_CTRL_DUMMY_HIGH_SHIFT) |				\
	 (((dummy) & CE_CTRL_DUMMY_LOW_MASK) << CE_CTRL_DUMMY_LOW_SHIFT))
#define CE_CTRL_STOP_ACTIVE		BIT(2)
#define CE_CTRL_MODE_MASK		0x3
#define	  CE_CTRL_READMODE		0x0
#define	  CE_CTRL_FREADMODE		0x1
#define	  CE_CTRL_WRITEMODE		0x2
#define	  CE_CTRL_USERMODE		0x3

/*
 * The Segment Register uses a 8MB unit to encode the start address
 * and the end address of the AHB window of a SPI flash device.
 * Default segment addresses are :
 *
 *   CE0  0x20000000 - 0x2fffffff  128MB
 *   CE1  0x28000000 - 0x29ffffff   32MB
 *   CE2  0x2a000000 - 0x2bffffff   32MB
 *
 * The full address space of the AHB window of the controller is
 * covered and CE0 start address and CE2 end addresses are read-only.
 */
#define SEGMENT_ADDR_START(reg)		((((reg) >> 16) & 0xff) << 23)
#define SEGMENT_ADDR_END(reg)		((((reg) >> 24) & 0xff) << 23)
#define SEGMENT_ADDR_VALUE(start, end)					\
	(((((start) >> 23) & 0xff) << 16) | ((((end) >> 23) & 0xff) << 24))

/* DMA Control/Status Register */
#define DMA_CTRL_DELAY_SHIFT		8
#define DMA_CTRL_DELAY_MASK		0xf
#define DMA_CTRL_FREQ_SHIFT		4
#define DMA_CTRL_FREQ_MASK		0xf
#define TIMING_MASK(div, delay)					   \
	(((delay & DMA_CTRL_DELAY_MASK) << DMA_CTRL_DELAY_SHIFT) | \
	 ((div & DMA_CTRL_FREQ_MASK) << DMA_CTRL_FREQ_SHIFT))
#define DMA_CTRL_CALIB			BIT(3)
#define DMA_CTRL_CKSUM			BIT(2)
#define DMA_CTRL_WRITE			BIT(1)
#define DMA_CTRL_ENABLE			BIT(0)

#define ASPEED_SPI_MAX_CS		3

struct aspeed_spi_regs {
	u32 conf;			/* 0x00 CE Type Setting */
	u32 ctrl;			/* 0x04 Control */
	u32 intr_ctrl;			/* 0x08 Interrupt Control and Status */
	u32 cmd_ctrl;			/* 0x0c Command Control */
	u32 ce_ctrl[ASPEED_SPI_MAX_CS];	/* 0x10 .. 0x18 CEx Control */
	u32 _reserved0[5];		/* .. */
	u32 segment_addr[ASPEED_SPI_MAX_CS];
					/* 0x30 .. 0x38 Segment Address */
	u32 _reserved1[17];		/* .. */
	u32 dma_ctrl;			/* 0x80 DMA Control/Status */
	u32 dma_flash_addr;		/* 0x84 DMA Flash Side Address */
	u32 dma_dram_addr;		/* 0x88 DMA DRAM Side Address */
	u32 dma_len;			/* 0x8c DMA Length Register */
	u32 dma_checksum;		/* 0x90 Checksum Calculation Result */
	u32 timings;			/* 0x94 Read Timing Compensation */

	/* not used */
	u32 soft_strap_status;		/* 0x9c Software Strap Status */
	u32 write_cmd_filter_ctrl;	/* 0xa0 Write Command Filter Control */
	u32 write_addr_filter_ctrl;	/* 0xa4 Write Address Filter Control */
	u32 lock_ctrl_reset;		/* 0xa8 Lock Control (SRST#) */
	u32 lock_ctrl_wdt;		/* 0xac Lock Control (Watchdog) */
	u32 write_addr_filter[5];	/* 0xb0 Write Address Filter */
};

#endif	/* _ASM_ARCH_ASPEED_SPI_H */
