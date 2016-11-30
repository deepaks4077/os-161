#include <types.h>
#include <kern/errno.h>
#include <syscall.h>
#include <lib.h>
#include <thread.h>
#include <proc.h>
#include <current.h>

/*
 * get process id of the current process
 */

/* TODO: return error code after looking at with errno.h */
pid_t sys_getpid(uint32_t t_addr){
    struct thread *curthread;
    curthread = (struct thread *)t_addr;
    struct proc *cur_proc = curthread->t_proc;
    return cur_proc->p_pid;
}
