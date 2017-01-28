#include <types.h>
#include <kern/errno.h>
#include <syscall.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <proc.h>
#include <current.h>
#include <copyinout.h>
#include <spinlock.h>
#include <mips/trapframe.h>
#include <addrspace.h>
#include <proctable.h>
#include <kern/wait.h>

/*
 * get process id of the current process
 */
pid_t sys_getpid(struct proc *curprocess){
    return curprocess->pid;
}

// Zombie processes are destroyed inside proctable.c
void sys__exit(struct proc *proc, int exitcode){

    exitcode = _MKWAIT_EXIT(exitcode);

    proc->exitcode = exitcode;
    proc->p_state = S_ZOMBIE;

    V(proc->sem_waitpid);

    thread_exit();
}

int sys_waitpid(pid_t pid, struct proc *proc, userptr_t status, int32_t *retval){

    int error = 0;
    int exitcode = 0;

    if(!is_valid_pid(pid)){
        return ESRCH;
    }

    struct proc *child = proctable_get(pid);
    if(child == NULL || child->p_state == S_ZOMBIE){
        return ESRCH;
    }

    if(child->ppid != curproc->pid){
        return ECHILD;
    }

    if(status != NULL){
        void *dest = kmalloc(1);
        error = copyin(status, dest, 1);
        kfree(dest);
        if(error){
            return error;
        }
    }

    if(child->p_state != S_ZOMBIE){
        child->iswaiting = true;
        P(proc->sem_waitpid);
        child->iswaiting = false;
    }

    exitcode = child->exitcode;

    if(status != NULL){
        error = copyout(&exitcode, status, sizeof(int));
        if(error){
            return error;
        }
    }

    *retval = pid;

    return error;
}

int sys_fork(struct trapframe *tf, struct proc *proc, struct thread *thread, int32_t *retval){

	DEBUG(DB_THREADS, "Process: %d, name:%s\n", proc->pid, proc->p_name);

    int error = 0;

    /* Create the child process */
    struct proc *child = proc_create_runprogram(proc->p_name);
    if(child == NULL){
        return ENOMEM;
    }

    /* Now, copy all other properties to it */
    struct fharray *fhs = _fhs_clone(&proc->p_fhs);
    if(fhs == NULL){
        proc_destroy(child);
        return ENOMEM;
    }

	child->p_fhs = *fhs;

    /* Copy the trapframe */
    struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));
    if(child_tf == NULL){
        proc_destroy(child);
        return ENOMEM;
    }

    child_tf = memcpy(child_tf, tf, sizeof(struct trapframe));
    if (child_tf == NULL){
        kfree(child_tf);
        proc_destroy(child);
        return ENOMEM;
    }

    child->ppid = proc->pid;

    /* Copy the address space */
    error = as_copy(proc->p_addrspace, &child->p_addrspace);
    if(error != 0){
        kfree(child_tf);
        proc_destroy(child);
        return ENOMEM;
    }

    /* Fork the process thread and assign to child */
    error = thread_fork(thread->t_name, child, &enter_forked_process, child_tf, 0);
    if (error != 0){
        proc_destroy(child);
        kfree(child_tf);
        return ENOMEM;
    }

    *retval = child->pid;
    return 0;
}



