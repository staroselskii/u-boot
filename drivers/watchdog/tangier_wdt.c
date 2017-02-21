/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <intel_scu_ipc.h>
#include <watchdog.h>

/* Hardware timeout in seconds */
#ifndef CONFIG_WATCHDOG_TIMEOUT_MSECS
#define WATCHDOG_HEARTBEAT 60000
#else
#define WATCHDOG_HEARTBEAT CONFIG_WATCHDOG_TIMEOUT_MSECS
#endif

enum {
	SCU_WATCHDOG_START = 0,
	SCU_WATCHDOG_STOP,
	SCU_WATCHDOG_KEEPALIVE,
	SCU_WATCHDOG_SET_ACTION_ON_TIMEOUT
};

void hw_watchdog_reset(void)
{
	static ulong prev;
	ulong now;

	if (gd->timer)
		now = timer_get_us();
	else
		now = rdtsc() / 1000000;

	/* Do not flood SCU */
	if (unlikely((now - prev) > (WATCHDOG_HEARTBEAT * 1000))) {
		prev = now;
		intel_scu_ipc_simple_command(IPCMSG_WATCHDOG_TIMER,
					     SCU_WATCHDOG_KEEPALIVE);
	}
}

int hw_watchdog_disable(void)
{
	return intel_scu_ipc_simple_command(IPCMSG_WATCHDOG_TIMER,
					    SCU_WATCHDOG_STOP);
}

void hw_watchdog_init(void)
{
	intel_scu_ipc_simple_command(IPCMSG_WATCHDOG_TIMER,
				     SCU_WATCHDOG_START);
}
