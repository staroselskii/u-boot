#include <asm/types.h>
#include <linux/types.h>
#include <common.h>
#include <malloc.h>
#include <pci.h>
#include <part.h>
#include <mmc.h>
#include <sdhci.h>

int tangier_sdhci_init(u32 regbase, int index, int bus_width);
