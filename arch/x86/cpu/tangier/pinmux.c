// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2018 Emlid Limited
 */

#include <common.h>
#include <dm.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/cpu.h>
#include <asm/scu.h>
#include <linux/io.h>
#include <linux/bitops.h>

#define BUFCFG_PINMODE_SHIFT	0
#define BUFCFG_PINMODE_MASK	GENMASK(2, 0)
#define BUFCFG_OFFSET	0x100

#define MRFLD_FAMILY_LEN	0x400

#define pin_to_bufno(f, p)	  ((p) - (f)->pin_base)

struct mrfld_family {
	unsigned int barno;
	unsigned int pin_base;
	size_t npins;
	void __iomem *regs;
};

#define MRFLD_FAMILY(b, s, e)		\
	{					   \
		.barno = (b),			   \
		.pin_base = (s),			\
		.npins = (e) - (s) + 1,		 \
	}

static struct mrfld_family i2c_family = MRFLD_FAMILY(7, 101, 114);

static void __iomem *mrfld_get_bufcfg(struct mrfld_family *family, unsigned int pin)
{
	unsigned int bufno;

	if (!family)
		return NULL;

	bufno = pin_to_bufno(family, pin);

	return family->regs + BUFCFG_OFFSET + bufno * 4;
}

static void mrfld_setup_families(void *base_addr, struct mrfld_family *families, unsigned int nfam)
{
    for (int i = 0; i < nfam; i++) {
		struct mrfld_family *family = &families[i];

		family->regs = base_addr + family->barno * MRFLD_FAMILY_LEN;

		debug("fam.regs: 0x%x\n", (u32) family->regs);
	}
}

int mrfld_pinconfig_protected(unsigned int pin)
{
	int cmd = IPCMSG_INDIRECT_WRITE;
	void __iomem *bufcfg;
	u32 v, value;
	int ret;

	struct mrfld_family *mf = &i2c_family;

	/* These are hardcoded values taken from the Linux kernel */

	u32 mask = BUFCFG_PINMODE_MASK;
	u32 bits = 0x1 << BUFCFG_PINMODE_SHIFT;

	bufcfg = mrfld_get_bufcfg(mf, pin);

	if (!bufcfg)
		return -EINVAL;

	value = readl(bufcfg);

	v = (value & ~mask) | (bits & mask);

	debug("scu: cmd: %d v: 0x%x p: 0x%x bits: %d, mask: %d bufcfg: 0x%p\n",
				cmd, v, (u32) bufcfg, bits, mask, bufcfg);

	ret = scu_ipc_raw_command(cmd, 0, &v, 4, NULL, 0, (u32) bufcfg, 0);

	if (ret)
		printf("Failed to set bufcfg via SCU IPC for pin %u (%d)\n", pin, ret);

	return ret;
}

static int tangier_pinctrl_probe(struct udevice *dev)
{
    void *base_addr = syscon_get_first_range(X86_SYSCON_PINCONF);

	debug("dt addr: 0x%p\n", base_addr);

	mrfld_setup_families(base_addr, &i2c_family, 1);

	return 0;
}

static const struct udevice_id tangier_pinctrl_match[] = {
	{ .compatible = "intel,pinctrl-tangier",
		.data = X86_SYSCON_PINCONF },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(tangier_pinctrl) = {
	.name = "tangier_pinctrl",
	.id = UCLASS_SYSCON,
	.of_match = tangier_pinctrl_match,
	.probe = tangier_pinctrl_probe,
};
