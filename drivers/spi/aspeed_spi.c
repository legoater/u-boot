// SPDX-License-Identifier: GPL-2.0+
/*
 * ASPEED AST2500 FMC/SPI Controller driver
 *
 * Copyright (c) 2015-2018, IBM Corporation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <spi.h>
#include <spi_flash.h>
#include <asm/io.h>
#include <asm/arch/spi.h>
#include <linux/ioport.h>

/*
 *
 */
struct aspeed_spi_flash {
	u8		cs;
	bool		init;		/* Initialized when the SPI bus is
					 * first claimed
					 */
	void __iomem	*ahb_base;	/* AHB Window for this device */
	u32		ahb_size;	/* AHB Window segment size */
	u32		ce_ctrl_user;	/* CE Control Register for USER mode */
	u32		ce_ctrl_fread;	/* CE Control Register for FREAD mode */
	u32		iomode;

	struct spi_flash *spi;		/* Associated SPI Flash device */
};

struct aspeed_spi_priv {
	struct aspeed_spi_regs	*regs;
	void __iomem	*ahb_base;	/* AHB Window for all flash devices */
	u32		ahb_size;	/* AHB Window segments size */

	ulong		hclk_rate;	/* AHB clock rate */
	u32		max_hz;
	u8		num_cs;
	bool		is_fmc;

	struct aspeed_spi_flash flashes[ASPEED_SPI_MAX_CS];
	u32		flash_count;

	u8		cmd_buf[16];	/* SPI command in progress */
	size_t		cmd_len;
};

static struct aspeed_spi_flash *aspeed_spi_get_flash(struct udevice *dev)
{
	struct dm_spi_slave_platdata *slave_plat = dev_get_parent_platdata(dev);
	struct aspeed_spi_priv *priv = dev_get_priv(dev->parent);
	u8 cs = slave_plat->cs;

	if (cs >= priv->flash_count) {
		pr_err("invalid CS %u\n", cs);
		return NULL;
	}

	return &priv->flashes[cs];
}

static u32 aspeed_spi_hclk_divisor(u32 hclk_rate, u32 max_hz)
{
	/* HCLK/1 ..	HCLK/16 */
	const u8 hclk_masks[] = {
		15, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 0
	};

	u32 i;

	for (i = 0; i < ARRAY_SIZE(hclk_masks); i++) {
		if (max_hz >= (hclk_rate / (i + 1)))
			break;
	}

	debug("hclk=%d required=%d divisor is %d (mask %x) speed=%d\n",
	      hclk_rate, max_hz, i + 1, hclk_masks[i], hclk_rate / (i + 1));

	return hclk_masks[i];
}

/*
 * Use some address/size under the first flash device CE0
 */
static u32 aspeed_spi_fmc_checksum(struct aspeed_spi_priv *priv, u8 div,
				   u8 delay)
{
	u32 flash_addr = (u32)priv->ahb_base + 0x10000;
	u32 flash_len = 0x200;
	u32 dma_ctrl;
	u32 checksum;

	writel(flash_addr, &priv->regs->dma_flash_addr);
	writel(flash_len,  &priv->regs->dma_len);

	/*
	 * When doing calibration, the SPI clock rate in the CE0
	 * Control Register and the data input delay cycles in the
	 * Read Timing Compensation Register are replaced by bit[11:4].
	 */
	dma_ctrl = DMA_CTRL_ENABLE | DMA_CTRL_CKSUM | DMA_CTRL_CALIB |
		TIMING_MASK(div, delay);
	writel(dma_ctrl, &priv->regs->dma_ctrl);

	while (!(readl(&priv->regs->intr_ctrl) & INTR_CTRL_DMA_STATUS))
		;

	writel(0x0, &priv->regs->intr_ctrl);

	checksum = readl(&priv->regs->dma_checksum);

	writel(0x0, &priv->regs->dma_ctrl);

	return checksum;
}

static u32 aspeed_spi_read_checksum(struct aspeed_spi_priv *priv, u8 div,
				    u8 delay)
{
	/* TODO(clg@kaod.org): the SPI controllers do not have the DMA
	 * registers. The algorithm is the same.
	 */
	if (!priv->is_fmc) {
		pr_warn("No timing calibration support for SPI controllers");
		return 0xbadc0de;
	}

	return aspeed_spi_fmc_checksum(priv, div, delay);
}

#define TIMING_DELAY_DI_4NS         BIT(3)
#define TIMING_DELAY_HCYCLE_MAX     5

static int aspeed_spi_timing_calibration(struct aspeed_spi_priv *priv)
{
	/* HCLK/5 .. HCLK/1 */
	const u8 hclk_masks[] = { 13, 6, 14, 7, 15 };

	u32 timing_reg = 0;
	u32 checksum, gold_checksum;
	int i, hcycle;

	debug("Read timing calibration :\n");

	/* Compute reference checksum at lowest freq HCLK/16 */
	gold_checksum = aspeed_spi_read_checksum(priv, 0, 0);

	/*
	 * Set CE0 Control Register to FAST READ command mode. The
	 * HCLK divisor will be set through the DMA Control Register.
	 */
	writel(CE_CTRL_CMD(0xb) | CE_CTRL_DUMMY(1) | CE_CTRL_FREADMODE,
	       &priv->regs->ce_ctrl[0]);

	/* Increase HCLK freq */
	for (i = 0; i < ARRAY_SIZE(hclk_masks); i++) {
		u32 hdiv = 5 - i;
		u32 hshift = (hdiv - 1) << 2;
		bool pass = false;
		u8 delay;

		if (priv->hclk_rate / hdiv > priv->max_hz) {
			debug("skipping freq %ld\n", priv->hclk_rate / hdiv);
			continue;
		}

		/* Increase HCLK cycles until read succeeds */
		for (hcycle = 0; hcycle <= TIMING_DELAY_HCYCLE_MAX; hcycle++) {
			/* Try first with a 4ns DI delay */
			delay = TIMING_DELAY_DI_4NS | hcycle;
			checksum = aspeed_spi_read_checksum(priv, hclk_masks[i],
							    delay);
			pass = (checksum == gold_checksum);
			debug(" HCLK/%d, 4ns DI delay, %d HCLK cycle : %s\n",
			      hdiv, hcycle, pass ? "PASS" : "FAIL");

			/* Try again with more HCLK cycles */
			if (!pass)
				continue;

			/* Try without the 4ns DI delay */
			delay = hcycle;
			checksum = aspeed_spi_read_checksum(priv, hclk_masks[i],
							    delay);
			pass = (checksum == gold_checksum);
			debug(" HCLK/%d,  no DI delay, %d HCLK cycle : %s\n",
			      hdiv, hcycle, pass ? "PASS" : "FAIL");

			/* All good for this freq  */
			if (pass)
				break;
		}

		if (pass) {
			timing_reg &= ~(0xfu << hshift);
			timing_reg |= delay << hshift;
		}
	}

	debug("Read Timing Compensation set to 0x%08x\n", timing_reg);
	writel(timing_reg, &priv->regs->timings);

	/* Reset CE0 Control Register */
	writel(0x0, &priv->regs->ce_ctrl[0]);

	return 0;
}

static int aspeed_spi_controller_init(struct aspeed_spi_priv *priv)
{
	int cs, ret;

	/*
	 * Enable write on all flash devices as USER command mode
	 * requires it.
	 */
	setbits_le32(&priv->regs->conf,
		     CONF_ENABLE_W2 | CONF_ENABLE_W1 | CONF_ENABLE_W0);

	/*
	 * Set the Read Timing Compensation Register. This setting
	 * applies to all devices.
	 */
	ret = aspeed_spi_timing_calibration(priv);
	if (ret)
		return ret;

	/*
	 * Set safe default settings for each device. These will be
	 * tuned after the SPI flash devices are probed.
	 */
	for (cs = 0; cs < priv->flash_count; cs++) {
		struct aspeed_spi_flash *flash = &priv->flashes[cs];
		u32 seg_addr = readl(&priv->regs->segment_addr[cs]);

		/*
		 * The start address of the AHB window of CE0 is
		 * read-only and is the same as the address of the
		 * overall AHB window of the controller for all flash
		 * devices.
		 */
		flash->ahb_base = cs ? (void *)SEGMENT_ADDR_START(seg_addr) :
			priv->ahb_base;

		flash->cs = cs;
		flash->ce_ctrl_user = CE_CTRL_USERMODE;
		flash->ce_ctrl_fread = CE_CTRL_READMODE;
	}

	return 0;
}

static int aspeed_spi_read_from_ahb(void __iomem *ahb_base, void *buf,
				    size_t len)
{
	size_t offset = 0;

	if (!((uintptr_t)buf % 4)) {
		readsl(ahb_base, buf, len >> 2);
		offset = len & ~0x3;
		len -= offset;
	}
	readsb(ahb_base, (u8 *)buf + offset, len);

	return 0;
}

static int aspeed_spi_write_to_ahb(void __iomem *ahb_base, const void *buf,
				   size_t len)
{
	size_t offset = 0;

	if (!((uintptr_t)buf % 4)) {
		writesl(ahb_base, buf, len >> 2);
		offset = len & ~0x3;
		len -= offset;
	}
	writesb(ahb_base, (u8 *)buf + offset, len);

	return 0;
}

static void aspeed_spi_start_user(struct aspeed_spi_priv *priv,
				  struct aspeed_spi_flash *flash)
{
	u32 ctrl_reg = flash->ce_ctrl_user | CE_CTRL_STOP_ACTIVE;

	/* Deselect CS and set USER command mode */
	writel(ctrl_reg, &priv->regs->ce_ctrl[flash->cs]);

	/* Select CS */
	clrbits_le32(&priv->regs->ce_ctrl[flash->cs], CE_CTRL_STOP_ACTIVE);
}

static void aspeed_spi_stop_user(struct aspeed_spi_priv *priv,
				 struct aspeed_spi_flash *flash)
{
	/* Deselect CS first */
	setbits_le32(&priv->regs->ce_ctrl[flash->cs], CE_CTRL_STOP_ACTIVE);

	/* Restore default command mode */
	writel(flash->ce_ctrl_fread, &priv->regs->ce_ctrl[flash->cs]);
}

static int aspeed_spi_read_reg(struct aspeed_spi_priv *priv,
			       struct aspeed_spi_flash *flash,
			       u8 opcode, u8 *read_buf, int len)
{
	aspeed_spi_start_user(priv, flash);
	aspeed_spi_write_to_ahb(flash->ahb_base, &opcode, 1);
	aspeed_spi_read_from_ahb(flash->ahb_base, read_buf, len);
	aspeed_spi_stop_user(priv, flash);

	return 0;
}

static int aspeed_spi_write_reg(struct aspeed_spi_priv *priv,
				struct aspeed_spi_flash *flash,
				u8 opcode, const u8 *write_buf, int len)
{
	aspeed_spi_start_user(priv, flash);
	aspeed_spi_write_to_ahb(flash->ahb_base, &opcode, 1);
	aspeed_spi_write_to_ahb(flash->ahb_base, write_buf, len);
	aspeed_spi_stop_user(priv, flash);

	return 0;
}

static void aspeed_spi_send_cmd_addr(struct aspeed_spi_priv *priv,
				     struct aspeed_spi_flash *flash,
				     const u8 *cmdbuf, unsigned int cmdlen)
{
	int i;
	u8 byte0 = 0x0;
	u8 addrlen = cmdlen - 1;

	/* First, send the opcode */
	aspeed_spi_write_to_ahb(flash->ahb_base, &cmdbuf[0], 1);

	/*
	 * The controller is configured for 4BYTE address mode. Fix
	 * the address width and send an extra byte if the SPI Flash
	 * layer uses 3 bytes addresses.
	 */
	if (addrlen == 3 && readl(&priv->regs->ctrl) & BIT(flash->cs))
		aspeed_spi_write_to_ahb(flash->ahb_base, &byte0, 1);

	/* Then the address */
	for (i = 1 ; i < cmdlen; i++)
		aspeed_spi_write_to_ahb(flash->ahb_base, &cmdbuf[i], 1);
}

static ssize_t aspeed_spi_read_user(struct aspeed_spi_priv *priv,
				    struct aspeed_spi_flash *flash,
				    unsigned int cmdlen, const u8 *cmdbuf,
				    unsigned int len, u8 *read_buf)
{
	u8 dummy = 0xff;
	int i;

	aspeed_spi_start_user(priv, flash);

	/* cmd buffer = cmd + addr + dummies */
	aspeed_spi_send_cmd_addr(priv, flash, cmdbuf,
				 cmdlen - flash->spi->dummy_byte);

	for (i = 0 ; i < flash->spi->dummy_byte; i++)
		aspeed_spi_write_to_ahb(flash->ahb_base, &dummy, 1);

	if (flash->iomode) {
		clrbits_le32(&priv->regs->ce_ctrl[flash->cs],
			     CE_CTRL_IO_MODE_MASK);
		setbits_le32(&priv->regs->ce_ctrl[flash->cs], flash->iomode);
	}

	aspeed_spi_read_from_ahb(flash->ahb_base, read_buf, len);
	aspeed_spi_stop_user(priv, flash);

	return 0;
}

static ssize_t aspeed_spi_write_user(struct aspeed_spi_priv *priv,
				     struct aspeed_spi_flash *flash,
				     unsigned int cmdlen, const u8 *cmdbuf,
				     unsigned int len,	const u8 *write_buf)
{
	aspeed_spi_start_user(priv, flash);

	/* cmd buffer = cmd + addr */
	aspeed_spi_send_cmd_addr(priv, flash, cmdbuf, cmdlen);
	aspeed_spi_write_to_ahb(flash->ahb_base, write_buf, len);

	aspeed_spi_stop_user(priv, flash);

	return 0;
}

static u32 aspeed_spi_flash_to_addr(struct aspeed_spi_flash *flash,
				    const u8 *cmdbuf, unsigned int cmdlen)
{
	u8 addrlen = cmdlen - 1;
	u32 addr = (cmdbuf[1] << 16) | (cmdbuf[2] << 8) | cmdbuf[3];

	/*
	 * U-Boot SPI Flash layer uses 3 bytes addresses, but it might
	 * change one day
	 */
	if (addrlen == 4)
		addr = (addr << 8) | cmdbuf[4];

	return addr;
}

/* TODO(clg@kaod.org): add support for XFER_MMAP instead ? */
static ssize_t aspeed_spi_read(struct aspeed_spi_priv *priv,
			       struct aspeed_spi_flash *flash,
			       unsigned int cmdlen, const u8 *cmdbuf,
			       unsigned int len, u8 *read_buf)
{
	/* cmd buffer = cmd + addr + dummies */
	u32 offset = aspeed_spi_flash_to_addr(flash, cmdbuf,
					      cmdlen - flash->spi->dummy_byte);

	/*
	 * Switch to USER command mode if the AHB window configured
	 * for the device is too small for the read operation
	 */
	if (offset + len >= flash->ahb_size) {
		return aspeed_spi_read_user(priv, flash, cmdlen, cmdbuf,
					    len, read_buf);
	}

	memcpy_fromio(read_buf, flash->ahb_base + offset, len);

	return 0;
}

static int aspeed_spi_xfer(struct udevice *dev, unsigned int bitlen,
			   const void *dout, void *din, unsigned long flags)
{
	struct udevice *bus = dev->parent;
	struct aspeed_spi_priv *priv = dev_get_priv(bus);
	struct aspeed_spi_flash *flash;
	u8 *cmd_buf = priv->cmd_buf;
	size_t data_bytes;
	int err = 0;

	flash = aspeed_spi_get_flash(dev);
	if (!flash)
		return -ENXIO;

	if (flags & SPI_XFER_BEGIN) {
		/* save command in progress */
		priv->cmd_len = bitlen / 8;
		memcpy(cmd_buf, dout, priv->cmd_len);
	}

	if (flags == (SPI_XFER_BEGIN | SPI_XFER_END)) {
		/* if start and end bit are set, the data bytes is 0. */
		data_bytes = 0;
	} else {
		data_bytes = bitlen / 8;
	}

	debug("CS%u: %s cmd %zu bytes data %zu bytes\n", flash->cs,
	      din ? "read" : "write", priv->cmd_len, data_bytes);

	if ((flags & SPI_XFER_END) || flags == 0) {
		if (priv->cmd_len == 0) {
			pr_err("No command is progress !\n");
			return -1;
		}

		if (din && data_bytes) {
			if (priv->cmd_len == 1)
				err = aspeed_spi_read_reg(priv, flash,
							  cmd_buf[0],
							  din, data_bytes);
			else
				err = aspeed_spi_read(priv, flash,
						      priv->cmd_len,
						      cmd_buf, data_bytes,
						      din);
		} else if (dout) {
			if (priv->cmd_len == 1)
				err = aspeed_spi_write_reg(priv, flash,
							   cmd_buf[0],
							   dout, data_bytes);
			else
				err = aspeed_spi_write_user(priv, flash,
							    priv->cmd_len,
							    cmd_buf, data_bytes,
							    dout);
		}

		if (flags & SPI_XFER_END) {
			/* clear command */
			memset(cmd_buf, 0, sizeof(priv->cmd_buf));
			priv->cmd_len = 0;
		}
	}

	return err;
}

static int aspeed_spi_child_pre_probe(struct udevice *dev)
{
	struct dm_spi_slave_platdata *slave_plat = dev_get_parent_platdata(dev);

	debug("pre_probe slave device on CS%u, max_hz %u, mode 0x%x.\n",
	      slave_plat->cs, slave_plat->max_hz, slave_plat->mode);

	if (!aspeed_spi_get_flash(dev))
		return -ENXIO;

	return 0;
}

/*
 * It is possible to automatically define a contiguous address space
 * on top of all CEs in the AHB window of the controller but it would
 * require much more work. Let's start with a simple mapping scheme
 * which should work fine for a single flash device.
 *
 * More complex schemes should probably be defined with the device
 * tree.
 */
static int aspeed_spi_flash_set_segment(struct aspeed_spi_priv *priv,
					struct aspeed_spi_flash *flash)
{
	u32 seg_addr;

	/* could be configured through the device tree */
	flash->ahb_size = flash->spi->size;

	seg_addr = SEGMENT_ADDR_VALUE((u32)flash->ahb_base,
				      (u32)flash->ahb_base + flash->ahb_size);

	writel(seg_addr, &priv->regs->segment_addr[flash->cs]);

	return 0;
}

/*
 * The Aspeed FMC controller automatically detects at boot time if a
 * flash device is in 4BYTE address mode and self configures to use
 * the appropriate address width. This can be a problem for the U-Boot
 * SPI Flash layer which only supports 3 byte addresses. In such a
 * case, a warning is emitted and the address width is fixed when sent
 * on the bus.
 */
static void aspeed_spi_flash_check_4b(struct aspeed_spi_priv *priv,
				      struct aspeed_spi_flash *flash)
{
	if (readl(&priv->regs->ctrl) & BIT(flash->cs))
		pr_err("CS%u: 4BYTE address mode is active\n", flash->cs);
}

static int aspeed_spi_flash_init(struct aspeed_spi_priv *priv,
				 struct aspeed_spi_flash *flash,
				 struct udevice *dev)
{
	struct spi_flash *spi_flash = dev_get_uclass_priv(dev);
	struct spi_slave *slave = dev_get_parent_priv(dev);
	u32 user_hclk;
	u32 read_hclk;

	/*
	 * The SPI flash device slave should not change, so initialize
	 * it only once.
	 */
	if (flash->init)
		return 0;

	/*
	 * The flash device has not been probed yet. Initial transfers
	 * to read the JEDEC of the device will use the initial
	 * default settings of the registers.
	 */
	if (!spi_flash->name)
		return 0;

	debug("CS%u: init %s flags:%x size:%d page:%d sector:%d erase:%d "
	      "cmds [ erase:%x read=%x write:%x ] dummy:%d\n",
	      flash->cs,
	      spi_flash->name, spi_flash->flags, spi_flash->size,
	      spi_flash->page_size, spi_flash->sector_size,
	      spi_flash->erase_size, spi_flash->erase_cmd,
	      spi_flash->read_cmd, spi_flash->write_cmd,
	      spi_flash->dummy_byte);

	flash->spi = spi_flash;

	/*
	 * Tune the CE Control Register values for the modes the
	 * driver will use:
	 *   - USER command for specific SPI commands, write and
	 *     erase.
	 *   - FAST READ command mode for reads. The flash device is
	 *     directly accessed through its AHB window.
	 *
	 * TODO(clg@kaod.org): introduce a DT property for writes ?
	 */
	user_hclk = 0;

	flash->ce_ctrl_user = CE_CTRL_CLOCK_FREQ(user_hclk) |
		CE_CTRL_USERMODE;

	read_hclk = aspeed_spi_hclk_divisor(priv->hclk_rate, slave->speed);

	if (slave->mode & (SPI_RX_DUAL | SPI_TX_DUAL)) {
		debug("CS%u: setting dual data mode\n", flash->cs);
		flash->iomode = CE_CTRL_IO_DUAL_DATA;
	}

	flash->ce_ctrl_fread = CE_CTRL_CLOCK_FREQ(read_hclk) |
		flash->iomode |
		CE_CTRL_CMD(flash->spi->read_cmd) |
		CE_CTRL_DUMMY(flash->spi->dummy_byte) |
		CE_CTRL_FREADMODE;

	debug("CS%u: USER mode 0x%08x FREAD mode 0x%08x\n", flash->cs,
	      flash->ce_ctrl_user, flash->ce_ctrl_fread);

	/* Set the CE Control Register default (FAST READ) */
	writel(flash->ce_ctrl_fread, &priv->regs->ce_ctrl[flash->cs]);

	/* Enable 4BYTE addresses (No support in U-Boot yet) */
	if (flash->spi->size >= 16 << 20)
		aspeed_spi_flash_check_4b(priv, flash);

	/* Set Address Segment Register for direct AHB accesses */
	aspeed_spi_flash_set_segment(priv, flash);

	/* All done */
	flash->init = true;

	return 0;
}

static int aspeed_spi_claim_bus(struct udevice *dev)
{
	struct udevice *bus = dev->parent;
	struct aspeed_spi_priv *priv = dev_get_priv(bus);
	struct dm_spi_slave_platdata *slave_plat = dev_get_parent_platdata(dev);
	struct aspeed_spi_flash *flash;

	debug("%s: claim bus CS%u\n", bus->name, slave_plat->cs);

	flash = aspeed_spi_get_flash(dev);
	if (!flash)
		return -ENODEV;

	return aspeed_spi_flash_init(priv, flash, dev);
}

static int aspeed_spi_release_bus(struct udevice *dev)
{
	struct udevice *bus = dev->parent;
	struct dm_spi_slave_platdata *slave_plat = dev_get_parent_platdata(dev);

	debug("%s: release bus CS%u\n", bus->name, slave_plat->cs);

	if (!aspeed_spi_get_flash(dev))
		return -ENODEV;

	return 0;
}

static int aspeed_spi_set_mode(struct udevice *bus, uint mode)
{
	debug("%s: setting mode to %x\n", bus->name, mode);

	if (mode & (SPI_RX_QUAD | SPI_TX_QUAD)) {
		pr_err("%s invalid QUAD IO mode\n", bus->name);
		return -EINVAL;
	}

	/* The CE Control Register is set in claim_bus() */
	return 0;
}

static int aspeed_spi_set_speed(struct udevice *bus, uint hz)
{
	debug("%s: setting speed to %u\n", bus->name, hz);

	/* The CE Control Register is set in claim_bus() */
	return 0;
}

static int aspeed_spi_count_flash_devices(struct udevice *bus)
{
	ofnode node;
	int count = 0;

	dev_for_each_subnode(node, bus) {
		if (ofnode_is_available(node) &&
		    ofnode_device_is_compatible(node, "spi-flash"))
			count++;
	}

	return count;
}

static int aspeed_spi_bind(struct udevice *bus)
{
	debug("%s assigned req_seq=%d seq=%d\n", bus->name, bus->req_seq,
	      bus->seq);

	return 0;
}

static int aspeed_spi_probe(struct udevice *bus)
{
	struct resource res_regs, res_ahb;
	struct aspeed_spi_priv *priv = dev_get_priv(bus);
	struct clk hclk;
	int ret;

	ret = dev_read_resource(bus, 0, &res_regs);
	if (ret < 0)
		return ret;

	priv->regs = (void __iomem *)res_regs.start;

	ret = dev_read_resource(bus, 1, &res_ahb);
	if (ret < 0)
		return ret;

	priv->ahb_base = (void __iomem *)res_ahb.start;
	priv->ahb_size = res_ahb.end - res_ahb.start;

	ret = clk_get_by_index(bus, 0, &hclk);
	if (ret < 0) {
		pr_err("%s could not get clock: %d\n", bus->name, ret);
		return ret;
	}

	priv->hclk_rate = clk_get_rate(&hclk);
	clk_free(&hclk);

	priv->max_hz = dev_read_u32_default(bus, "spi-max-frequency",
					    100000000);

	priv->num_cs = dev_read_u32_default(bus, "num-cs", ASPEED_SPI_MAX_CS);

	priv->flash_count = aspeed_spi_count_flash_devices(bus);
	if (priv->flash_count > priv->num_cs) {
		pr_err("%s has too many flash devices: %d\n", bus->name,
		       priv->flash_count);
		return -EINVAL;
	}

	if (!priv->flash_count) {
		pr_err("%s has no flash devices ?!\n", bus->name);
		return -ENODEV;
	}

	/*
	 * There are some slight differences between the FMC and the
	 * SPI controllers
	 */
	priv->is_fmc = device_is_compatible(bus, "aspeed,ast2500-fmc");

	ret = aspeed_spi_controller_init(priv);
	if (ret)
		return ret;

	debug("%s probed regs=%p ahb_base=%p max-hz=%d cs=%d seq=%d\n",
	      bus->name, priv->regs, priv->ahb_base, priv->max_hz,
	      priv->flash_count, bus->seq);

	return 0;
}

static const struct dm_spi_ops aspeed_spi_ops = {
	.claim_bus	= aspeed_spi_claim_bus,
	.release_bus	= aspeed_spi_release_bus,
	.set_mode	= aspeed_spi_set_mode,
	.set_speed	= aspeed_spi_set_speed,
	.xfer		= aspeed_spi_xfer,
};

static const struct udevice_id aspeed_spi_ids[] = {
	{ .compatible = "aspeed,ast2500-fmc" },
	{ .compatible = "aspeed,ast2500-spi" },
	{ }
};

U_BOOT_DRIVER(aspeed_spi) = {
	.name = "aspeed_spi",
	.id = UCLASS_SPI,
	.of_match = aspeed_spi_ids,
	.ops = &aspeed_spi_ops,
	.priv_auto_alloc_size = sizeof(struct aspeed_spi_priv),
	.child_pre_probe = aspeed_spi_child_pre_probe,
	.bind  = aspeed_spi_bind,
	.probe = aspeed_spi_probe,
};
