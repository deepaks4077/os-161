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
pid_t sys_getpid(struct proc *curprocess){
    return curprocess->p_pid;
}
