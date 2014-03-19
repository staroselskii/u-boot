/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * (C) Copyright 2010,2011
 * Graeme Russ, <graeme.russ@gmail.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <asm/bootparam.h>
#include <asm/e820.h>
#include <asm/global_data.h>
#include <asm/processor.h>
#include <asm/sections.h>
#include <asm/sfi.h>
#include <asm/u-boot-x86.h>

DECLARE_GLOBAL_DATA_PTR;

/* Memory type definitions */
enum sfi_mem_type {
	SFI_MEM_RESERVED,
	SFI_LOADER_CODE,
	SFI_LOADER_DATA,
	SFI_BOOT_SERVICE_CODE,
	SFI_BOOT_SERVICE_DATA,
	SFI_RUNTIME_SERVICE_CODE,
	SFI_RUNTIME_SERVICE_DATA,
	SFI_MEM_CONV,
	SFI_MEM_UNUSABLE,
	SFI_ACPI_RECLAIM,
	SFI_ACPI_NVS,
	SFI_MEM_MMIO,
	SFI_MEM_IOPORT,
	SFI_PAL_CODE,
	SFI_MEM_TYPEMAX,
};

#define SFI_BASE_ADDR		0x000E0000
#define SFI_LENGTH		0x00020000
#define SFI_TABLE_LENGTH	16

static int sfi_table_check(struct sfi_table_header *sbh)
{
	char chksum = 0;
	char *pos = (char *)sbh;
	int i;

	if (sbh->len < SFI_TABLE_LENGTH)
		return -1;

	if (sbh->len > SFI_LENGTH)
		return -1;

	for (i = 0; i < sbh->len; i++)
		chksum += *pos++;

	if (chksum)
		error("sfi: Invalid checksum\n");

	/* Checksum is OK if zero */
	return chksum;
}

static int sfi_table_is_type(struct sfi_table_header *sbh, const char *signature)
{
	return !strncmp(sbh->sig, signature, SFI_SIGNATURE_SIZE) &&
	       !sfi_table_check(sbh);
}

static struct sfi_table_simple *sfi_get_table_by_sig(unsigned long addr,
						     const char *signature)
{
	struct sfi_table_simple *sb = NULL;
	int i;

	for (i = 0; i < SFI_LENGTH; i += SFI_TABLE_LENGTH) {
		sb = (struct sfi_table_simple *)(addr + i);
		if (sfi_table_is_type(&sb->header, signature))
			break;
	}

	if (i >= SFI_LENGTH || !sb)
		return NULL;

	return sb;
}

static struct sfi_table_simple *sfi_search_mmap(void)
{
	struct sfi_table_header *sbh;
	struct sfi_table_simple *sb;
	u32 sys_entry_cnt;
	u32 i;

	/* Find SYST table */
	sb = sfi_get_table_by_sig(SFI_BASE_ADDR, SFI_SIG_SYST);
	if (!sb) {
		error("failed to locate SYST table\n");
		return NULL;
	}

	sys_entry_cnt = (sb->header.len - sizeof(*sbh)) / 8;

	/* Search through each SYST entry for MMAP table */
	for (i = 0; i < sys_entry_cnt; i++) {
		sbh = (struct sfi_table_header *)(unsigned long)sb->pentry[i];

		if (sfi_table_is_type(sbh, SFI_SIG_MMAP))
			return (struct sfi_table_simple *) sbh;
	}

	error("failed to locate SFI MMAP table\n");
	return NULL;
}

#define sfi_for_each_mentry(i, sb, mentry)				\
	for (i = 0, mentry = (struct sfi_mem_entry *)sb->pentry;	\
	     i < SFI_GET_NUM_ENTRIES(sb, struct sfi_mem_entry);		\
	     i++, mentry++)						\

static unsigned sfi_setup_e820(unsigned max_entries, struct e820entry *entries)
{
	struct sfi_table_simple *sb;
	struct sfi_mem_entry *mentry;
	unsigned long long start, end, size;
	int type, total = 0;
	int i;

	sb = sfi_search_mmap();
	if (!sb)
		return 0;

	sfi_for_each_mentry(i, sb, mentry) {
		start = mentry->phys_start;
		size = mentry->pages << 12;
		end = start + size;

		if (start > end)
			continue;

		/* translate SFI mmap type to E820 map type */
		switch (mentry->type) {
		case SFI_MEM_CONV:
			type = E820_RAM;
			break;
		case SFI_MEM_UNUSABLE:
		case SFI_RUNTIME_SERVICE_DATA:
			continue;
		default:
			type = E820_RESERVED;
		}

		if (total == E820MAX)
			break;
		entries[total].addr = start;
		entries[total].size = size;
		entries[total].type = type;

		total++;
	}

	return total;
}

static unsigned sfi_get_bank_size(void)
{
	struct sfi_table_simple *sb;
	struct sfi_mem_entry *mentry;
	int bank = 0;
	int i;

	sb = sfi_search_mmap();
	if (!sb)
		return 0;

	sfi_for_each_mentry(i, sb, mentry) {
		if (mentry->type != SFI_MEM_CONV)
			continue;

		gd->bd->bi_dram[bank].start = mentry->phys_start;
		gd->bd->bi_dram[bank].size = mentry->pages << 12;
		bank++;
	}

	return bank;
}

static phys_size_t sfi_get_ram_size(void)
{
	struct sfi_table_simple *sb;
	struct sfi_mem_entry *mentry;
	phys_size_t ram = 0;
	int i;

	sb = sfi_search_mmap();
	if (!sb)
		return 0;

	sfi_for_each_mentry(i, sb, mentry) {
		if (mentry->type != SFI_MEM_CONV)
			continue;

		ram += (mentry->pages << 12);
	}

	debug("RAM size %llu\n", ram);

	return ram;
}

unsigned install_e820_map(unsigned max_entries, struct e820entry *entries)
{
	return sfi_setup_e820(max_entries, entries);
}

void dram_init_banksize(void)
{
	sfi_get_bank_size();
}

int dram_init(void)
{
	gd->ram_size = sfi_get_ram_size();

	return 0;
}
