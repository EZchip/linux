
#include <linux/interrupt.h>
#include <linux/console.h>
#include <asm/io.h>
#include <linux/serial_8250.h>

#ifdef CONFIG_SERIAL_8250

static struct plat_serial8250_port uart8250_data[] = {
	{
			.type	 = PORT_16550A,
			.irq     = 5,
			.uartclk = CONFIG_ARC700_CLK,
			.iotype  = UPIO_MEM32,
			.flags   = (UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_IOREMAP) ,
			.mapbase = 0xC0000000,
			.regshift= 2, /* Each reg is 32 bit physically */
	},
	{ },
};

static struct platform_device arc_uart8250_device = {
	.name			= "serial8250",
	.id			= PLAT8250_DEV_PLATFORM,
	.dev			= {
		.platform_data	= uart8250_data,
	}
};

static struct platform_device *arc_devices[] __initdata = {
		&arc_uart8250_device,
};
static int __init arc_add_devices(void)
{
	int err;
	err = platform_add_devices(arc_devices, ARRAY_SIZE(arc_devices));
	if (err)
		return err;
	return 0;
}

device_initcall(arc_add_devices);


#ifdef CONFIG_EARLY_PRINTK

void prom_putchar(unsigned char c)
{
	void __iomem *base = (void __iomem *)0xC0000000;
	 int timeout, i;
	/* check LSR TX_EMPTY bit */
	timeout = 0xfff;
	do {
	 if (__raw_readl(base + 0x14) & 0x20)
	break;
	 /* slow down */
	for (i = 100; i; i--)
	asm volatile ("nop");
	} while (--timeout);
	__raw_writel(c, base + 0x00);   /* tx */
	wmb();
}

static void __init
early_console_write(struct console *con, const char *s, unsigned n)
{
	while (n-- && *s) {
		if (*s == '\n')
			prom_putchar('\r');
		prom_putchar(*s);
		s++;
	}
}

static struct console early_console __initdata = {
	.name	= "early",
	.write	= early_console_write,
	.flags	= CON_PRINTBUFFER | CON_BOOT,
	.index	= -1
};
void __init arc_early_serial_reg(void)
{
    register_console(&early_console);
}

#endif
#endif /*CONFIG_SERIAL_8250*/
