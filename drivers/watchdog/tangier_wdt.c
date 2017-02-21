/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <watchdog.h>
#include <asm/scu.h>

/* Hardware timeout in seconds */
#ifndef CONFIG_WATCHDOG_TIMEOUT_MSECS
#define WATCHDOG_HEARTBEAT 60000
#else
#define WATCHDOG_HEARTBEAT CONFIG_WATCHDOG_TIMEOUT_MSECS
#endif

enum {
	SCU_WATCHDOG_START			= 0,
	SCU_WATCHDOG_STOP			= 1,
	SCU_WATCHDOG_KEEPALIVE			= 2,
	SCU_WATCHDOG_SET_ACTION_ON_TIMEOUT	= 3,
};

void hw_watchdog_reset(void)
{
	static unsigned long prev;
	unsigned long now;

	if (gd->timer)
		now = timer_get_us();
	else
		now = rdtsc() / 1000000;

	/* Do not flood SCU */
	if (unlikely((now - prev) > (WATCHDOG_HEARTBEAT * 1000))) {
		prev = now;
		scu_ipc_simple_command(IPCMSG_WATCHDOG_TIMER, SCU_WATCHDOG_KEEPALIVE);
	}
}

int hw_watchdog_disable(void)
{
	return scu_ipc_simple_command(IPCMSG_WATCHDOG_TIMER, SCU_WATCHDOG_STOP);
}

void hw_watchdog_init(void)
{
	u32 timeout = WATCHDOG_HEARTBEAT / 1000;
	int in_size;
	struct ipc_wd_start {
		u32 pretimeout;
		u32 timeout;
	} ipc_wd_start = { timeout, timeout };

	/*
	 * SCU expects the input size for watchdog IPC
	 * to be based on 4 bytes
	 */
	in_size = DIV_ROUND_UP(sizeof(ipc_wd_start), 4);

	scu_ipc_command(IPCMSG_WATCHDOG_TIMER, SCU_WATCHDOG_START,
			(u32 *)&ipc_wd_start, in_size, NULL, 0);
}
