/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * (C) Copyright 2008
 * Graeme Russ, graeme.russ@gmail.com.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/u-boot-x86.h>
#include <intel_scu_ipc.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Miscellaneous platform dependent initializations
 */
int arch_cpu_init(void)
{
	return x86_cpu_init_f();
}

int print_cpuinfo(void)
{
	return default_print_cpuinfo();
}

void reset_cpu(ulong addr)
{
	intel_scu_ipc_simple_command(IPCMSG_COLD_RESET, 0);
}
