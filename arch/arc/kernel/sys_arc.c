/******************************************************************************
 * Copyright ARC International (www.arc.com) 2008-2010
 *
 *  Vineetg: July 2009
 *    -kernel_execve inline asm optimised to prevent un-needed reg shuffling
 *     Now have 6 instructions in  generated code as opposed to 16.
 *
 *****************************************************************************/
/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

#include <linux/errno.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>


struct sel_arg_struct {
    unsigned long n;
    fd_set *inp, *outp, *exp;
    struct timeval *tvp;
};

asmlinkage int old_select(struct sel_arg_struct *arg)
{
    struct sel_arg_struct a;

    if (copy_from_user(&a, arg, sizeof(a)))
        return -EFAULT;
    /* sys_select() does the appropriate kernel locking */
    return sys_select(a.n, a.inp, a.outp, a.exp, a.tvp);
}

/*
 * sys_ipc() is the de-multiplexer for the SysV IPC calls..
 *
 * This is really horribly ugly.
 */
asmlinkage int sys_ipc(uint call, int first, int second,
               int third, void __user *ptr, long fifth)
{
    int version, ret;

    version = call >> 16;   /* hack for backward compatibility */
    call &= 0xffff;

    switch (call) {
    case SEMOP:
        return sys_semtimedop(first, (struct sembuf __user *)ptr, second, NULL);

    case SEMTIMEDOP:
        return sys_semtimedop(first, (struct sembuf __user *)ptr, second,
                    (const struct timespec __user *)fifth);

    case SEMGET:
        return sys_semget(first, second, third);
    case SEMCTL:{
            union semun fourth;
            if (!ptr)
                return -EINVAL;
            if (get_user(fourth.__pad, (void __user * __user *)ptr))
                return -EFAULT;
            return sys_semctl(first, second, third, fourth);
        }

    case MSGSND:
        return sys_msgsnd(first, (struct msgbuf __user*)ptr, second, third);

    case MSGRCV:
        switch (version) {
        case 0:{
                struct ipc_kludge tmp;
                if (!ptr)
                    return -EINVAL;

                if (copy_from_user(&tmp,
                           (struct ipc_kludge __user*)ptr,
                           sizeof(tmp)))
                    return -EFAULT;
                return sys_msgrcv(first, tmp.msgp, second,
                          tmp.msgtyp, third);
            }
        default:
            return sys_msgrcv(first,
                      (struct msgbuf __user *)ptr,
                      second, fifth, third);
        }
    case MSGGET:
        return sys_msgget((key_t) first, second);
    case MSGCTL:
        return sys_msgctl(first, second, (struct msqid_ds __user*)ptr);

    case SHMAT:
        switch (version) {
        default:{
                ulong raddr;
                ret =
                    do_shmat(first, (char __user *)ptr, second,
                         &raddr);
                if (ret)
                    return ret;
                return put_user(raddr, (ulong *) third);
            }

        case 1: /* iBCS2 emulator entry point */
            if (!segment_eq(get_fs(), get_ds()))
                return -EINVAL;
            /* The "(ulong *) third" is valid _only_ because of the kernel segment thing */
            return do_shmat(first, (char __user *) ptr, second, (ulong *) third);
        }
    case SHMDT:
        return sys_shmdt((char __user *)ptr);
    case SHMGET:
        return sys_shmget(first, second, third);
    case SHMCTL:
        return sys_shmctl(first, second, (struct shmid_ds __user *)ptr);
    default:
        return -ENOSYS;
    }
}

int kernel_execve(const char *filename, char *const argv[], char *const envp[])
{
    /* Although the arguments (order, number) to this function are
     * same as sys call, we don't need to setup args in regs again.
     * However in case mainline kernel changes the order of args to
     * kernel_execve, that assumtion will break.
     * So to be safe, let gcc know the args for sys call.
     * If they match no extra code will be generated
     */
    register int arg2 asm ("r1") = (int)argv;
    register int arg3 asm ("r2") = (int)envp;

    register int filenm_n_ret asm ("r0") = (int)filename;

    __asm__ __volatile__(
                 "mov   r8, %1\n\t"
                 "trap0 \n\t"
                 :"+r"(filenm_n_ret)
                 :"i"(__NR_execve),  "r"(arg2), "r"(arg3)
                 :"r8","memory");

    return filenm_n_ret;
}
EXPORT_SYMBOL(kernel_execve);
