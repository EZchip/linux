/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Basic keyboard.h just to support the dummy keyboard (CONFIG_DUMMY_KEYB) - amitb
 *
 */

#ifndef _ASM_KEYBOARD_H
#define _ASM_KEYBOARD_H

#ifdef __KERNEL__

#include <linux/config.h>

extern int kbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int kbd_getkeycode(unsigned int scancode);
extern int kbd_translate(unsigned char scancode, unsigned char *keycode,
	char raw_mode);
extern char kbd_unexpected_up(unsigned char keycode);
extern void kbd_leds(unsigned char leds);
extern void kbd_init_hw(void);

#endif /* __KERNEL */

#endif /* _ASM_KEYBOARD_H */
