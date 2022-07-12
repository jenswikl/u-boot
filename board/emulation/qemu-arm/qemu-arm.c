// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Tuomas Tynkkynen
 */

#include <common.h>
#include <cpu_func.h>
#include <dm.h>
#include <fdtdec.h>
#include <init.h>
#include <log.h>
#include <transfer_list.h>
#include <virtio_types.h>
#include <virtio.h>

#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>

static struct mm_region qemu_arm64_mem_map[] = {
	{
		/* Flash */
		.virt = 0x00000000UL,
		.phys = 0x00000000UL,
		.size = 0x08000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* Lowmem peripherals */
		.virt = 0x08000000UL,
		.phys = 0x08000000UL,
		.size = 0x38000000,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* RAM */
		.virt = 0x40000000UL,
		.phys = 0x40000000UL,
		.size = 255UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* Highmem PCI-E ECAM memory area */
		.virt = 0x4010000000ULL,
		.phys = 0x4010000000ULL,
		.size = 0x10000000,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* Highmem PCI-E MMIO memory area */
		.virt = 0x8000000000ULL,
		.phys = 0x8000000000ULL,
		.size = 0x8000000000ULL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = qemu_arm64_mem_map;
#endif

/* Boot parameters saved from lowlevel_init.S */
struct {
	unsigned long arg1;
	unsigned long arg3;
} qemu_saved_args __section(".data");

int board_init(void)
{
	return 0;
}

int board_late_init(void)
{
	/*
	 * Make sure virtio bus is enumerated so that peripherals
	 * on the virtio bus can be discovered by their drivers
	 */
	virtio_init();

	return 0;
}

int dram_init(void)
{
	if (fdtdec_setup_mem_size_base() != 0)
		return -EINVAL;

	return 0;
}

int dram_init_banksize(void)
{
	fdtdec_setup_memory_banksize();

	return 0;
}

void *board_fdt_blob_setup(void)
{
	/*
	 * QEMU loads a generated DTB for us at the start of RAM. Either
	 * use this DTB or use the location for a new DTB.
	 */
	void *fdt = (void *)CONFIG_SYS_SDRAM_BASE;
#if defined(CONFIG_FIRMWARE_HANDOFF)
	unsigned long max_size = 0x10000;
	struct transfer_list *tl = gd->transfer_list;
	struct transfer_entry *te_fdt;
	struct transfer_entry *te_fdto;
	void *new_fdt;

	/*
	 * If it looks like there's a valid DTB at the start of RAM save
	 * that as a backup DTB and use the location right after for the
	 * as the a temporary location of the new DTB.
	 */
	if (!fdt_check_header(fdt))
		new_fdt = (uint8_t *)fdt + roundup(fdt_totalsize(fdt), 0x1000);
	else
		new_fdt = fdt;

	if (!tl && qemu_saved_args.arg1 == TRANSFER_LIST_SIGNATURE &&
	    qemu_saved_args.arg3) {
		tl = transfer_list_check_header((void *)qemu_saved_args.arg3);
		gd->transfer_list = tl;;
	}

	if (!tl)
		return fdt;

	te_fdt = transfer_list_find(tl, TL_TAG_FDT);
	if (!te_fdt)
		return fdt;

	if (fdt_open_into(transfer_list_data(te_fdt), new_fdt, max_size))
		return fdt;

	te_fdto = transfer_list_find(tl, TL_TAG_FDT_OVERLAY);
	if (!te_fdto)
		goto use_new_fdt;

	if (fdt_overlay_apply(new_fdt, transfer_list_data(te_fdto))) {
		/*
		 * We failed to apply the overlay for some reason, use the
		 * DTB unmodified instead.
		 */
		if (fdt_open_into(transfer_list_data(te_fdt), new_fdt,
				  max_size))
			return fdt;
	}

use_new_fdt:
	fdt_move(new_fdt, fdt, fdt_totalsize(new_fdt));
	return fdt;
#endif

	return fdt;
}

void enable_caches(void)
{
	 icache_enable();
	 dcache_enable();
}

#if defined(CONFIG_EFI_RNG_PROTOCOL)
#include <efi_loader.h>
#include <efi_rng.h>

#include <dm/device-internal.h>

efi_status_t platform_get_rng_device(struct udevice **dev)
{
	int ret;
	efi_status_t status = EFI_DEVICE_ERROR;
	struct udevice *bus, *devp;

	for (uclass_first_device(UCLASS_VIRTIO, &bus); bus;
	     uclass_next_device(&bus)) {
		for (device_find_first_child(bus, &devp); devp;
		     device_find_next_child(&devp)) {
			if (device_get_uclass_id(devp) == UCLASS_RNG) {
				*dev = devp;
				status = EFI_SUCCESS;
				break;
			}
		}
	}

	if (status != EFI_SUCCESS) {
		debug("No rng device found\n");
		return EFI_DEVICE_ERROR;
	}

	if (*dev) {
		ret = device_probe(*dev);
		if (ret)
			return EFI_DEVICE_ERROR;
	} else {
		debug("Couldn't get child device\n");
		return EFI_DEVICE_ERROR;
	}

	return EFI_SUCCESS;
}
#endif /* CONFIG_EFI_RNG_PROTOCOL */

#ifdef CONFIG_ARM64
#define __W	"w"
#else
#define __W
#endif

u8 flash_read8(void *addr)
{
	u8 ret;

	asm("ldrb %" __W "0, %1" : "=r"(ret) : "m"(*(u8 *)addr));
	return ret;
}

u16 flash_read16(void *addr)
{
	u16 ret;

	asm("ldrh %" __W "0, %1" : "=r"(ret) : "m"(*(u16 *)addr));
	return ret;
}

u32 flash_read32(void *addr)
{
	u32 ret;

	asm("ldr %" __W "0, %1" : "=r"(ret) : "m"(*(u32 *)addr));
	return ret;
}

void flash_write8(u8 value, void *addr)
{
	asm("strb %" __W "1, %0" : "=m"(*(u8 *)addr) : "r"(value));
}

void flash_write16(u16 value, void *addr)
{
	asm("strh %" __W "1, %0" : "=m"(*(u16 *)addr) : "r"(value));
}

void flash_write32(u32 value, void *addr)
{
	asm("str %" __W "1, %0" : "=m"(*(u32 *)addr) : "r"(value));
}
