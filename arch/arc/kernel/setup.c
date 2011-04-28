/******************************************************************************
 * Copyright ARC International (www.arc.com) 2008-2010
 *
 *  Rajeshwarr: June 2009
 *    -Tidy up the Bootmem Allocator setup
 *    -We were skipping OVER "1" page in bootmem setup
 *
 *  Vineetg: June 2009
 *    -Tag parsing done ONLY if bootloader passes it, otherwise defaults
 *     for those params are already setup at compile time.
 *
 *  Vineetg: April 2009
 *    -NFS and IDE (hda2) root filesystem works now
 *
 *  Vineetg: Jan 30th 2008
 *    -setup_processor() is now CPU init API called by all CPUs
 *        It includes TLB init, Vect Tbl Iit, Clearing ALL IRQs etc
 *    -cpuinfo_arc700 is now an array of NR_CPUS; contains MP info as well
 *    -/proc/cpuinfo iterator fixed so that show_cpuinfo() gets
 *        correct CPU-id to display CPU specific info
 *
 *****************************************************************************/

/*****************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/mmzone.h>
#include <linux/bootmem.h>
#include <asm/pgtable.h>
#include <asm/serial.h>
#include <asm/arcregs.h>
#include <asm/tlb.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/cache.h>
#include <asm/irq.h>
#include <linux/socket.h>
#include <linux/root_dev.h>
#include <linux/initrd.h>
#include <linux/console.h>

#include <asm/unwind.h>
#include <linux/module.h>

extern int _text, _etext, _edata, _end;
extern unsigned long end_kernel;
extern unsigned long ramd_start, ramd_end;
extern int root_mountflags;

extern void arc_irq_init(void);
extern char * arc_mmu_mumbojumbo(int cpu_id, char *buf);
extern char * arc_cache_mumbojumbo(int cpu_id, char *buf);
extern void __init arc_verify_sig_sz(void);


struct cpuinfo_arc cpuinfo_arc700[NR_CPUS];

/* Important System variables
 * We start with compile time DEFAULTS and over-ride ONLY if bootloader
 * passes atag list
 */

unsigned long end_mem = CONFIG_SRAM_BASE + CONFIG_SRAM_SIZE + PHYS_SRAM_OFFSET;
unsigned long clk_speed = CONFIG_ARC700_CLK;
unsigned long serial_baudrate = BASE_BAUD;
int arc_console_baud = (CONFIG_ARC700_CLK/(BASE_BAUD * 4)) - 1;
struct sockaddr mac_addr = {0, {0x64,0x66,0x46,0x88,0x63,0x33 } };

#ifdef CONFIG_ROOT_NFS

// Example of NFS root booting.
char __initdata command_line[COMMAND_LINE_SIZE] = {"root=/dev/nfs nfsroot=172.16.0.196:/shared,nolock ip=dhcp,console=ttyS0" };

#elif CONFIG_ARC_UART_CONSOLE

/* with console=tty0, arc uart console will be prefered console and
 * registrations will be successful, otherwise dummy console will be
 * registered if CONFIG_VT_CONSOLE is enabled
 */
char __initdata command_line[COMMAND_LINE_SIZE] = {"console=ttyS0"};

#else

// Clean, no kernel command line.
char __initdata command_line[COMMAND_LINE_SIZE];

// Use this next line to temporarily switch on "earlyprintk"
//char __initdata command_line[COMMAND_LINE_SIZE] = {"earlyprintk=1"};

#endif

struct task_struct *_current_task[NR_CPUS];  /* currently active task */

/* A7 does not autoswitch stacks on interrupt or exception ,so we do it
 * manually.But we need one register for each interrupt level to play with
 * which is saved and restored from the following variable.
 */

/* vineetg: Jan 10th 2008:
 * In SMP we use MMU_SCRATCH_DATA0 to save a temp reg to make the ISR
 * re-entrant. However this causes performance penalty in TLB Miss code
 * because SCRATCH_DATA0 no longer stores the PGD ptr, which now has to be
 * fetched with 3 mem accesse as follows: curr_task->active_mm->pgd
 * Thus in non SMP we retain the old scheme of using int1_saved_reg in ISR
 */
#ifndef CONFIG_SMP
volatile unsigned long int1_saved_reg;
#endif

volatile unsigned long int2_saved_reg;

/*
 * CPU info code now more organised. Instead of stupid if else,
 * added tables which can be run thru
 */
typedef struct {
    int id;
    const char *str;
    int up_range;
}
cpuinfo_data_t;


#define READ_BCR(reg, into)             \
{                                       \
    unsigned int tmp;                   \
    tmp = read_new_aux_reg(reg);        \
    if (sizeof(tmp) == sizeof(into))    \
        into = *((typeof(into) *)&tmp); \
    else  {                             \
        extern void bogus_undefined(void);\
        bogus_undefined();              \
    }                                   \
}

int __init read_arc_build_cfg_regs(void)
{
    int cpu = smp_processor_id();
    struct cpuinfo_arc *p_cpu = &cpuinfo_arc700[cpu];

    READ_BCR(AUX_IDENTITY, p_cpu->cpu);

    p_cpu->timers = read_new_aux_reg(ARC_REG_TIMERS_BCR);
    p_cpu->vec_base = read_new_aux_reg(AUX_INTR_VEC_BASE);
    p_cpu->perip_base = read_new_aux_reg(ARC_REG_PERIBASE_BCR);
    READ_BCR(ARC_REG_D_UNCACH_BCR, p_cpu->uncached_space);

    p_cpu->extn.mul = read_new_aux_reg(ARC_REG_MUL_BCR);
    p_cpu->extn.swap = read_new_aux_reg(ARC_REG_SWAP_BCR);
    p_cpu->extn.norm = read_new_aux_reg(ARC_REG_NORM_BCR);
    p_cpu->extn.minmax = read_new_aux_reg(ARC_REG_MIXMAX_BCR);
    p_cpu->extn.barrel = read_new_aux_reg(ARC_REG_BARREL_BCR);
    READ_BCR(ARC_REG_MAC_BCR, p_cpu->extn_mac_mul);

    p_cpu->extn.ext_arith = read_new_aux_reg(ARC_REG_EXTARITH_BCR);
    p_cpu->extn.crc = read_new_aux_reg(ARC_REG_CRC_BCR);
    p_cpu->extn.dccm = read_new_aux_reg(ARC_REG_DCCM_BCR);
    p_cpu->extn.iccm = read_new_aux_reg(ARC_REG_ICCM_BCR);

    READ_BCR(ARC_REG_XY_MEM_BCR, p_cpu->extn_xymem);

#ifdef CONFIG_ARCH_ARC800
    READ_BCR(ARC_REG_MP_BCR, p_cpu->mp);
#endif

    return cpu;
}


char * arc_cpu_mumbojumbo(int cpu_id, char *buf)
{
    int i, num=0;
    struct cpuinfo_arc *p_cpu = & cpuinfo_arc700[cpu_id];

    cpuinfo_data_t cpu_fam_nm [] = {
        { 0x10, "ARCTangent A5", 0x1F },
        { 0x20, "ARC 600", 0x2F },
        { 0x30, "ARC 700", 0x3F },
        { 0x0, NULL }
    };

    for ( i = 0; cpu_fam_nm[i].id != 0; i++) {
        if ( (p_cpu->cpu.family >=  cpu_fam_nm[i].id ) &&
             (p_cpu->cpu.family <=  cpu_fam_nm[i].up_range) )
        {
            num += sprintf(buf, "\n Processor Family: %s, Core ID: %d\n",
                    cpu_fam_nm[i].str, p_cpu->cpu.cpu_id);
            break;
        }
    }

    if ( cpu_fam_nm[i].id == 0 )
        num += sprintf(buf, "UNKNOWN ARC Processor\n");

    num += sprintf(buf+num, "CPU speed :\t%u.%02u Mhz\n",
                (unsigned int)(clk_speed / 1000000),
                (unsigned int)(clk_speed / 10000) % 100);

    num += sprintf(buf+num, "Timers: \t%s %s \n",
                ((p_cpu->timers & 0x200) ? "TIMER1":""),
                ((p_cpu->timers & 0x100) ? "TIMER0":""));

    num += sprintf(buf+num,"Interrupt Vect Base: \t0x%x \n", p_cpu->vec_base);
    num += sprintf(buf+num,"Peripheral Base: \t0x%x \n", p_cpu->perip_base);

    num += sprintf(buf+num, "Data UNCACHED Base (I/O): start %#x Sz, %d MB \n",
            p_cpu->uncached_space.start, (0x10 << p_cpu->uncached_space.sz) );

    return buf;
}

char * arc_extn_mumbojumbo(int cpu_id, char *buf)
{
    int num=0;
    struct cpuinfo_arc *p_cpu = & cpuinfo_arc700[cpu_id];

const cpuinfo_data_t mul_type_nm [] = {
    { 0x0, "Not Present" },
    { 0x1, "32x32 with SPECIAL Result Reg" },
    { 0x2, "32x32 with ANY Result Reg" }
};

const cpuinfo_data_t mac_mul_nm[] = {
    { 0x0, "Not Present" },
    { 0x1, "Not Present" },
    { 0x2, "Dual 16 x 16" },
    { 0x3, "Not Present" },
    { 0x4, "32 x 16" },
    { 0x5, "Not Present" },
    { 0x6, "Dual 16 x 16 and 32 x 16" }
};

#define IS_AVAIL1(var)   ((var)? "Present" :"NA")

    num += sprintf(buf+num, "Extensions:\n");

    num += sprintf(buf+num, "Multiplier: %s",
                        mul_type_nm[p_cpu->extn.mul].str);

    num += sprintf(buf+num, "   MAC Multiplier: %s\n",
                        mac_mul_nm[p_cpu->extn_mac_mul.type].str);

#ifdef CONFIG_ARCH_ARC_DCCM
    num += sprintf(buf+num, "DCCM: %s,", IS_AVAIL1(p_cpu->extn.dccm));
    if (p_cpu->extn.dccm)
        num += sprintf(buf+num, " (%x)\n", p_cpu->extn.dccm);
#endif

#ifdef CONFIG_ARCH_ARC_ICCM
    num += sprintf(buf+num, "   ICCM: %s\n", IS_AVAIL1(p_cpu->extn.iccm));
    if (p_cpu->extn.iccm)
        num += sprintf(buf+num, " (%x)\n", p_cpu->extn.iccm);
#endif

    num += sprintf(buf+num, "CRC  Instrn: %s,", IS_AVAIL1(p_cpu->extn.crc));
    num += sprintf(buf+num, "   SWAP Instrn: %s", IS_AVAIL1(p_cpu->extn.swap));

#define IS_AVAIL2(var)   ((var == 0x2)? "Present" :"NA")

    num += sprintf(buf+num, "   NORM Instrn: %s\n", IS_AVAIL2(p_cpu->extn.norm));
    num += sprintf(buf+num, "Min-Max Instrn: %s,", IS_AVAIL2(p_cpu->extn.minmax));
    num += sprintf(buf+num, "   Barrel Shift Rotate Instrn: %s\n",
                        IS_AVAIL2(p_cpu->extn.barrel));

    num += sprintf(buf+num, "Ext Arith Instrn: %s\n",
                        IS_AVAIL2(p_cpu->extn.ext_arith));

#ifdef CONFIG_ARCH_ARC800
    num += sprintf(buf+num, "MP Extensions: Ver (%d), Arch (%d)\n",
                    p_cpu->mp.ver, p_cpu->mp.mp_arch);

    num += sprintf(buf+num, "    SCU %s, IDU %s, SDU %s\n", IS_AVAIL1(p_cpu->mp.scu),
                    IS_AVAIL1(p_cpu->mp.idu), IS_AVAIL1(p_cpu->mp.sdu));
#endif

    return buf;
}


/*
 * Initialize and setup the processor core
 * This is called by all the CPUs thus should not do special case stuff
 *    such as only for boot CPU etc
 */

void __init setup_processor(void)
{
    char str[512];
    int cpu_id = read_arc_build_cfg_regs();

    arc_irq_init();

    printk(arc_cpu_mumbojumbo(cpu_id, str));

    /* Enable MMU */
    tlb_init();

    a7_cache_init();

    printk(arc_extn_mumbojumbo(cpu_id, str));
}

static int __init parse_tag_core(struct tag *tag)
{
    printk_init("ATAG_CORE: successful parsing\n");
    return 0;
}

__tagtable(ATAG_CORE, parse_tag_core);

static int __init parse_tag_mem32(struct tag *tag)
{
    printk_init("ATAG_MEM: size = 0x%x\n", tag->u.mem.size);

    end_mem = CONFIG_SRAM_BASE + CONFIG_SRAM_SIZE + PHYS_SRAM_OFFSET;

    return 0;
}

__tagtable(ATAG_MEM, parse_tag_mem32);

static int __init parse_tag_clk_speed(struct tag *tag)
{
    printk_init("ATAG_CLK_SPEED: clock-speed = %d\n",
           tag->u.clk_speed.clk_speed_hz);
    clk_speed = tag->u.clk_speed.clk_speed_hz;
    return 0;
}

__tagtable(ATAG_CLK_SPEED, parse_tag_clk_speed);

/* not yet useful... */
static int __init parse_tag_ramdisk(struct tag *tag)
{
    printk_init("ATAG_RAMDISK: Flags for ramdisk = 0x%x\n",
           tag->u.ramdisk.flags);
    printk_init("ATAG_RAMDISK: ramdisk size = 0x%x\n", tag->u.ramdisk.size);
    printk_init("ATAG_RAMDISK: ramdisk start address = 0x%x\n",
           tag->u.ramdisk.start);
    return 0;

}

__tagtable(ATAG_RAMDISK, parse_tag_ramdisk);

/* not yet useful.... */
static int __init parse_tag_initrd2(struct tag *tag)
{
    printk_init("ATAG_INITRD2: initrd start = 0x%x\n", tag->u.initrd.start);
    printk_init("ATAG_INITRD2: initrd size = 0x%x\n", tag->u.initrd.size);
    return 0;

}

__tagtable(ATAG_INITRD2, parse_tag_initrd2);

static int __init parse_tag_cache(struct tag *tag)
{
    printk_init("ATAG_CACHE: ICACHE status = 0x%x\n", tag->u.cache.icache);
    printk_init("ATAG_CACHE: DCACHE status = 0x%x\n", tag->u.cache.dcache);
    return 0;

}

__tagtable(ATAG_CACHE, parse_tag_cache);

static int __init parse_tag_serial(struct tag *tag)
{
    printk_init("ATAG_SERIAL: serial_nr = %d\n", tag->u.serial.serial_nr);
    /* when we have multiple uart's serial_nr should also be processed */
    printk_init("ATAG_SERIAL: serial baudrate = %d\n", tag->u.serial.baudrate);
    serial_baudrate = tag->u.serial.baudrate;
    return 0;

}

__tagtable(ATAG_SERIAL, parse_tag_serial);

static int __init parse_tag_vmac(struct tag *tag)
{
    int i;
    printk_init("ATAG_VMAC: vmac address = %d:%d:%d:%d:%d:%d\n",
           tag->u.vmac.addr[0], tag->u.vmac.addr[1], tag->u.vmac.addr[2],
           tag->u.vmac.addr[3], tag->u.vmac.addr[4], tag->u.vmac.addr[5]);

    mac_addr.sa_family = 0;

    for (i = 0; i < 6; i++)
        mac_addr.sa_data[i] = tag->u.vmac.addr[i];

    return 0;

}

__tagtable(ATAG_VMAC, parse_tag_vmac);

static int __init parse_tag_cmdline(struct tag *tag)
{
    printk_init("ATAG_CMDLINE: command line = %s\n", tag->u.cmdline.cmdline);
    strcpy(command_line, tag->u.cmdline.cmdline);
    return 0;

}

__tagtable(ATAG_CMDLINE, parse_tag_cmdline);

static int __init parse_tag(struct tag *tag)
{
    extern struct tagtable __tagtable_begin, __tagtable_end;
    struct tagtable *t;

    for (t = &__tagtable_begin; t < &__tagtable_end; t++)
        if (tag->hdr.tag == t->tag) {
            t->parse(tag);
            break;
        }

    return t < &__tagtable_end;
}

static void __init parse_tags(struct tag *tags)
{
    while (tags->hdr.tag != ATAG_NONE) {

        if (!parse_tag(tags))
            printk_init("Ignoring unknown tag type\n");

        tags = tag_next(tags);
    }

}

/* Sample of how atags must be prepared by the bootloader */
static struct init_tags {
    struct tag_header hdr1;
    struct tag_core core;
    struct tag_header hdr5;
    struct tag_clk_speed clk_speed;
    struct tag_header hdr6;
    struct tag_cache cache;
#ifdef CONFIG_ARC700_SERIAL
    struct tag_header hdr7;
    struct tag_serial serial;
#endif
#ifdef CONFIG_ARCTANGENT_EMAC
    struct tag_header hdr8;
    struct tag_vmac vmac;
#endif
    struct tag_header hdr9;

} init_tags __initdata = {

    {tag_size(tag_core), ATAG_CORE},
    {},

    {tag_size(tag_clk_speed), ATAG_CLK_SPEED},
    {CONFIG_ARC700_CLK},

    {tag_size(tag_cache), ATAG_CACHE},

#ifdef CONFIG_ARC700_USE_ICACHE
    {1,
#else
    {0,

#endif
#ifdef CONFIG_ARC700_USE_DCACHE
     1},
#else
     0},

#endif

#ifdef CONFIG_ARC700_SERIAL
    {tag_size(tag_serial), ATAG_SERIAL},
    {0, CONFIG_ARC700_SERIAL_BAUD},
#endif

#ifdef CONFIG_ARCTANGENT_EMAC
    {tag_size(tag_vmac), ATAG_VMAC},
    {{0, 1, 2, 3, 4, 5}}, /* Never set the mac address starting with 1 */
#endif

    {0, ATAG_NONE}
};

void __init setup_arch(char **cmdline_p)
{
    int bootmap_size;
    unsigned long first_free_pfn, kernel_end_addr;
    extern unsigned long atag_head;
    struct tag *tags = (struct tag *)&init_tags;   /* to shut up gcc */

    /* If parameters passed by u-boot, override compile-time parameters */
    if (atag_head) {
        tags = (struct tag *)atag_head;

        if (tags->hdr.tag == ATAG_CORE) {
            printk_init("Parsing ATAG parameters from bootloader\n");
            parse_tags(tags);
        } else
            printk_init("INVALID ATAG parameters from bootloader\n");
    }
    else {
        printk_init("SKIPPING ATAG parsing...\n");
    }

    /* clk_speed and serial_baudrate intialised during parsing
       parameters */
    arc_console_baud = (clk_speed / (serial_baudrate * 4)) - 1;

    setup_processor();

#ifdef CONFIG_SMP
    smp_init_cpus();
#endif

#ifdef CONFIG_ARC700_SERIAL
    /* FIXME: ARC : temporarily done to reset the vuart */
    reset_vuart();
#endif

    init_mm.start_code = (unsigned long)&_text;
    init_mm.end_code = (unsigned long)&_etext;
    init_mm.end_data = (unsigned long)&_edata;
    init_mm.brk = (unsigned long)&_end;

    *cmdline_p = command_line;
    /*
     *  save a copy of he unparsed command line for the
     *  /proc/cmdline interface
     */
    memcpy(boot_command_line, command_line, sizeof(command_line));
    boot_command_line[COMMAND_LINE_SIZE - 1] = '\0';

    _current_task[0] = &init_task;

	/******* Setup bootmem allocator *************/

#define TO_PFN(addr) ((addr) >> PAGE_SHIFT)

	/* Make sure that "end_kernel" is page aligned in linker script
	 * so that it points to first free page in system
	 * Also being a linker script var, we need to do &end_kernel which
	 * doesn't work with >> operator, hence helper "kernel_end_addr"
	 */
	kernel_end_addr = (unsigned long) &end_kernel;
    first_free_pfn = TO_PFN(kernel_end_addr);

    min_low_pfn = TO_PFN(PAGE_OFFSET);
    max_low_pfn = max_pfn = TO_PFN(end_mem);   // for us no HIGH Mem
    num_physpages = max_low_pfn - min_low_pfn;

	bootmap_size = init_bootmem_node(NODE_DATA(0),
					first_free_pfn, min_low_pfn, max_low_pfn);

	free_bootmem(kernel_end_addr,  end_mem - kernel_end_addr);
	reserve_bootmem(kernel_end_addr, bootmap_size, BOOTMEM_DEFAULT);

	/* If no initramfs provided to kernel, and no NFS root, we fall back to
	 * /dev/hda2 as ROOT device, assuming it has busybox and other
	 * userland stuff
	 */
#if !defined(CONFIG_BLK_DEV_INITRD) && !defined(CONFIG_ROOT_NFS)
    ROOT_DEV = Root_HDA2;
#endif

    /* Can be issue if someone passes cmd line arg "ro"
     * But that is unlikely so keeping it as it is
     */
    root_mountflags &= ~MS_RDONLY;

    console_verbose();

#ifdef CONFIG_VT
#if defined(CONFIG_ARC_PGU_CONSOLE)
    /* Arc PGU Console */
    #error "FIXME: enable PGU Console"
#elif defined(CONFIG_DUMMY_CONSOLE)
    conswitchp = &dummy_con;
#endif
#endif

    paging_init();

    arc_verify_sig_sz();

    arc_unwind_init();
    arc_unwind_setup();
}

/*
 *  Get CPU information for use by the procfs.
 */

static int show_cpuinfo(struct seq_file *m, void *v)
{
    char str[1024];
    int cpu_id = (0xFFFF & (int)v);

    seq_printf(m, arc_cpu_mumbojumbo(cpu_id, str));

    seq_printf(m, "Bogo MIPS : \t%lu.%02lu\n",
           loops_per_jiffy / (500000 / HZ),
           (loops_per_jiffy / (5000 / HZ)) % 100);

    seq_printf(m, arc_mmu_mumbojumbo(cpu_id, str));

    seq_printf(m, arc_cache_mumbojumbo(cpu_id, str));

    seq_printf(m, arc_extn_mumbojumbo(cpu_id, str));

    seq_printf(m, "\n\n");

    return 0;
}

static void *c_start(struct seq_file *m, loff_t * pos)
{
    /* this 0xFF xxxx business is a simple hack.
       We encode cpu-id as 0x 00FF <cpu-id> and return it as a ptr
       We Can't return cpu-id directly because 1st cpu-id is 0, which has
       special meaning in seq-file framework (iterator end).
       Otherwise we have to kmalloc in c_start() and do a free in c_stop()
       which is really not required for a such a simple case
       show_cpuinfo() extracts the cpu-id from it.
     */
    return *pos < NR_CPUS ? ((void *)(0xFF0000 | (int)(*pos))) : NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t * pos)
{
    ++*pos;
    return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

struct seq_operations cpuinfo_op = {
      start:c_start,
      next:c_next,
      stop:c_stop,
      show:show_cpuinfo,
};

/* vineetg, Dec 15th 2007
   CPU Topology Support
 */

#include <linux/cpu.h>

struct cpu cpu_topology[NR_CPUS];

static int __init topology_init(void)
{
    int cpu;

    for_each_possible_cpu(cpu)
        register_cpu(&cpu_topology[cpu], cpu);

    return 0;
}

subsys_initcall(topology_init);
