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
	"vendor_boot_part=vendor_boot_b\0" \
	"android_setup=setenv loadaddr 0x10000000; setenv kernel_addr_r 0x04000000; " \
		"setenv vendor_boot_comp_addr_r 0x18000000; setenv ramdisk_addr_r 0x20000000; " \
		"setenv fdt_addr_r 0x30000000\0" \
	"android_mmc=mmc dev 0; mmc rescan\0" \
	"android_parts_a=part start mmc 0 boot_a bs; part size mmc 0 boot_a bz; " \
		"part start mmc 0 ${vendor_boot_part} vs; part size mmc 0 ${vendor_boot_part} vz\0" \
	"android_read_boot=mmc read ${loadaddr} ${bs} ${bz}\0" \
	"android_read_vendor=mmc read ${vendor_boot_comp_addr_r} ${vs} ${vz}\0" \
	"android_fix_vendor=mw.b 0x18258896 0x6d 1; mw.b 0x18258897 0x6d 1; " \
		"mw.b 0x18258898 0x63 1; mw.b 0x18258899 0x20 1; mw.b 0x1825889a 0x20 1\0" \
	"android_image=abootimg addr ${loadaddr} ${vendor_boot_comp_addr_r}\0" \
	"android_load_a=run android_mmc; run android_parts_a; run android_read_boot; " \
		"run android_read_vendor; run android_fix_vendor; run android_image\0" \
	"android_args_core=setenv bootargs console=ttyS0,115200 earlycon 8250.nr_uarts=4 " \
		"clk_ignore_unused loop.max_part=7 loglevel=8 ignore_loglevel init=/init " \
		"firmware_class.path=/vendor/firmware mitigations=off\0" \
	"android_args_a=run android_args_core\0" \
	"boot_android_a=run android_setup; run android_load_a; run android_args_a; bootm ${loadaddr}\0"
#else
#define CFG_EXTRA_ENV_SETTINGS \
	"PS1=[LPi4A]# \0"
#endif

#endif /* __TH1520_LPI4A_H */
