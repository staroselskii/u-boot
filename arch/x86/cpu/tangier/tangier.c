/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * (C) Copyright 2008
 * Graeme Russ, graeme.russ@gmail.com.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/arch/intel-mid.h>
#include <asm/arch/mmc.h>
#include <asm/u-boot-x86.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <asm/msr.h>
#include <intel_scu_ipc.h>
#include <u-boot/md5.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Miscellaneous platform dependent initializations
 */
int arch_cpu_init(void)
{
	return x86_cpu_init_f();
}

int board_early_init_f(void)
{
	return 0;
}

int board_early_init_r(void)
{
	init_scu_ipc();
	return 0;
}

int board_final_cleanup(void)
{

	return 0;
}

int board_late_init(void)
{
	if (!getenv("serial#")) {

		struct mmc *mmc = find_mmc_device(0);
		unsigned char emmc_ssn[16];
		char ssn[33];
		char usb_gadget_addr[18];

		if (mmc) {
			int i;

			md5((unsigned char *)mmc->cid, sizeof(mmc->cid), emmc_ssn);

			for (i = 0; i < 16; i++)
				snprintf(&(ssn[2*i]), 3, "%02x", emmc_ssn[i]);

			snprintf(&(usb_gadget_addr[0]), sizeof(usb_gadget_addr),
					"02:00:86:%02x:%02x:%02x", emmc_ssn[13], emmc_ssn[14],
					emmc_ssn[15]);
			setenv("usb0addr", usb_gadget_addr);
			setenv("serial#", ssn);
#if defined(CONFIG_CMD_SAVEENV) && !defined(CONFIG_ENV_IS_NOWHERE)
			saveenv();
#endif
		}
	}

	if (!getenv("hardware_id")) {
		union ipc_ifwi_version v;
		int ret;
		char hardware_id[4];

		ret = intel_scu_ipc_command(IPCMSG_GET_FW_REVISION, 1,
				NULL, 0, (u32 *) &(v.raw[0]), 4);
		if (ret < 0) {
			printf("Can't retrieve hardware revision\n");
		}

		snprintf(hardware_id, sizeof(hardware_id), "%02X", v.fw.hardware_id);
		setenv("hardware_id", hardware_id);
#if defined(CONFIG_CMD_SAVEENV) && !defined(CONFIG_ENV_IS_NOWHERE)
		saveenv();
#endif
	}


	return 0;
}

void panic_puts(const char *str)
{
}

int print_cpuinfo(void)
{
	return default_print_cpuinfo();
}

int board_mmc_init(bd_t * bis)
{
	int index = 0;
	unsigned int base = CONFIG_SYS_EMMC_PORT_BASE + (0x40000 * index);

	return tangier_sdhci_init(base, index, 4);
}

void reset_cpu(ulong addr)
{
	intel_scu_ipc_simple_command(IPCMSG_COLD_RESET, 0);
}
