/*******************************************************************************

  EZNPS Platform support code
  Copyright(c) 2012 EZchip Technologies.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

*******************************************************************************/

#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/console.h>
#include <linux/if_ether.h>
#include <linux/socket.h>
#include <asm/setup.h>
#include <plat/serial.h>
#include <plat/irq.h>
#include <plat/memmap.h>
#include <plat/net.h>

/*----------------------- Platform Devices -----------------------------*/
unsigned long eznps_he_version = 0;

#ifdef CONFIG_SERIAL_8250
#include <linux/io.h>
#include <linux/serial_8250.h>

unsigned int serial8250_early_in(struct uart_port *port, int offset)
{
	return __raw_readl(port->membase + (offset << 2));
}

void serial8250_early_out(struct uart_port *port, int offset, int value)
{
	__raw_writel(value, port->membase + (offset << 2));
}

static struct plat_serial8250_port uart8250_data[] = {
	{
		.type = PORT_16550A,
		.irq = UART0_IRQ,
		.uartclk = BASE_BAUD * 16,
		.flags = (UPF_FIXED_TYPE | UPF_SKIP_TEST) ,
		.membase = (void __iomem *)UART0_BASE,
		.iotype = UPIO_MEM32,
		.regshift = 2,
		/* asm-generic/io.h always assume LE */
		.serial_in = serial8250_early_in,
		.serial_out = serial8250_early_out,
	},
	{ },
};

static struct platform_device arc_uart8250_dev = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = uart8250_data,
	}
};

#endif	/* CONFIG_SERIAL_8250 */

/*
 * Early Platform Initialization called from setup_arch()
 */
void __init arc_platform_early_init(void)
{
	unsigned long hw_ver, major_ver;
	pr_info("[plat-eznps]: registering early dev resources\n");

#ifdef CONFIG_SERIAL_8250
	/*
	 * This is to make sure that arc uart would be preferred console
	 * despite one/more of following:
	 *   -command line lacked "console=ttyS0" or
	 *   -CONFIG_VT_CONSOLE was enabled (for no reason whatsoever)
	 * Note that this needs to be done after above early console is reg,
	 * otherwise the early console never gets a chance to run.
	 */
	add_preferred_console("ttyS", 0, "115200");
#endif	/* CONFIG_SERIAL_8250 */

	if (running_on_hw)
	{
		hw_ver = __raw_readl(REG_CPU_HW_VER);
		major_ver = (hw_ver & HW_VER_MAJOR_MASK) >> HW_VER_MAJOR_SHIFT;
		eznps_he_version = (major_ver > 1) ? 1 : 0;
	}
}

static struct platform_device *arc_devs[] = {
#if defined(CONFIG_SERIAL_8250)
	&arc_uart8250_dev,
#endif
};

int __init eznps_plat_init(void)
{
	pr_info("[plat-eznps]: registering device resources\n");

	platform_add_devices(arc_devs, ARRAY_SIZE(arc_devs));

	return 0;
}
arch_initcall(eznps_plat_init);

#ifdef CONFIG_EZNPS_NET
static int __init mac_addr_setup(char *mac)
{
	int i, h, l;

	for (i = 0; i < ETH_ALEN; i++) {
		if (i != ETH_ALEN-1 && *(mac + 2) != ':')
			return 1;

		h = hex_to_bin(*mac++);
		if (h == -1)
			return 1;

		l = hex_to_bin(*mac++);
		if (l == -1)
			return 1;

		mac++;
		mac_addr.sa_data[i] = (h << 4) + l;
	}

	return 0;
}

__setup("mac=", mac_addr_setup);

static int  __init add_eznet(void)
{
	struct platform_device *pd;

	pd = platform_device_register_simple("eth", 0, NULL, 0);
	if (IS_ERR(pd))
		pr_err("Fail\n");

	return IS_ERR(pd) ? PTR_ERR(pd) : 0;
}
device_initcall(add_eznet);
#endif /* CONFIG_EZNPS_NET */
