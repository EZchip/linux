/******************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2009
 *
 * Vineetg: Jan 2009
 *  -Added Code to lookup symbol name from address so that callsites are not
 *   cluttered with KALLSYMS and friends
 *
 *****************************************************************************/
#include <linux/types.h>
#include <asm/utils.h>
#include <linux/kallsyms.h>

extern int snprintf(char * buf, size_t size, const char *fmt, ...);

/* This global would render arc_identify_sym() non-re-entrant
 * However it is only for debugging so I can live with it without
 * having nightmares of guilt or urge to suicide
 */
static char pvt_buf[KSYM_NAME_LEN+1];

/* Actually I coudn't live it for 15 minutes.
 * So decided to write a re-entrant version as well
 * Peace at last !!!
 */

char *arc_identify_sym(unsigned long addr)
{
    return arc_identify_sym_r(addr, pvt_buf, sizeof(pvt_buf));
}

char *arc_identify_sym_r(unsigned long addr, char *namebuf, size_t sz)
{
#ifdef CONFIG_KALLSYMS
    char *modname;
    const char *name;
    unsigned long offset, size;

    if ( sz >= KSYM_NAME_LEN && addr ) {

        name = kallsyms_lookup(addr, &size, &offset, &modname, namebuf);

        if(name) {
            if(modname) {
                snprintf(namebuf, sz, "%s+%#lx[%s]",name, offset, modname);
            }
            else {
                snprintf(namebuf, sz, "%s+%#lx",name, offset);
            }
            return namebuf;
        }
    }
#endif
    snprintf(namebuf, sz, "%#lx",addr);
    return namebuf;
}
