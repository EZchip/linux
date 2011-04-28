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
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/msg.h>
#include <linux/file.h>
#include <linux/shm.h>
/* Sameer: Added as some definitions are moved from
           linux/sem.h to linux/syscalls.h */
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
/*
 * sys_pipe() is the normal C calling standard for creating
 * a pipe. It's not the way unix traditionally does this, though.
 */
asmlinkage int sys_pipe(unsigned long *fildes)
{
	int fd[2];
	int error;

	error = do_pipe(fd);
	if (!error) {
		if (copy_to_user(fildes, fd, 2 * sizeof(int)))
			error = -EFAULT;
	}
	return error;
}

/* common code for old and new mmaps */
static inline long do_mmap2(unsigned long addr, unsigned long len,
			    unsigned long prot, unsigned long flags,
			    unsigned long fd, unsigned long pgoff)
{
	int error = -EBADF;
	struct file *file = NULL;

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	if (!(flags & MAP_ANONYMOUS)) {
		file = fget(fd);
		if (!file)
			goto out;
	}

	down_write(&current->mm->mmap_sem);
	error = do_mmap_pgoff(file, addr, len, prot, flags, pgoff);
	up_write(&current->mm->mmap_sem);

	if (file)
		fput(file);
      out:
	return error;
}

asmlinkage long sys_mmap2(unsigned long addr, unsigned long len,
			  unsigned long prot, unsigned long flags,
			  unsigned long fd, unsigned long pgoff)
{
	return do_mmap2(addr, len, prot, flags, fd, pgoff);
}

/*
 * Perform the select(nd, in, out, ex, tv) and mmap() system
 * calls. Linux/i386 didn't use to be able to handle more than
 * 4 system call parameters, so these system calls used a memory
 * block for parameter passing..
 */

struct mmap_arg_struct {
	unsigned long addr;
	unsigned long len;
	unsigned long prot;
	unsigned long flags;
	unsigned long fd;
	unsigned long offset;
};

asmlinkage int old_mmap(struct mmap_arg_struct *arg)
{
	struct mmap_arg_struct a;
	int err = -EFAULT;

	if (copy_from_user(&a, arg, sizeof(a)))
		goto out;

	err = -EINVAL;
	if (a.offset & ~PAGE_MASK)
		goto out;

	err =
	    do_mmap2(a.addr, a.len, a.prot, a.flags, a.fd,
		     a.offset >> PAGE_SHIFT);
      out:
	return err;
}

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
		       int third, void *ptr, long fifth)
{
	int version, ret;

	version = call >> 16;	/* hack for backward compatibility */
	call &= 0xffff;

	switch (call) {
	case SEMOP:
		return sys_semop(first, (struct sembuf *)ptr, second);
	case SEMGET:
		return sys_semget(first, second, third);
	case SEMCTL:{
			union semun fourth;
			if (!ptr)
				return -EINVAL;
			if (get_user(fourth.__pad, (void **)ptr))
				return -EFAULT;
			return sys_semctl(first, second, third, fourth);
		}

	case MSGSND:
		return sys_msgsnd(first, (struct msgbuf *)ptr, second, third);
	case MSGRCV:
		switch (version) {
		case 0:{
				struct ipc_kludge tmp;
				if (!ptr)
					return -EINVAL;

				if (copy_from_user(&tmp,
						   (struct ipc_kludge *)ptr,
						   sizeof(tmp)))
					return -EFAULT;
				return sys_msgrcv(first, tmp.msgp, second,
						  tmp.msgtyp, third);
			}
		default:
			return sys_msgrcv(first,
					  (struct msgbuf *)ptr,
					  second, fifth, third);
		}
	case MSGGET:
		return sys_msgget((key_t) first, second);
	case MSGCTL:
		return sys_msgctl(first, second, (struct msqid_ds *)ptr);

	case SHMAT:
		switch (version) {
		default:{
				ulong raddr;
				/* Sameer: Things change over time and so do
				   function signatures and their usage :-P */
				ret =
				    do_shmat(first, (char __user *)ptr, second,
					     &raddr);
				/* ret = sys_shmat (first, (char *) ptr,
				   second, &raddr); */
				if (ret)
					return ret;
				return put_user(raddr, (ulong *) third);
			}

		case 1:	/* Of course, we don't support iBCS2! */
			return -EINVAL;
			/* /\* iBCS2 emulator entry point *\/ */
/* 			if (!segment_eq(get_fs(), get_ds())) */
/* 				return -EINVAL; */
/* 			return sys_shmat (first, (char *) ptr, second,
			(ulong *) third); */
		}
	case SHMDT:
		return sys_shmdt((char *)ptr);
	case SHMGET:
		return sys_shmget(first, second, third);
	case SHMCTL:
		return sys_shmctl(first, second, (struct shmid_ds *)ptr);
	default:
		return -EINVAL;
	}
}

int kernel_execve(const char *filename, char *const argv[], char *const envp[])
{
	struct pt_regs regs;
	int ret;

	__asm__ __volatile__("mov	r3, %5\n\t"
			     "mov	r2, %4\n\t"
			     "mov	r1, %3\n\t"
			     "mov	r0, %2\n\t"
			     "mov	r8, %1\n\t"
			     "trap0 \n\t"
			     "nop    \n\t"
			     "nop    \n\t"
			     "mov	%0, r0":"=r"(ret)
			     :"i"(__NR_execve),
			     "r"((long)(char __user *)filename),
			     "r"((long)(char __user * __user *)argv),
			     "r"((long)(char __user * __user *)envp),
			     "r"((long)(&regs))
			     :"cc", "r0", "r1", "r2", "r3", "r8");

	return ret;

}

EXPORT_SYMBOL(kernel_execve);
