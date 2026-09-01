// SPDX-License-Identifier: GPL-2.0+
/* T-Head TH1520 PWM controller */

#include <asm/io.h>
#include <clk.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/read.h>
#include <div64.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <pwm.h>

#define TH1520_PWM_CHANNELS		6
#define TH1520_PWM_CH_BASE(n)		((n) * 0x20)
#define TH1520_PWM_CTRL(n)		(TH1520_PWM_CH_BASE(n) + 0x00)
#define TH1520_PWM_PERIOD(n)		(TH1520_PWM_CH_BASE(n) + 0x08)
#define TH1520_PWM_DUTY(n)		(TH1520_PWM_CH_BASE(n) + 0x0c)

#define TH1520_PWM_START		BIT(0)
#define TH1520_PWM_CFG_UPDATE		BIT(2)
#define TH1520_PWM_CONTINUOUS		BIT(5)
#define TH1520_PWM_FPOUT		BIT(8)

struct th1520_pwm_priv {
	void __iomem *base;
	struct clk clk;
	ulong rate;
};

static int th1520_pwm_set_config(struct udevice *dev, uint channel,
				  uint period_ns, uint duty_ns)
{
	struct th1520_pwm_priv *priv = dev_get_priv(dev);
	u64 period_cycles, duty_cycles;
	u32 ctrl;

	if (channel >= TH1520_PWM_CHANNELS || !period_ns || duty_ns > period_ns)
		return -EINVAL;

	period_cycles = (u64)period_ns * priv->rate;
	do_div(period_cycles, 1000000000ULL);
	duty_cycles = (u64)duty_ns * priv->rate;
	do_div(duty_cycles, 1000000000ULL);
	if (!period_cycles || period_cycles > (u64)~(u32)0 ||
	    duty_cycles > (u64)~(u32)0)
		return -ERANGE;

	ctrl = TH1520_PWM_CONTINUOUS | TH1520_PWM_FPOUT;
	writel(ctrl, priv->base + TH1520_PWM_CTRL(channel));
	writel((u32)period_cycles, priv->base + TH1520_PWM_PERIOD(channel));
	writel((u32)duty_cycles, priv->base + TH1520_PWM_DUTY(channel));
	writel(ctrl | TH1520_PWM_CFG_UPDATE,
	       priv->base + TH1520_PWM_CTRL(channel));

	return 0;
}

static int th1520_pwm_set_enable(struct udevice *dev, uint channel, bool enable)
{
	struct th1520_pwm_priv *priv = dev_get_priv(dev);
	u32 ctrl;

	if (channel >= TH1520_PWM_CHANNELS)
		return -EINVAL;

	ctrl = readl(priv->base + TH1520_PWM_CTRL(channel));
	if (enable)
		ctrl |= TH1520_PWM_START;
	else
		ctrl &= ~TH1520_PWM_START;
	writel(ctrl, priv->base + TH1520_PWM_CTRL(channel));

	return 0;
}

static int th1520_pwm_probe(struct udevice *dev)
{
	struct th1520_pwm_priv *priv = dev_get_priv(dev);
	int ret;

	priv->base = dev_read_addr_ptr(dev);
	if (!priv->base)
		return -EINVAL;

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret)
		return ret;
	ret = clk_enable(&priv->clk);
	if (ret)
		return ret;

	priv->rate = clk_get_rate(&priv->clk);
	if (!priv->rate)
		return -EINVAL;

	return 0;
}

static const struct pwm_ops th1520_pwm_ops = {
	.set_config = th1520_pwm_set_config,
	.set_enable = th1520_pwm_set_enable,
};

static const struct udevice_id th1520_pwm_ids[] = {
	{ .compatible = "thead,th1520-pwm" },
	{ }
};

U_BOOT_DRIVER(th1520_pwm) = {
	.name = "th1520_pwm",
	.id = UCLASS_PWM,
	.of_match = th1520_pwm_ids,
	.ops = &th1520_pwm_ops,
	.probe = th1520_pwm_probe,
	.priv_auto = sizeof(struct th1520_pwm_priv),
};
