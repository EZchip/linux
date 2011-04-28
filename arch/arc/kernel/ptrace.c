/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

/* arch/arcnommu/kernel/ptrace.c
 *
 * Copyright (C)
 *
 * Author(s) : Soam Vasani, Kanika Nema
 */

#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/user.h>
#include <linux/mm.h>
#include <linux/elf.h>
#include <linux/regset.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <asm/arcdefs.h>

/* OFFSET(x,y) is the offset of y in x */
#define OFFSET(x,y) ( &((x *)0)->y )

//#define PTRACE_DEBUG
int ptrace_dbg = 0;
#ifdef PTRACE_DEBUG
#define DBG(fmt, args...) if (ptrace_dbg) printk(KERN_INFO fmt , ## args)
#else
#define DBG(fmt, args...)
#endif


void ptrace_disable(struct task_struct *child)
{
  /* DUMMY: dummy function to resolve undefined references */
}


unsigned long
getreg(unsigned int offset,struct task_struct *child)
{
    struct callee_regs *calleeregs = (struct callee_regs *)child->thread.callee_reg;
    struct pt_regs *ptregs = (struct pt_regs *)(calleeregs + 1);
    BUG_ON(ptregs != task_pt_regs(child));

    DBG("    %s( 0x%x, 0x%x, 0x%x, 0x%x) \n", __FUNCTION__, ptregs, calleeregs, offset, child);

    if(offset < sizeof(*ptregs))
        return *(unsigned long *)(((char *)ptregs) + offset);

    if((offset-sizeof(*ptregs)) < sizeof(*calleeregs) )
        return *(unsigned long *)(((char *)calleeregs) + offset - sizeof(*ptregs));

    if(offset ==(int) OFFSET(struct user_regs_struct,stop_pc))
    {
        /* orig_r8 contains either syscall num or NR_syscalls+1 incase of an exception */
        if(ptregs->orig_r8 > NR_syscalls + 1)
        {
            DBG("Reading stop_pc(efa) %08lx\n",child->thread.fault_address);
            return child->thread.fault_address;
        }
        /* orig_r8 -1 for int1 and -2 for int2 and NR_SYSCALLS + 1
           is for a non-trap_s exception*/
        else
        {
            DBG("Reading stop_pc(ret) %08lx\n",ptregs->ret);
            return ptregs->ret;
        }
    }
    if(offset ==(int) OFFSET(struct user_regs_struct,efa))
    {
        DBG("Reading efa %08lx\n",child->thread.fault_address);
        return child->thread.fault_address;
    }

    return 0;                /* can't happen */
}


void
setreg(int offset, int data, struct task_struct *child)
{
    struct callee_regs *calleeregs = (struct callee_regs *)child->thread.callee_reg;
    struct pt_regs *ptregs = (struct pt_regs *)(calleeregs + 1);
    BUG_ON(ptregs != task_pt_regs(child));

    if(offset ==(int) OFFSET(struct user_regs_struct, reserved1) ||
        offset == (int) OFFSET(struct user_regs_struct, reserved2) ||
        offset == (int) OFFSET(struct user_regs_struct,efa))
        return;

    if(offset ==(int) OFFSET(struct user_regs_struct,stop_pc))
    {
        DBG("   [setreg]: writing to stop_pc(ret) %08lx\n",data);
        ptregs->ret = data;
        return ;
    }

    if(offset < sizeof(*ptregs))
    {
        *(unsigned long *)(((char *)ptregs) + offset) = data;
        return;
    }

    offset -= sizeof(*ptregs);

    if(offset < sizeof(*calleeregs))
        *(unsigned long *)(((char *)calleeregs) + offset) = data;

    return;

    /* cant happen */
}

long arch_ptrace(struct task_struct *child, long request, long addr, long data)
{
    int ret;
    DBG("---- ptrace(0x%x, %ld,%lx,%lx) ----\n", child, request, addr, data);

    switch (request) {
    case PTRACE_PEEKTEXT: /* read word at location addr. */
    case PTRACE_PEEKDATA: {
        unsigned long tmp;
        int copied;

        copied = access_process_vm(child, addr, &tmp, sizeof(tmp), 0);
        ret = -EIO;
        if (copied != sizeof(tmp))
            break;
        ret = put_user(tmp,(unsigned long *) data);
        DBG("    Peek @ 0x%x = 0x%x \n",addr, tmp);
        break;
    }

    case PTRACE_POKETEXT: /* write the word at location addr. */
    case PTRACE_POKEDATA:
        DBG("    @ POKE @ 0x%x = 0x%x \n",addr, data);
        ret = 0;
        if (access_process_vm(child, addr, &data, sizeof(data), 1) == sizeof(data))
            break;
        ret = -EIO;
        break;

    case PTRACE_SYSCALL:
    case PTRACE_CONT: { /* restart after signal. */
        ret = -EIO;
        if ((unsigned long) data > _NSIG)
            break;
        if (request == PTRACE_SYSCALL)
            child->ptrace |= PT_TRACESYS;
        else
            child->ptrace &= ~PT_TRACESYS;
        child->exit_code = data;
        wake_up_process(child);
        ret = 0;
        break;
    }

    case PTRACE_GETREGS: {
        int i;
        unsigned long tmp;

        for(i=0; i< sizeof(struct user_regs_struct);i++)
        {
            /* getreg wants a byte offset */
            tmp = getreg(i << 2,child);
            ret = put_user(tmp,((unsigned long *)data) + i);
            if(ret < 0)
                goto out;
        }
        break;
    }

    case PTRACE_SETREGS: {
        int i;

        for(i=0; i< sizeof(struct user_regs_struct)/4; i++)
        {
            unsigned long tmp;
            ret = get_user(tmp, (((unsigned long *)data) + i));
            if(ret < 0)
                goto out;
            setreg(i << 2, tmp, child);
        }

        ret = 0;
        break;
    }

        /*
         * make the child exit.  Best I can do is send it a sigkill.
         * perhaps it should be put in the status that it wants to
         * exit.
         */
    case PTRACE_KILL: {
        ret = 0;
        if (child->exit_state == EXIT_ZOMBIE)   /* already dead */
            break;
        child->exit_code = SIGKILL;
        wake_up_process(child);
        break;
    }

    case PTRACE_DETACH:
        /* detach a process that was attached. */
        ret = ptrace_detach(child, data);
        break;


    case PTRACE_SETOPTIONS:
        if (data & PTRACE_O_TRACESYSGOOD)
            child->ptrace |= PT_TRACESYSGOOD;
        else
            child->ptrace &= ~PT_TRACESYSGOOD;
        ret = 0;
        break;

    case PTRACE_PEEKUSR: {
        int tmp;

        /* user regs */
        if(addr > (unsigned)OFFSET(struct user,regs) &&
           addr < (unsigned)OFFSET(struct user,regs) + sizeof(struct user_regs_struct))
        {
            tmp = getreg(addr,child);
            ret = put_user(tmp, ((unsigned long *)data));
        }
        /* signal */
        else if(addr == (unsigned)OFFSET(struct user, signal))
        {
            tmp = current->exit_code; /* set by syscall_trace */
            ret = put_user(tmp, ((unsigned long *)data));
        }

        /* nothing else is interesting yet*/
        else if(addr > 0 && addr < sizeof(struct user))
            return -EIO;
        /* out of bounds */
        else
            return -EIO;

        break;
    }

    case PTRACE_POKEUSR: {
        ret = 0;
        if(addr ==(int) OFFSET(struct user_regs_struct,efa))
            return -EIO;

        if(addr > (unsigned)OFFSET(struct user,regs) &&
           addr < (unsigned)OFFSET(struct user,regs) + sizeof(struct user_regs_struct))
        {
            setreg(addr, data, child);
        }
        else
            return -EIO;

        break;
    }

        /*
         * Single step not supported.  ARC's single step bit won't
         * work for us.  It is in the DEBUG register, which the RTI
         * instruction does not restore.
         */
    case PTRACE_SINGLESTEP:

    default:
        ret = ptrace_request(child, request, addr, data);
        break;
    }

out:
    return ret;

}

asmlinkage void syscall_trace(void)
{
    if (!(current->ptrace & PT_PTRACED))
        return;
    if (!test_thread_flag(TIF_SYSCALL_TRACE))
        return;

    /* The 0x80 provides a way for the tracing parent to distinguish
       between a syscall stop and SIGTRAP delivery */
    ptrace_notify(SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD) ?
                             0x80 : 0));

    /*
     * this isn't the same as continuing with a signal, but it will do
     * for normal use.  strace only continues with a signal if the
     * stopping signal is not SIGTRAP.  -brl
     */
    if (current->exit_code) {
        send_sig(current->exit_code, current, 1);
        current->exit_code = 0;
    }
}

static int arc_regs_get(struct task_struct *target,
			 const struct user_regset *regset,
			 unsigned int pos, unsigned int count,
			 void *kbuf, void __user *ubuf)
{
    DBG("arc_regs_get %x %d %d\n", target, pos, count);

    if (kbuf) {
        unsigned long *k = kbuf;
        while (count > 0) {
            *k++ = getreg(pos, target);
            count -= sizeof(*k);
            pos += sizeof(*k);
        }
    }
    else {
		unsigned long __user *u = ubuf;
		while (count > 0) {
            if (__put_user(getreg(pos, target), u++))
                return -EFAULT;
            count -= sizeof(*u);
            pos += sizeof(*u);
        }
    }

    return 0;
}

static int arc_regs_set(struct task_struct *target,
			 const struct user_regset *regset,
			 unsigned int pos, unsigned int count,
			 const void *kbuf, const void __user *ubuf)
{
    int ret = 0;

    DBG("arc_regs_set %x %d %d\n", target, pos, count);

    if (kbuf) {
        const unsigned long *k = kbuf;
        while (count > 0) {
            setreg(pos, *k++, target);
            count -= sizeof(*k);
            pos += sizeof(*k);
        }
    }
    else {
		const unsigned long __user *u = ubuf;
        unsigned long word;
		while (count > 0) {
            ret = __get_user(word,u++);
            if (ret)
                return ret;
            setreg(pos, word, target);
            count -= sizeof(*u);
            pos += sizeof(*u);
        }
    }

    return 0;
}

static const struct user_regset arc_regsets[] = {
    [0] = {
        .core_note_type = NT_PRSTATUS,
	    .n = ELF_NGREG,
	    .size = sizeof(long),
	    .align = sizeof(long),
	    .get = arc_regs_get,
	    .set = arc_regs_set
    }
};

static const struct user_regset_view user_arc_view = {
	.name = UTS_MACHINE,
	.e_machine = EM_ARCTANGENT,
	.regsets = arc_regsets,
	.n = ARRAY_SIZE(arc_regsets)
};

const struct user_regset_view *task_user_regset_view(struct task_struct *task)
{
	return &user_arc_view;
}
