#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/param.h>

/************************************************************************
    This File contains arc specific over-rides for mainline kernel
    functions done primarliy for making linux load faster on SIMULATOR
************************************************************************/

/* vineetg, Noc 29th 2007
    in <include/asm-arc/setup.h> we over-ride calibaret_delay with
    calibate_delay_arc (ARC version) so that init/main.c sees the
    ARC version and not ours.
 */

extern unsigned long loops_per_jiffy;

void __devinit calibrate_delay_arc(void)
{
    /* vineetg, Nov 29th 2007
        If running on Simulator, hardcode to a value which calibate_delay()
        would normally figure out running on simulator.
        This greatly speeds up bootup on sim
     */
    loops_per_jiffy = 0x26e00;

    printk("Bogus %lu.%02lu BogoMIPS (lpj=%lu)\n", loops_per_jiffy/(500000/HZ),
                            (loops_per_jiffy/(5000/HZ)) % 100, loops_per_jiffy);
}
