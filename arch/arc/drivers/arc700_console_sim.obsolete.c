/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

/*
 * (C) 2003, ARC International
 *
 * Amit Shah
 *
 * Driver for console on serial interface for the ArcAngel3 board. We
 * use VUART0 as the console device.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <asm/irq.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/serial.h>

#undef SERIAL_PARANOIA_CHECK
#define RX_DELAY 10
#define RX_START_DELAY 600

static struct aa3_serial aa3_info[NR_PORTS];

int console_inited = 0;		/* have we initialized the console already? */

struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

static struct tty_struct *serial_table[NR_PORTS];
static struct termios *serial_termios[NR_PORTS];
static struct termios *serial_termios_locked[NR_PORTS];

DECLARE_TASK_QUEUE(tq_serial);

/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the memcpy_fromfs blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
DECLARE_MUTEX(tmp_buf_sem);

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

/* Buffers for transmit and receive */
static unsigned char *tmp_buf, *rcv_buf;
//#define BUF_SIZE 256
#define BUF_SIZE 8192
static struct timer_list rx;

extern void aa3_console_sim_write(struct console *cp, const char *p,
				  unsigned len);

static inline int serial_paranoia_check(struct aa3_serial *info,
					kdev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
	    "Warning: bad magic number for serial struct (%s) in %s\n";
	static const char *badinfo =
	    "Warning: null async_struct for (%s) in %s\n";

	if (!info) {
		printk(badinfo, kdevname(device), routine);
		return 1;
	}
	if (info->magic != SERIAL_MAGIC) {
		printk(badmagic, kdevname(device), routine);
		return 1;
	}
#endif
	return 0;
}

static int aa3_write(struct tty_struct *tty, int from_user,
		     const unsigned char *buf, int count)
{
	int c, ret = 0;
	struct console arc_aa3_console_sim;

	while (1) {
		c = BUF_SIZE;	/* tmp_buf is just one page long */
		if (count < c)
			c = count;
		if (c <= 0)
			break;

		if (from_user) {
			copy_from_user(tmp_buf, buf, c);
			/* no error checking as of now */
		} else {
			memcpy(tmp_buf, buf, c);
		}
		/* call aa3_console_sim_write which calls the HostLink write
		   function _hl_write */
		aa3_console_sim_write(&arc_aa3_console_sim, tmp_buf, c);
		ret += c;
		count -= c;
	}

	return ret;
}

/*
 * This routine is used to handle the "bottom half" processing for the
 * serial driver, known also the "software interrupt" processing.
 * This processing is done at the kernel interrupt level, after the
 * arcserial_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using arcserial_sched_event(), and they get done here.
 */
static void do_serial_bh(void)
{
	run_task_queue(&tq_serial);
}

static void do_softint(void *private_)
{
	struct aa3_serial *info = (struct aa3_serial *)private_;
	struct tty_struct *tty;

	tty = info->tty;
	if (!tty)
		return;

	if (test_and_clear_bit(ARCSERIAL_EVENT_WRITE_WAKEUP, &info->event)) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup) (tty);
		wake_up_interruptible(&tty->write_wait);
	}
}

/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static inline void arcserial_sched_event(struct aa3_serial *info, int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}

static void receive_chars(struct aa3_serial *info, int count)
{
	struct tty_struct *tty = info->tty;
	unsigned char ch;
	int i = 0;

	do {
		ch = rcv_buf[i];
		if (!tty)
			goto clear_and_exit;

		/*
		 * Make sure that we do not overflow the buffer
		 */
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			queue_task(&tty->flip.tqueue, &tq_timer);
			return;
		}

		*tty->flip.char_buf_ptr++ = ch;
		tty->flip.count++;
		i++;
	} while (i < count);

	queue_task(&tty->flip.tqueue, &tq_timer);

      clear_and_exit:
	return;
}

void rx_poll(void)
{
	struct aa3_serial *info;
	unsigned long expires;
	int count;

	info = &aa3_info[0];
	if (!info)
		return;

	/* Call the HostLik read function, it behaves in the same way
	   as read() */
	count = _hl_read(0, rcv_buf, BUF_SIZE);
	/* Give the data read to the tty receive buffer */
	if (count > 0) {
		receive_chars(info, count);
	}

	/* set the timer once again to poll after 10 jiffies i.e. 100ms */
	expires = jiffies + RX_DELAY;
	mod_timer(&rx, expires);
}

static void do_serial_hangup(void *private_)
{
}

/*
 * ------------------------------------------------------------
 * aa3_close()
 *
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void aa3_close(struct tty_struct *tty, struct file *filp)
{

}

static void aa3_set_ldisc(struct tty_struct *tty)
{

}

static void aa3_flush_chars(struct tty_struct *tty)
{
	return;
}

static int aa3_write_room(struct tty_struct *tty)
{
	return BUF_SIZE;
}

static int aa3_chars_in_buffer(struct tty_struct *tty)
{
	return 0;
}

static void aa3_flush_buffer(struct tty_struct *tty)
{
	return;
}

static int aa3_ioctl(struct tty_struct *tty, struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	return -ENOIOCTLCMD;
}

static void aa3_throttle(struct tty_struct *tty)
{

}

static void aa3_unthrottle(struct tty_struct *tty)
{

}

static void aa3_set_termios(struct tty_struct *tty, struct termios *old_termios)
{

}

static void aa3_stop(struct tty_struct *tty)
{

}

static void aa3_start(struct tty_struct *tty)
{

}

static void aa3_hangup(struct tty_struct *tty)
{

}

static int aa3_readproc(char *page, char **start, off_t off, int count,
			int *eof, void *data)
{

	return 0;
}

static void change_speed(struct aa3_serial *info)
{
}

static int startup(struct aa3_serial *info)
{
	unsigned long flags;

	if (info->flags & ASYNC_INITIALIZED)
		return 0;

	if (!info->xmit_buf) {
//              info->xmit_buf = (unsigned char *)kmalloc(BUF_SIZE, GFP_KERNEL);
		info->xmit_buf = (unsigned char *)get_free_page(GFP_KERNEL);
		if (!info->xmit_buf)
			return -ENOMEM;
	}

	save_flags(flags);
	cli();

	/*
	 * Clear the FIFO buffers and disable them
	 * (they will be reenabled in change_speed())
	 */

	info->xmit_fifo_size = 1;

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;

	/*
	 * and set the speed of the serial port
	 */
	change_speed(info);

	info->flags |= ASYNC_INITIALIZED;
	restore_flags(flags);
	return 0;
}

static int block_til_ready(struct tty_struct *tty, struct file *filp,
			   struct aa3_serial *info)
{
	DECLARE_WAITQUEUE(wait, current);
	int retval;
	int do_clocal = 0;

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (info->flags & ASYNC_CLOSING) {
		interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		if (info->flags & ASYNC_HUP_NOTIFY)
			return -EAGAIN;
		else
			return -ERESTARTSYS;
#else
		return -EAGAIN;
#endif
	}

	/*
	 * If this is a callout device, then just make sure the normal
	 * device isn't being used.
	 */
	if (tty->driver.subtype == SERIAL_TYPE_CALLOUT) {
		if (info->flags & ASYNC_NORMAL_ACTIVE)
			return -EBUSY;
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_SESSION_LOCKOUT) &&
		    (info->session != current->session))
			return -EBUSY;
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_PGRP_LOCKOUT) &&
		    (info->pgrp != current->pgrp))
			return -EBUSY;
		info->flags |= ASYNC_CALLOUT_ACTIVE;
		return 0;
	}

	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) || (tty->flags & (1 << TTY_IO_ERROR))) {
		if (info->flags & ASYNC_CALLOUT_ACTIVE)
			return -EBUSY;
		info->flags |= ASYNC_NORMAL_ACTIVE;
		return 0;
	}

	if (info->flags & ASYNC_CALLOUT_ACTIVE) {
		if (info->normal_termios.c_cflag & CLOCAL)
			do_clocal = 1;
	} else {
		if (tty->termios->c_cflag & CLOCAL)
			do_clocal = 1;
	}

	/*
	 * Block waiting for the carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, info->count is dropped by one, so that
	 * arcserial_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);

	info->count--;
	info->blocked_open++;
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		if (tty_hung_up_p(filp) || !(info->flags & ASYNC_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
			if (info->flags & ASYNC_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    !(info->flags & ASYNC_CLOSING) && do_clocal)
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&info->open_wait, &wait);
	if (!tty_hung_up_p(filp))
		info->count++;
	info->blocked_open--;

	if (retval)
		return retval;
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
}

int aa3_open(struct tty_struct *tty, struct file *filp)
{
	struct aa3_serial *info;
	int retval, line;

	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PORTS)) {
		printk("AA3 serial driver: check your lines\n");
		return -ENODEV;
	}

	info = &aa3_info[line];

	if (serial_paranoia_check(info, tty->device, "aa3_open"))
		return -ENODEV;

	info->count++;
	tty->driver_data = info;
	info->tty = tty;

	/*
	 * Start up serial port
	 */
	retval = startup(info);
	if (retval)
		return retval;

	retval = block_til_ready(tty, filp, info);
	if (retval)
		return retval;

	if ((info->count == 1) && (info->flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->normal_termios;
		else
			*tty->termios = info->callout_termios;
		change_speed(info);
	}

	info->session = current->session;
	info->pgrp = current->pgrp;

	if (!tmp_buf) {
//              tmp_buf = (unsigned char *)kmalloc(BUF_SIZE, GFP_KERNEL);
		tmp_buf = (unsigned char *)get_free_page(GFP_KERNEL);
		if (!tmp_buf)
			return -ENOMEM;
	}

	if (!rcv_buf) {
//              rcv_buf = (unsigned char *)kmalloc(BUF_SIZE, GFP_KERNEL);
		rcv_buf = (unsigned char *)get_free_page(GFP_KERNEL);
		if (!rcv_buf)
			return -ENOMEM;
	}

	return 0;
}

static int __init aa3_serial_init(void)
{
	struct aa3_serial *info;
	int flags, i;

	/* Setup base handler, and timer table. */
	init_bh(SERIAL_BH, do_serial_bh);

	/* Initialize the tty_driver structure */
	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.name = "ttyS";
	serial_driver.major = TTY_MAJOR;
	serial_driver.minor_start = 64;
	serial_driver.num = NR_PORTS;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;

	serial_driver.init_termios.c_cflag =
	    B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver.flags = TTY_DRIVER_REAL_RAW;
	serial_driver.refcount = &serial_refcount;
	serial_driver.table = serial_table;
	serial_driver.termios = serial_termios;
	serial_driver.termios_locked = serial_termios_locked;

	serial_driver.open = aa3_open;
	serial_driver.close = aa3_close;
	serial_driver.write = aa3_write;
	serial_driver.flush_chars = aa3_flush_chars;
	serial_driver.write_room = aa3_write_room;
	serial_driver.chars_in_buffer = aa3_chars_in_buffer;
	serial_driver.flush_buffer = aa3_flush_buffer;
	serial_driver.ioctl = aa3_ioctl;
	serial_driver.throttle = aa3_throttle;
	serial_driver.unthrottle = aa3_unthrottle;
	serial_driver.set_termios = aa3_set_termios;
	serial_driver.stop = aa3_stop;
	serial_driver.start = aa3_start;
	serial_driver.hangup = aa3_hangup;
	serial_driver.set_ldisc = aa3_set_ldisc;
	serial_driver.driver_name = "serial";

	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	callout_driver = serial_driver;
	callout_driver.name = "cua";
	callout_driver.major = TTYAUX_MAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;

	if (tty_register_driver(&serial_driver)) {
		printk("%s : Couldn't register serial driver\n", __FUNCTION__);
		return (-EBUSY);
	}
	if (tty_register_driver(&callout_driver)) {
		printk("%s : Couldn't register callout driver\n", __FUNCTION__);
		return (-EBUSY);
	}

	save_flags(flags);
	cli();

	for (i = 0; i < NR_PORTS; i++) {

		info = &aa3_info[i];
		info->magic = SERIAL_MAGIC;
		info->tty = 0;
		info->custom_divisor = 16;
		info->close_delay = 50;
		info->closing_wait = 3000;
		info->x_char = 0;
		info->event = 0;
		info->count = 0;
		info->blocked_open = 0;
		info->tqueue.routine = do_softint;
		info->tqueue.data = info;
		info->tqueue_hangup.routine = do_serial_hangup;
		info->tqueue_hangup.data = info;
		info->callout_termios = callout_driver.init_termios;
		info->normal_termios = serial_driver.init_termios;
		init_waitqueue_head(&info->open_wait);
		init_waitqueue_head(&info->close_wait);
		info->line = i;
		info->is_cons = 1;

	}
	/* Timer Added for polling */
	init_timer(&rx);
	rx.data = 0;
	rx.expires = jiffies + RX_START_DELAY;
	rx.function = rx_poll;
	add_timer(&rx);

	restore_flags(flags);
	return 0;
}

module_init(aa3_serial_init);

int aa3_console_sim_setup(struct console *cp, char *arg)
{
	console_inited++;

	return 0;		/* successful initialization */
}

static kdev_t aa3_console_sim_device(struct console *c)
{
	return mk_kdev(TTY_MAJOR, 64 + c->index);
}

void aa3_console_sim_write(struct console *cp, const char *p, unsigned len)
{
	if (!console_inited)
		aa3_console_sim_setup(cp, NULL);
	/* _hl_write(fdhandle,buffer,length :
	 * where fdhandle is the file desc number
	 */
	_hl_write(1, p, len);
}

/*
 * declare our consoles
 */

struct console arc_aa3_console_sim = {
	.name = "ttyS",
	.write = aa3_console_sim_write,
	.read = NULL,
	.device = aa3_console_sim_device,
	.unblank = NULL,
	.setup = aa3_console_sim_setup,
	.flags = CON_PRINTBUFFER,
	.index = -1,
	.cflag = 0,
	.next = NULL
};

void __init aa3_console_sim_init(void)
{
	register_console(&arc_aa3_console_sim);
}

/* dummy functions needed by HostLink libhlt.a */

void _mwmutex_lock(void)
{
}
void _mwmutex_unlock(void)
{
}
int __errno(void)
{
	return 0;
}
