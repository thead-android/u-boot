// SPDX-License-Identifier: GPL-2.0+
/*
 * T-Head TH1520 system reset driver
 *
 * The reset sequence is based on the vendor U-Boot implementation.  WDT0
 * must first be allowed to request a system reset through the always-on reset
 * generator before the watchdog is enabled.
 */

#include <dm.h>
#include <sysreset.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/errno.h>

#define TH1520_AON_RSTGEN_BASE		0xfffff44000ULL
#define TH1520_RST_REQ_EN_0		(TH1520_AON_RSTGEN_BASE + 0x140)
#define TH1520_WDT0_SYS_RST_REQ		BIT(8)

#define TH1520_WDT0_BASE		0xffefc30000ULL
#define TH1520_WDT_CR			(TH1520_WDT0_BASE + 0x0)
#define TH1520_WDT_TORR			(TH1520_WDT0_BASE + 0x4)

static int th1520_sysreset_request(struct udevice *dev, enum sysreset_t type)
{
	if (type != SYSRESET_WARM && type != SYSRESET_COLD)
		return -EPROTONOSUPPORT;

	setbits_le32((void __iomem *)TH1520_RST_REQ_EN_0,
		     TH1520_WDT0_SYS_RST_REQ);
	writel(1, (void __iomem *)TH1520_WDT_CR);
	writel(1, (void __iomem *)TH1520_WDT_TORR);

	return -EINPROGRESS;
}

static const struct sysreset_ops th1520_sysreset_ops = {
	.request = th1520_sysreset_request,
};

static const struct udevice_id th1520_sysreset_ids[] = {
	{ .compatible = "thead,th1520-sysreset" },
	{ }
};

U_BOOT_DRIVER(th1520_sysreset) = {
	.name = "th1520_sysreset",
	.id = UCLASS_SYSRESET,
	.of_match = th1520_sysreset_ids,
	.ops = &th1520_sysreset_ops,
};
