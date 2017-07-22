/*
 * Copyright (c) 2017 Intel Corporation
 *
 * Partially based on iomap.h for other x86 platforms
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _TANGIER_IOMAP_H_
#define _TANGIER_IOMAP_H_

/* Memory Mapped IO bases */

/* PCI Configuration Space */
#define MCFG_BASE_ADDRESS		CONFIG_PCIE_ECAM_BASE
#define MCFG_BASE_SIZE			0x10000000

/* Root Complex Base Address */
#define RCBA_BASE_ADDRESS		CONFIG_RCBA_BASE
#define RCBA_BASE_SIZE			0x4000

#define HPET_BASE_ADDRESS		0xfed00000
#define HPET_BASE_SIZE			0x1000

#endif /* _TANGIER_IOMAP_H_ */
