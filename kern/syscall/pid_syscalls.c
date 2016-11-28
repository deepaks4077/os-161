#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <proc.h>
#include <current.h>

/*
 * get process id of the current process
 */

/* TODO: return error code after looking at with errno.h */
pid_t
sys_getpid(uint32_t t_addr){
    struct thread *curthread;
    curthread = t_addr;
    struct proc *curproc = curthread->t_proc;
    return curproc->p_pid;
}
