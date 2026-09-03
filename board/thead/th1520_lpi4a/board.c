// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023, Yixun Lan <dlan@gentoo.org>
 *
 */

#include <asm/io.h>
#include <blk.h>
#include <cpu_func.h>
#include <dm.h>
#include <dwc3-uboot.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <malloc.h>
#include <part.h>
#include <pwm.h>
#include <string.h>
#include <usb.h>

#define TH1520_AON_FW_PARTITION		"bootpart"
#define TH1520_AON_FW_ADDR		0xffffef8000UL
#define TH1520_AON_FW_SIZE		0x10000UL
#define TH1520_AON_FW_MAGIC		"AON_CONFIG"
#define TH1520_AON_FW_ENTRY_OFFSET	0xc00UL
#define TH1520_E902_START_ADDR		(0xffef8000UL + \
					 TH1520_AON_FW_ENTRY_OFFSET)
#define TH1520_E902_START_REG		0xfffff48044UL
#define TH1520_E902_RESET_REG		0xfffff44024UL
#define TH1520_E902_IOPMP_BASE		0xffffc21000UL
#define TH1520_E902_MBOX_CTL		0xffffc41000UL
#define TH1520_C910_MBOX_CTL		0xffffc38000UL
#define TH1520_C910_E902_LOCAL		0xffffc3d000UL
#define TH1520_E902_C910_REMOTE		0xffffc40000UL
#define TH1520_AP_CLKGEN_BASE		0xffef010000UL
#define TH1520_AP_TEE_RESET_BASE	0xffff015000UL
#define TH1520_CLK_AON2CPU_A2X_CFG	0x134
#define TH1520_CLK_CPU2AON_X2H_CFG	0x138
#define TH1520_CLK_GATE3_CFG		0x208
#define TH1520_RST_MBOX0_CFG		0x24
#define TH1520_RST_MBOX1_CFG		0x28
#define TH1520_RST_CPU2AON_X2H_CFG	0xe4
#define TH1520_RST_AON2CPU_A2X_CFG	0xfc
#define TH1520_MBOX_STA			0x0
#define TH1520_MBOX_CLR			0x4
#define TH1520_MBOX_MASK			0xc
#define TH1520_MBOX_GEN			0x10
#define TH1520_MBOX_INFO0		0x14
#define TH1520_MBOX_INFO7		0x30
#define TH1520_MBOX_GEN_RX_DATA		BIT(6)
#define TH1520_MBOX_ACK_MAGIC		0xdeadbeaf

static int th1520_aon_rpc_selftest(void)
{
	void *c910_ctl = (void *)TH1520_C910_MBOX_CTL;
	void *e902_ctl = (void *)TH1520_E902_MBOX_CTL;
	void *local = (void *)TH1520_C910_E902_LOCAL;
	void *remote = (void *)TH1520_E902_C910_REMOTE;
	const u32 request[7] = { 0x06050702, 0, 0, 0, 0, 0, 0 };
	u32 response[7];
	unsigned int elapsed;
	unsigned int i;

	/* Start from an idle CPU0 <-> E902 channel. */
	writel(BIT(0), c910_ctl + TH1520_MBOX_CLR);
	writel(BIT(0), e902_ctl + TH1520_MBOX_CLR);
	writel(0, local + TH1520_MBOX_GEN);
	writel(0, remote + TH1520_MBOX_GEN);
	for (i = 0; i < ARRAY_SIZE(request); i++) {
		writel(0, local + TH1520_MBOX_INFO0 + i * sizeof(u32));
		writel(request[i], remote + TH1520_MBOX_INFO0 +
		       i * sizeof(u32));
	}
	writel(0, local + TH1520_MBOX_INFO7);
	writel(0, remote + TH1520_MBOX_INFO7);
	writel(TH1520_MBOX_GEN_RX_DATA, remote + TH1520_MBOX_GEN);

	for (elapsed = 0; elapsed < 500; elapsed++) {
		if (readl(local + TH1520_MBOX_INFO0))
			break;
		udelay(1000);
	}

	for (i = 0; i < ARRAY_SIZE(response); i++)
		response[i] = readl(local + TH1520_MBOX_INFO0 +
				    i * sizeof(u32));

	if (elapsed == 500) {
		printf("TH1520: AON RPC self-test timeout\n");
		printf("TH1520: e902 sta=0x%x mask=0x%x txack=0x%x\n",
		       readl(e902_ctl + TH1520_MBOX_STA),
		       readl(e902_ctl + TH1520_MBOX_MASK),
		       readl(local + TH1520_MBOX_INFO7));
		return -ETIMEDOUT;
	}

	/* Release csi_mbox_send() in the E902 firmware. */
	writel(0, local + TH1520_MBOX_INFO0);
	writel(TH1520_MBOX_ACK_MAGIC, remote + TH1520_MBOX_INFO7);
	writel(BIT(0), c910_ctl + TH1520_MBOX_CLR);

	if (response[0] != 0x06c50902 || (response[1] & 0xff)) {
		printf("TH1520: AON RPC self-test bad response 0x%08x 0x%08x\n",
		       response[0], response[1]);
		return -EPROTO;
	}

	printf("TH1520: AON RPC self-test passed in %u ms\n", elapsed);
	return 0;
}

static int th1520_start_aon(void)
{
	struct disk_partition part;
	struct blk_desc *desc;
	void *fw_buf;
	lbaint_t blocks;
	ulong fw_bytes;
	ulong count;
	int ret;

	ret = blk_select_hwpart_devnum(UCLASS_MMC, 0, 0);
	if (ret)
		return ret;

	desc = blk_get_dev("mmc", 0);
	if (!desc)
		return -ENODEV;
	part_init(desc);

	if (part_get_info_by_name(desc, TH1520_AON_FW_PARTITION, &part) < 0)
		return -ENOENT;

	blocks = DIV_ROUND_UP(TH1520_AON_FW_SIZE, desc->blksz);
	if (part.size < blocks)
		return -ENOSPC;

	fw_bytes = blocks * desc->blksz;
	fw_buf = memalign(ARCH_DMA_MINALIGN, fw_bytes);
	if (!fw_buf)
		return -ENOMEM;

	/* The MMC DMA engine cannot address the E902 high memory alias. */
	count = blk_dread(desc, part.start, blocks, fw_buf);
	if (count != blocks) {
		ret = -EIO;
		goto out_free;
	}

	if (memcmp(fw_buf, TH1520_AON_FW_MAGIC,
		   sizeof(TH1520_AON_FW_MAGIC) - 1)) {
		ret = -ENOEXEC;
		goto out_free;
	}

	memcpy((void *)TH1520_AON_FW_ADDR, fw_buf, fw_bytes);
	flush_cache(TH1520_AON_FW_ADDR, fw_bytes);
	free(fw_buf);

	/*
	 * E902 initializes the mailbox before Linux gets a chance to prepare
	 * these clocks.  Keep both interconnect directions and the two mailbox
	 * controllers alive while the firmware boots, otherwise its first
	 * access to the mailbox data window can stall permanently.
	 */
	setbits_le32((void *)(TH1520_AP_CLKGEN_BASE +
			      TH1520_CLK_AON2CPU_A2X_CFG), BIT(8));
	setbits_le32((void *)(TH1520_AP_CLKGEN_BASE +
			      TH1520_CLK_CPU2AON_X2H_CFG), BIT(8));
	setbits_le32((void *)(TH1520_AP_CLKGEN_BASE +
			      TH1520_CLK_GATE3_CFG), BIT(7) | BIT(6));

	/* All reset controls are active-low; one deasserts the reset. */
	setbits_le32((void *)(TH1520_AP_TEE_RESET_BASE +
			      TH1520_RST_MBOX0_CFG), BIT(0));
	setbits_le32((void *)(TH1520_AP_TEE_RESET_BASE +
			      TH1520_RST_MBOX1_CFG), BIT(0));
	setbits_le32((void *)(TH1520_AP_TEE_RESET_BASE +
			      TH1520_RST_CPU2AON_X2H_CFG), BIT(0));
	setbits_le32((void *)(TH1520_AP_TEE_RESET_BASE +
			      TH1520_RST_AON2CPU_A2X_CFG), BIT(0));

	writel(0xffffffff, (void *)(TH1520_E902_IOPMP_BASE + 0xc0));
	/*
	 * Reset the complete E902 subsystem, including the local CLIC state.
	 * This matches the vendor boot flow; a core-only reset can retain stale
	 * interrupt-controller state and leave mailbox IRQ 38 undeliverable.
	 */
	writel(0, (void *)TH1520_E902_RESET_REG);
	udelay(2);
	/* Clear stale C910T/C906/C910R requests while E902 is held reset. */
	writel(0, (void *)(TH1520_E902_MBOX_CTL + TH1520_MBOX_MASK));
	writel(0x7, (void *)(TH1520_E902_MBOX_CTL + TH1520_MBOX_CLR));
	writel(TH1520_E902_START_ADDR, (void *)TH1520_E902_START_REG);
	writel(0x3, (void *)TH1520_E902_RESET_REG);
	mdelay(20);

	printf("TH1520: E902 AON firmware started at 0x%lx from %s\n",
	       TH1520_E902_START_ADDR, TH1520_AON_FW_PARTITION);
	printf("TH1520: AON mbox sta=0x%x mask=0x%x clocks=0x%x\n",
	       readl((void *)(TH1520_E902_MBOX_CTL + TH1520_MBOX_STA)),
	       readl((void *)(TH1520_E902_MBOX_CTL + TH1520_MBOX_MASK)),
	       readl((void *)(TH1520_AP_CLKGEN_BASE + TH1520_CLK_GATE3_CFG)));
	th1520_aon_rpc_selftest();
	return 0;

out_free:
	free(fw_buf);
	return ret;
}

#if defined(CONFIG_USB_DWC3)
#define TH1520_DWC3_BASE		0xffe7040000UL
#define TH1520_USBPHY_TEST_CTRL2	0xffec03f02cUL
#define TH1520_USBPHY_TEST_CTRL3	0xffec03f030UL
#define TH1520_REF_SSP_EN		0xffec03f034UL
#define TH1520_USB3_DRD_SWRST		0xffec02c014UL
#define TH1520_SYSCLK_USB_CTRL		0xfffc02d104UL

static struct dwc3_device th1520_dwc3 = {
	.base = TH1520_DWC3_BASE,
	.maximum_speed = USB_SPEED_SUPER,
	.dr_mode = USB_DR_MODE_PERIPHERAL,
	.legacy_phy_reset_quirk = 1,
	.index = 0,
};

static void th1520_usb_enable(void)
{
	void __iomem *sysclk = (void __iomem *)TH1520_SYSCLK_USB_CTRL;
	void __iomem *ref_ssp = (void __iomem *)TH1520_REF_SSP_EN;

	writel(readl(sysclk) | 0xf, sysclk);
	writel(readl(ref_ssp) | 0x1, ref_ssp);
	udelay(10);

	writel(0, (void __iomem *)TH1520_USB3_DRD_SWRST);
	udelay(1000);
	writel(0x7, (void __iomem *)TH1520_USB3_DRD_SWRST);

	writel(0x77f, (void __iomem *)TH1520_USBPHY_TEST_CTRL3);
	writel(0x15150f0, (void __iomem *)TH1520_USBPHY_TEST_CTRL2);
}

int board_usb_init(int index, enum usb_init_type init)
{
	if (index != 0 || init != USB_INIT_DEVICE)
		return -EINVAL;

	return dwc3_uboot_init(&th1520_dwc3);
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	if (index != 0 || init != USB_INIT_DEVICE)
		return -EINVAL;

	dwc3_uboot_exit(index);
	return 0;
}

int g_dnl_board_usb_cable_connected(void)
{
	return 1;
}
#endif

int board_late_init(void)
{
	int aon_ret;

	aon_ret = th1520_start_aon();
	if (aon_ret)
		printf("TH1520: AON firmware unavailable (%d)\n", aon_ret);

	if (IS_ENABLED(CONFIG_DM_PWM)) {
		struct udevice *pwm;
		int ret;

		ret = uclass_get_device_by_name(UCLASS_PWM,
						"pwm@ffec01c000", &pwm);
		if (!ret)
			ret = pwm_set_config(pwm, 1, 10000000, 10000000);
		if (!ret)
			ret = pwm_set_enable(pwm, 1, true);
		if (ret)
			printf("TH1520: failed to enable fan PWM1 (%d)\n", ret);
		else
			printf("TH1520: fan PWM1 enabled at full speed\n");
	}

	return 0;
}

int board_init(void)
{
	enable_caches();

#if defined(CONFIG_USB_DWC3)
	th1520_usb_enable();
#endif

	return 0;
}
