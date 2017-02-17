/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/device.h>
#include <intel_scu_ipc.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/sizes.h>

#define IPC_STATUS_ADDR         0x04
#define IPC_SPTR_ADDR           0x08
#define IPC_DPTR_ADDR           0x0C
#define IPC_READ_BUFFER         0x90
#define IPC_WRITE_BUFFER        0x80
#define IPC_IOC			BIT(8)

struct intel_scu {
	void __iomem *base;
};

/* Only one for now */
static struct intel_scu *the_scu;

static void scu_writel(void __iomem *base, unsigned int offset, unsigned int val)
{
	writel(val, base + offset);
}

static unsigned int scu_readl(void __iomem *base, unsigned int offset)
{
	return readl(base + offset);
}

/*
 * Command Register (Write Only):
 * A write to this register results in an interrupt to the SCU core processor
 * Format:
 * |rfu2(8) | size(8) | command id(4) | rfu1(3) | ioc(1) | command(8)|
 */
static void intel_scu_ipc_send_command(struct intel_scu *scu, u32 cmd)
{
	scu_writel(scu->base, 0x00, cmd | IPC_IOC);
}

/*
 * IPC Write Buffer (Write Only):
 * 16-byte buffer for sending data associated with IPC command to
 * SCU. Size of the data is specified in the IPC_COMMAND_REG register
 */
static void ipc_data_writel(struct intel_scu *scu, u32 data, u32 offset)
{
	scu_writel(scu->base, IPC_WRITE_BUFFER + offset, data);
}

/*
 * Status Register (Read Only):
 * Driver will read this register to get the ready/busy status of the IPC
 * block and error status of the IPC command that was just processed by SCU
 * Format:
 * |rfu3(8)|error code(8)|initiator id(8)|cmd id(4)|rfu1(2)|error(1)|busy(1)|
 */
static u32 ipc_read_status(struct intel_scu *scu)
{
	return scu_readl(scu->base, IPC_STATUS_ADDR);
}

static u32 ipc_data_readl(struct intel_scu *scu, u32 offset)
{
	return scu_readl(scu->base, IPC_READ_BUFFER + offset);
}

static int intel_scu_ipc_check_status(struct intel_scu *scu)
{
	int loop_count = 3000000;
	int status;

	do {
		status = ipc_read_status(scu);
		udelay(1);
	} while ((status & BIT(0)) && --loop_count);
	if (!loop_count)
		return -ETIMEDOUT;

	if (status & BIT(1)) {
		printf("%s() status=0x%08x\n", __func__, status);
		return -EIO;
	}

	return 0;
}

static int intel_scu_ipc_raw_cmd(struct intel_scu *scu, u32 cmd, u32 sub,
				 u8 *in, u8 inlen, u32 *out, u32 outlen,
				 u32 dptr, u32 sptr)
{
	int i, err;
	u32 wbuf[4];

	if (inlen > 16)
		return -EINVAL;

	memcpy(wbuf, in, inlen);

	scu_writel(scu->base, IPC_DPTR_ADDR, dptr);
	scu_writel(scu->base, IPC_SPTR_ADDR, sptr);

	/**
	 * SRAM controller don't support 8bit write, it only supports
	 * 32bit write, so we have to write into the WBUF in 32bit,
	 * and SCU FW will use the inlen to determine the actual input
	 * data length in the WBUF.
	 */
	for (i = 0; i < DIV_ROUND_UP(inlen, 4); i++)
		ipc_data_writel(scu, wbuf[i], 4 * i);

	/**
	 * Watchdog IPC command is an exception here using double word
	 * as the unit of input data size because of some historical
	 * reasons and SCU FW is doing so.
	 */
	if ((cmd & 0xff) == IPCMSG_WATCHDOG_TIMER)
		inlen = DIV_ROUND_UP(inlen, 4);

	intel_scu_ipc_send_command(scu, (inlen << 16) | (sub << 12) | cmd);
	err = intel_scu_ipc_check_status(scu);

	for (i = 0; i < outlen; i++)
		*out++ = ipc_data_readl(scu, 4 * i);

	return err;
}

/**
 * intel_scu_ipc_simple_command - send a simple command
 * @cmd: command
 * @sub: sub type
 *
 * Issue a simple command to the SCU. Do not use this interface if
 * you must then access data as any data values may be overwritten
 * by another SCU access by the time this function returns.
 *
 * This function may sleep. Locking for SCU accesses is handled for
 * the caller.
 */
int intel_scu_ipc_simple_command(int cmd, int sub)
{
	intel_scu_ipc_send_command(the_scu, sub << 12 | cmd);

	return intel_scu_ipc_check_status(the_scu);
}

int intel_scu_ipc_command(u32 cmd, u32 sub, u8 *in, u8 inlen, u32 *out, u32 outlen)
{
	return intel_scu_ipc_raw_cmd(the_scu, cmd, sub, in, inlen, out, outlen, 0, 0);
}

static int intel_scu_bind(struct udevice *dev)
{
	/* This device is needed straight away */
	return device_probe(dev);
}

static int intel_scu_probe(struct udevice *dev)
{
	struct intel_scu	*scu = dev_get_uclass_priv(dev);
	fdt_addr_t		base;

	base = dev_get_addr(dev);
	if (base == FDT_ADDR_T_NONE)
		return -EINVAL;

	scu->base = devm_ioremap(dev, base, SZ_1K);
	if (!scu->base)
		return -ENOMEM;

	the_scu = scu;

	return 0;
}

static const struct udevice_id intel_scu_match[] = {
	{ .compatible = "intel,scu-ipc" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(intel_scu_ipc) = {
	.name		= "intel_scu_ipc",
	.id		= UCLASS_MISC,
	.of_match	= intel_scu_match,
	.bind		= intel_scu_bind,
	.probe		= intel_scu_probe,
	.priv_auto_alloc_size = sizeof(struct intel_scu),
};
