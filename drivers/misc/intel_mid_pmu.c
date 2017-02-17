/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/device.h>
#include <linux/io.h>
#include <linux/sizes.h>
#include <malloc.h>

/* Registers */
#define PM_STS  		0x00
#define PM_CMD  		0x04
#define PM_ICS  		0x08
#define PM_WKC(x)		(0x10 + (x) * 4)
#define PM_WKS(x)		(0x18 + (x) * 4)
#define PM_SSC(x)		(0x20 + (x) * 4)
#define PM_SSS(x)		(0x30 + (x) * 4)

/* Bits in PM_STS */
#define PM_STS_BUSY		(1 << 8)

/* List of Intel Tangier LSSs */
#define PMU_LSS_TANGIER_SDIO0_01	1

struct pmu_mid {
	void __iomem *ioaddr;
};

static unsigned int pmu_readl(void __iomem *base, unsigned int offset)
{
	return readl(base + offset);
}

static void pmu_writel(void __iomem *base, unsigned int offset,
		unsigned int value)
{
	writel(value, base + offset);
}

static int pmu_read_status(struct pmu_mid *pmu)
{
	int retry = 500000;
	u32 reg;

	do {
		reg = pmu_readl(pmu->ioaddr, PM_STS);
		if (!(reg & PM_STS_BUSY))
			return 0;

		udelay(1);
	} while (--retry);

	printf("WARNING: PMU still busy\n");
	return -EBUSY;
}

static int pmu_enable_mmc(struct pmu_mid *pmu)
{
	u32 pm_ssc0;
	int ret;

	/* Check PMU status */
	ret = pmu_read_status(pmu);
	if (ret)
		return ret;

	/* Read PMU values */
	pm_ssc0 = pmu_readl(pmu->ioaddr, PM_SSS(0));

	/* Modify PMU values */

	/* Enable SDIO0, sdhci for SD card */
	pm_ssc0 &= ~(0x3 << (PMU_LSS_TANGIER_SDIO0_01 * 2));

	/* Write modified PMU values */
	pmu_writel(pmu->ioaddr, PM_SSC(0), pm_ssc0);

	/* Update modified PMU values */
	pmu_writel(pmu->ioaddr, PM_CMD, 0x00002201);

	/* Check PMU status */
	return pmu_read_status(pmu);
}

static int pmu_mid_bind(struct udevice *dev)
{
	/* This device is needed straight away */
	return device_probe(dev);
}

static int pmu_mid_probe(struct udevice *dev)
{
	struct pmu_mid	*pmu = dev_get_uclass_priv(dev);
	fdt_addr_t	base;

	base = dev_get_addr(dev);
	if (base == FDT_ADDR_T_NONE)
		return -EINVAL;

	pmu->ioaddr = devm_ioremap(dev, base, SZ_1K);
	if (!pmu->ioaddr)
		return -ENOMEM;

	return pmu_enable_mmc(pmu);
}

static const struct udevice_id pmu_mid_match[] = {
	{ .compatible = "intel,pmu-mid" },
	{ .compatible = "intel,pmu-tangier" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(intel_mid_pmu) = {
	.name		= "intel_mid_pmu",
	.id		= UCLASS_MISC,
	.of_match	= pmu_mid_match,
	.bind		= pmu_mid_bind,
	.probe		= pmu_mid_probe,
	.priv_auto_alloc_size = sizeof(struct pmu_mid),
};
