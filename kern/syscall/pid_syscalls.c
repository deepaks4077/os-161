#include <syscall.h>
#include <thread.h>

/*
 * get process id of the current process
 */
pid_t
sys_getpid(uint32_t t_addr){
    struct thread *curthread;
    curthread = t_addr;
    struct proc *curproc = curthread->t_proc;
    return curproc->p_pid;
}
