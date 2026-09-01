// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2023, Yixun Lan <dlan@gentoo.org>
 *
 */

#include <asm/io.h>
#include <cpu_func.h>
#include <dm.h>
#include <dwc3-uboot.h>
#include <linux/delay.h>
#include <pwm.h>
#include <usb.h>

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

#if CONFIG_IS_ENABLED(DM_PWM)
int board_late_init(void)
{
	struct udevice *pwm;
	int ret;

	ret = uclass_get_device_by_name(UCLASS_PWM, "pwm@ffec01c000", &pwm);
	if (ret)
		goto err;

	ret = pwm_set_config(pwm, 1, 10000000, 10000000);
	if (!ret)
		ret = pwm_set_enable(pwm, 1, true);
	if (!ret) {
		printf("TH1520: fan PWM1 enabled at full speed\n");
		return 0;
	}
err:
	printf("TH1520: failed to enable fan PWM1 (%d)\n", ret);
	return 0;
}
#endif

int board_init(void)
{
	enable_caches();

#if defined(CONFIG_USB_DWC3)
	th1520_usb_enable();
#endif

	return 0;
}
