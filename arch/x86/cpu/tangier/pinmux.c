#include <common.h>
#include <asm/scu.h>
#include <linux/io.h>
#include <linux/bitops.h>

#define FLIS_BASE_ADDR		0xff0c0000
#define FLIS_LENGTH			0x8000

#define BUFCFG_PINMODE_SHIFT	0
#define BUFCFG_PINMODE_MASK	 GENMASK(2, 0)

#define BUFCFG_OFFSET			0x100

#define MRFLD_FAMILY_LEN		0x400

#define MRFLD_I2C6_SCL			111
#define MRFLD_I2C6_SDA			112

#define pin_to_bufno(f, p)	  ((p) - (f)->pin_base)

enum mrfld_protected {
	PROTECTED_NONE,
	PROTECTED_READ,
	PROTECTED_WRITE,
	PROTECTED_FULL,
};

struct mrfld_family {
	unsigned int barno;
	unsigned int pin_base;
	size_t npins;
	enum mrfld_protected protected;
	void __iomem *regs;
	phys_addr_t phys;
};

#define MRFLD_FAMILY_PROTECTED(b, s, e, _p_)		\
	{					   \
		.barno = (b),			   \
		.pin_base = (s),			\
		.npins = (e) - (s) + 1,		 \
		.protected = PROTECTED_##_p_,	   \
	}

#define MRFLD_FAMILY(b, s, e)   MRFLD_FAMILY_PROTECTED(b, s, e, NONE)

static struct mrfld_family mrfld_families[] = {
	MRFLD_FAMILY(1, 0, 12),
	MRFLD_FAMILY(2, 13, 36),
	MRFLD_FAMILY(3, 37, 56),
	MRFLD_FAMILY(4, 57, 64),
	MRFLD_FAMILY(5, 65, 78),
	MRFLD_FAMILY(6, 79, 100),
	MRFLD_FAMILY_PROTECTED(7, 101, 114, WRITE),
	MRFLD_FAMILY(8, 115, 126),
	MRFLD_FAMILY(9, 127, 145),
	MRFLD_FAMILY(10, 146, 157),
	MRFLD_FAMILY(11, 158, 179),
	MRFLD_FAMILY_PROTECTED(12, 180, 194, FULL),
	MRFLD_FAMILY(13, 195, 214),
	MRFLD_FAMILY(14, 215, 227),
	MRFLD_FAMILY(15, 228, 232),
};

static void __iomem *mrfld_get_bufcfg(struct mrfld_family *mp, unsigned int pin)
{
	const struct mrfld_family *family;
	unsigned int bufno;

	family = mp;

	if (!family)
		return NULL;

	bufno = pin_to_bufno(family, pin);
	return family->regs + BUFCFG_OFFSET + bufno * 4;
}

static void mrfld_setup_families(struct mrfld_family *families, unsigned int nfam)
{
	/* I guess, this address should come from ACPI.
	* But it hasn't been loaded when this code is run
	*/

	phys_addr_t flis_start = FLIS_BASE_ADDR;
	void __iomem *regs = ioremap(FLIS_BASE_ADDR, FLIS_LENGTH);

	/**
	* For now we're intersted only in I2C#6 family.
	* But this code can be easily extended to support all
	* other families as well.
	*/

	struct mrfld_family *family = &families[6];

	family->regs = regs + family->barno * MRFLD_FAMILY_LEN;
	family->phys = flis_start + family->barno * MRFLD_FAMILY_LEN;

	debug("fam.phys: 0x%llx fam.length: 0x%x\n", (phys_addr_t) family->phys, FLIS_LENGTH);
	debug("fam.regs: 0x%llx\n", (phys_addr_t) family->regs);
}

static phys_addr_t mrfld_get_phys(struct mrfld_family *mf, unsigned int pin)
{
	const struct mrfld_family *family;
	unsigned int bufno;

	family = mf;
	if (!family)
		return 0;

	bufno = pin_to_bufno(family, pin);

	return family->phys + BUFCFG_OFFSET + bufno * 4;
}

static int mrfld_update_phys(struct mrfld_family *mf, unsigned int pin,
				 u32 bits, u32 mask)
{
	int cmd = IPCMSG_INDIRECT_WRITE;
	void __iomem *bufcfg;
	phys_addr_t phys;
	u32 v, value;
	int ret;

	bufcfg = mrfld_get_bufcfg(mf, pin);
	value = readl(bufcfg);

	v = (value & ~mask) | (bits & mask);

	phys = mrfld_get_phys(mf, pin);

	debug("scu: cmd: %d v_addr: %p v: %d p: %p bits: %d, mask: %d\n", 
							cmd, (u8 *) &v, v, &phys, bits, mask);

	ret = scu_ipc_raw_command(cmd, 0, (u8 *)&v, 4, NULL, 0, phys, 0);

	if (ret)
		printf("Failed to set bufcfg via SCU IPC for pin %u (%d)\n", pin, ret);

	return ret;
}

int mrfld_i2c6_setup()
{
	mrfld_setup_families(mrfld_families, ARRAY_SIZE(mrfld_families));

	struct mrfld_family *i2c6_family = &mrfld_families[6];

	/* These are hardcoded values taken from the Linux kernel */

	u32 mask = BUFCFG_PINMODE_MASK;
	u32 bits = 0x1 << BUFCFG_PINMODE_SHIFT;

	mrfld_update_phys(i2c6_family, MRFLD_I2C6_SCL, bits, mask);
	mrfld_update_phys(i2c6_family, MRFLD_I2C6_SDA, bits, mask);

	return 0;
}

