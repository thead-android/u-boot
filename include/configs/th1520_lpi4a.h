/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2023 Yixun Lan <dlan@gentoo.org>
 *
 */

#ifndef __TH1520_LPI4A_H
#define __TH1520_LPI4A_H

#include <linux/sizes.h>

#define CFG_SYS_NS16550_CLK		100000000
#define CFG_SYS_SDRAM_BASE		0x00000000

#define UART_BASE	0xffe7014000
#define UART_REG_WIDTH  32

/* Environment options */

#ifdef CONFIG_BOOTMETH_ANDROID
#define CFG_EXTRA_ENV_SETTINGS \
	"PS1=[LPi4A]# \0" \
	"loadaddr=0x10000000\0" \
	"kernel_addr_r=0x04000000\0" \
	"vendor_boot_comp_addr_r=0x18000000\0" \
	"ramdisk_addr_r=0x20000000\0" \
	"fdt_addr_r=0x30000000\0" \
	"android_setup=setenv loadaddr 0x10000000; setenv kernel_addr_r 0x04000000; " \
		"setenv vendor_boot_comp_addr_r 0x18000000; setenv ramdisk_addr_r 0x20000000; " \
		"setenv fdt_addr_r 0x30000000\0" \
	"android_mmc=mmc dev 0; mmc rescan\0" \
	"android_select_a=setenv android_slot a; setenv slot_suffix _a; " \
		"setenv bs; setenv bz; setenv vs; setenv vz\0" \
	"android_select_b=setenv android_slot b; setenv slot_suffix _b; " \
		"setenv bs; setenv bz; setenv vs; setenv vz\0" \
	"android_parts=if part start mmc 0 boot_${android_slot} bs && " \
		"part size mmc 0 boot_${android_slot} bz && " \
		"part start mmc 0 vendor_boot_${android_slot} vs && " \
		"part size mmc 0 vendor_boot_${android_slot} vz; then true; " \
		"else echo Android slot ${slot_suffix} partitions not found; false; fi\0" \
	"android_read_boot=mmc read ${loadaddr} ${bs} ${bz}\0" \
	"android_read_vendor=mmc read ${vendor_boot_comp_addr_r} ${vs} ${vz}\0" \
	"android_image=abootimg addr ${loadaddr} ${vendor_boot_comp_addr_r}\0" \
	"android_load=if run android_mmc && run android_parts && run android_read_boot && " \
		"run android_read_vendor; then run android_image; else false; fi\0" \
	"android_args_core=setenv bootargs console=ttyS0,115200 earlycon 8250.nr_uarts=4 " \
		"clk_ignore_unused loop.max_part=7 loglevel=8 ignore_loglevel init=/init " \
		"firmware_class.path=/vendor/firmware mitigations=off\0" \
	"android_args_slot=setenv bootargs ${bootargs} androidboot.slot_suffix=${slot_suffix} " \
		"androidboot.force_normal_boot=1 androidboot.verifiedbootstate=orange\0" \
	"android_boot_selected=echo Booting Android slot ${slot_suffix}; " \
		"if run android_setup && run android_load && run android_args_core && " \
		"run android_args_slot; then bootm ${loadaddr}; " \
		"else echo Android slot ${slot_suffix} boot aborted; false; fi\0" \
	"boot_android_a=run android_select_a; run android_boot_selected\0" \
	"boot_android_b=run android_select_b; run android_boot_selected\0"
#else
#define CFG_EXTRA_ENV_SETTINGS \
	"PS1=[LPi4A]# \0"
#endif

#endif /* __TH1520_LPI4A_H */
