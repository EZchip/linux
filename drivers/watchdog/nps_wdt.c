/*
* Copyright (c) 2016, Mellanox Technologies. All rights reserved.
*
* This software is available to you under a choice of one of two
* licenses.  You may choose to be licensed under the terms of the GNU
* General Public License (GPL) Version 2, available from the file
* COPYING in the main directory of this source tree, or the
* OpenIB.org BSD license below:
*
*     Redistribution and use in source and binary forms, with or
*     without modification, are permitted provided that the following
*     conditions are met:
*
*      - Redistributions of source code must retain the above
*        copyright notice, this list of conditions and the following
*        disclaimer.
*
*      - Redistributions in binary form must reproduce the above
*        copyright notice, this list of conditions and the following
*        disclaimer in the documentation and/or other materials
*        provided with the distribution.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
* BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
* ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include "nps_wdt.h"

/* module parameters */
static int timeout = NPS_WDT_DEFAULT_TIMEOUT;
module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout,
		"Watchdog timeout in seconds. (1 < timeout < 5, default="
				__MODULE_STRING(NPS_WDT_DEFAULT_TIMEOUT) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
		"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static int nps_wdt_start(struct watchdog_device *wdt_dev)
{
	struct nps_wdt *nps_wdt_dev = watchdog_get_drvdata(wdt_dev);
	struct nps_wdt_soc_wdog_cfglock wdog_cfglock = { .value = 0 };
	struct nps_wdt_soc_wdog_cfg wdog_cfg = { .value = 0 };

	/* unlock watchdog */
	wdog_cfglock.code = NPS_WDT_CFG_CODE_UNLOCK;
	nps_wdt_reg_set(nps_wdt_dev, NPS_WDT_REG_SOC_WDOG_CFGLOCK,
		wdog_cfglock.value);

	/* set watchdog timeout */
	nps_wdt_reg_set(nps_wdt_dev, NPS_WDT_REG_SOC_WDOG_LOAD,
		SECS_TO_WDOG_TICKS(wdt_dev->timeout, nps_wdt_dev));

	/* enable watchdog */
	wdog_cfg.en = 1;
	wdog_cfg.rst_msk = 1;
	nps_wdt_reg_set(nps_wdt_dev, NPS_WDT_REG_SOC_WDOG_CFG, wdog_cfg.value);

	/* lock watchdog */
	nps_wdt_reg_set(nps_wdt_dev, NPS_WDT_REG_SOC_WDOG_CFGLOCK, 0);

	pr_debug("watchdog enabled (timeout = %u sec)\n", wdt_dev->timeout);

	return 0;
}

static int nps_wdt_stop(struct watchdog_device *wdt_dev)
{
	struct nps_wdt *nps_wdt_dev = watchdog_get_drvdata(wdt_dev);
	struct nps_wdt_soc_wdog_cfglock cfglock = { .value = 0 };

	/* unlock watchdog */
	cfglock.code = NPS_WDT_CFG_CODE_UNLOCK;
	nps_wdt_reg_set(nps_wdt_dev, NPS_WDT_REG_SOC_WDOG_CFGLOCK,
		cfglock.value);

	/* disable watchdog */
	nps_wdt_reg_set(nps_wdt_dev, NPS_WDT_REG_SOC_WDOG_CFG, 0);

	/* lock watchdog */
	nps_wdt_reg_set(nps_wdt_dev, NPS_WDT_REG_SOC_WDOG_CFGLOCK, 0);

	pr_debug("watchdog disabled\n");

	return 0;
}

static int nps_wdt_keepalive(struct watchdog_device *wdt_dev)
{
	struct nps_wdt *nps_wdt_dev = watchdog_get_drvdata(wdt_dev);

	nps_wdt_reg_set(nps_wdt_dev, NPS_WDT_REG_SOC_WDOG_SRV,
		NPS_WDT_SRV_RELOAD);

	return 0;
}

static int nps_wdt_set_timeout(struct watchdog_device *wdt_dev,
	unsigned int timeout)
{
	struct nps_wdt *nps_wdt_dev = watchdog_get_drvdata(wdt_dev);
	struct nps_wdt_soc_wdog_cfglock cfglock = { .value = 0 };

	/* unlock watchdog */
	cfglock.code = NPS_WDT_CFG_CODE_UNLOCK;
	nps_wdt_reg_set(nps_wdt_dev, NPS_WDT_REG_SOC_WDOG_CFGLOCK,
		cfglock.value);

	/* set watchdog timeout */
	nps_wdt_reg_set(nps_wdt_dev, NPS_WDT_REG_SOC_WDOG_LOAD,
		SECS_TO_WDOG_TICKS(timeout, nps_wdt_dev));

	/* lock watchdog */
	nps_wdt_reg_set(nps_wdt_dev, NPS_WDT_REG_SOC_WDOG_CFGLOCK, 0);

	wdt_dev->timeout = timeout;

	pr_debug("watchdog set new timeout = %u sec\n", timeout);

	return 0;
}

unsigned int nps_wdt_get_timeleft(struct watchdog_device *wdt_dev)
{
	unsigned int ticks;
	struct nps_wdt *nps_wdt_dev = watchdog_get_drvdata(wdt_dev);

	ticks = nps_wdt_reg_get(nps_wdt_dev, NPS_WDT_REG_SOC_WDOG_CNTVAL);

	return WDOG_TICKS_TO_SECS(ticks, nps_wdt_dev);
}

static const struct watchdog_info nps_wdt_info = {
	.identity = "NPS Watchdog",
	.options = WDIOF_KEEPALIVEPING | WDIOF_SETTIMEOUT,
};

static const struct watchdog_ops nps_wdt_ops = {
	.owner = THIS_MODULE,
	.start = nps_wdt_start,
	.stop = nps_wdt_stop,
	.ping = nps_wdt_keepalive,
	.set_timeout = nps_wdt_set_timeout,
	.get_timeleft = nps_wdt_get_timeleft,
};

static int nps_wdt_probe(struct platform_device *pdev)
{
	struct nps_wdt *nps_wdt_dev;
	struct watchdog_device *wdt_dev;
	struct resource *res_regs;
	struct device *dev = &pdev->dev;
	int ret;

	nps_wdt_dev = devm_kzalloc(dev, sizeof(*nps_wdt_dev), GFP_KERNEL);
	if (!nps_wdt_dev)
		return -ENOMEM;

	res_regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	nps_wdt_dev->regs_base = devm_ioremap_resource(dev, res_regs);
	if (IS_ERR(nps_wdt_dev->regs_base))
		return PTR_ERR(nps_wdt_dev->regs_base);

	wdt_dev = &nps_wdt_dev->wdt_device;
	wdt_dev->info = &nps_wdt_info;
	wdt_dev->ops = &nps_wdt_ops;
	wdt_dev->min_timeout = NPS_WDT_TIMEOUT_MIN;
	wdt_dev->max_timeout = NPS_WDT_TIMEOUT_MAX;
	wdt_dev->parent = dev;

	ret = watchdog_init_timeout(wdt_dev, timeout, dev);
	if (ret) {
		dev_err(dev, "unable to set timeout value %d\n", timeout);
		return ret;
	}

	watchdog_set_nowayout(wdt_dev, nowayout);
	watchdog_set_drvdata(wdt_dev, nps_wdt_dev);

	ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency",
		&nps_wdt_dev->clock_frequency);
	if (ret) {
		dev_err(dev, "input clock not found\n");
		return ret;
	}

	ret = watchdog_register_device(wdt_dev);
	if (ret) {
		dev_err(dev, "error registering watchdog device\n");
		return ret;
	}

	platform_set_drvdata(pdev, wdt_dev);

	dev_info(dev, "NPS watchdog device timeout=%d sec (nowayout=%d)\n",
		wdt_dev->timeout, nowayout);

	return 0;
}

static int nps_wdt_remove(struct platform_device *pdev)
{
	struct nps_wdt *nps_wdt_dev = platform_get_drvdata(pdev);

	watchdog_unregister_device(&nps_wdt_dev->wdt_device);

	return 0;
}

static const struct of_device_id nps_wdt_dt_ids[] = {
	{ .compatible = "ezchip,nps-wdt" },
	{},
};

static struct platform_driver nps_wdt_driver = {
	.probe = nps_wdt_probe,
	.remove = nps_wdt_remove,
	.driver = {
		.name  = "nps-wdt",
		.of_match_table = nps_wdt_dt_ids,
	},
};

module_platform_driver(nps_wdt_driver);

MODULE_AUTHOR("EZchip Semiconductor");
MODULE_DESCRIPTION("Driver for the NPS watchdog");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:nps-wdt");
