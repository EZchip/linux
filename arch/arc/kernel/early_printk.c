/*
 * arch/arc/kernel/early_printk.c
 *
 *  Copyright (C) 1999, 2000  Niibe Yutaka
 *  Copyright (C) 2002  M. R. Brown
 *  Copyright (C) 2004 - 2007  Paul Mundt
 *  6/1/09 - Modified by Simon Spooner of ARC to support ARC early console
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>


// Demonstrating the earlyprintk functionality
// This uses the second serial port as early printk
// once the console is fully initialized control
// is transferred back the regular console.


#define UART_BASE   0xc0fc1100      // UART1 base address
#define UART_BAUDL  0x18
#define UART_BAUDH  0x1c
#define UART_STATUS 0x14
#define UART_TXRX   0x10
#define UART_RXENB  0x04
#define UART_TXEMPTY 0x80



extern void arcconsole_write(struct console *cp, const char *p, unsigned len);
extern int arcconsole_setup(struct console *cp, char *arg);

extern int arc_console_baud;

// Low level console write.


static void arc_console_write(struct console *co, const char *s,
            unsigned count)
{

    volatile unsigned char *status = (unsigned char *)UART_BASE + UART_STATUS;
    volatile unsigned char *tx = (unsigned char *)UART_BASE + UART_TXRX;
    while (count --)
    {
        while(!(*status & UART_TXEMPTY));

        *tx = *s;
        if (*s == 0x0a)  // Handle CR/LF.
        {
            while(!(*status & UART_TXEMPTY));
            *tx = 0x0d;
        }
        s++;
    }

}

// Initialize the UART.

static int __init arc_console_setup (struct console *co, char *options)
{

//    int cflag = CREAD | HUPCL | CLOCAL;
    volatile unsigned char *baudh = (unsigned char *)UART_BASE + UART_BAUDH;
    volatile unsigned char *baudl = (unsigned char *)UART_BASE + UART_BAUDL;
    volatile unsigned char *status = (unsigned char *)UART_BASE+ UART_STATUS;

    *status = UART_RXENB;
    *baudl = (arc_console_baud & 0xff);
    *baudh = (arc_console_baud & 0xff00) >> 8;

//    cflag |= B57600 | CS8 | 0;
//    co->cflag = cflag;

    return 0;
}





static struct console arc_console = {
    .name = "arc_console",

    .write = arc_console_write,
    .setup = arc_console_setup,

// You can use the regular serial console too
// rather than this included version.
//    .write = arcconsole_write,
//    .setup = arcconsole_setup,
    .flags = CON_PRINTBUFFER,
    .index = -1.
    };


static struct console *early_console = &arc_console;

static int __init setup_early_printk(char *buf)
{
	int keep_early = 0;

	if (!buf)
		return 0;

printk("Setting up early_printk\n");


	if (strstr(buf, "keep"))
		keep_early = 1;

    early_console = &arc_console;

	if (likely(early_console)) {
		if (keep_early)
			early_console->flags &= ~CON_BOOT;
		else
			early_console->flags |= CON_BOOT;
		register_console(early_console);
	}

	return 0;
}
early_param("earlyprintk", setup_early_printk);
