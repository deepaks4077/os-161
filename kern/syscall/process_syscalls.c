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

static void reset_ppid_on_exit(pid_t pid);

/*
 * get process id of the current process
 */
pid_t sys_getpid(struct proc *curprocess){
    return curprocess->pid;
}

// Zombie processes are destroyed inside proctable.c
void sys__exit(struct proc *proc, int exitcode){

    exitcode = _MKWAIT_EXIT(exitcode);

    KASSERT(!lock_do_i_hold(lk_proctable));

    lock_acquire(lk_proctable);

    proc->exitcode = exitcode;
    proc->p_state = S_ZOMBIE;

    reset_ppid_on_exit(proc->pid);

    cv_broadcast(proc->cv_waitpid, lk_proctable);
    lock_release(lk_proctable);

    thread_exit();
}

int sys_waitpid(pid_t pid, struct proc *proc, userptr_t status, int32_t *retval){

    int error = 0;
    int exitcode = 0;

    if(!is_valid_pid(pid)){
        return ESRCH;
    }

    if(status != NULL){
        void *dest = kmalloc(1);
        error = copyin(status, dest, 1);
        kfree(dest);
        if(error){
            return error;
        }
    }

    KASSERT(!lock_do_i_hold(lk_proctable));
    lock_acquire(lk_proctable);

    struct proc *child = proctable_get(pid);
    if(child == NULL){
        lock_release(lk_proctable);
        return ESRCH;
    }

    if(child->ppid != curproc->pid){
        lock_release(lk_proctable);
        return ECHILD;
    }

    if(child->p_state != S_ZOMBIE){
        child->iswaiting = true;
        while(child->p_state != S_ZOMBIE){
            cv_wait(child->cv_waitpid, lk_proctable);
        }
    }else{
        lock_release(lk_proctable);
        return ESRCH;
    }

    exitcode = child->exitcode;
    child->iswaiting = false;
    lock_release(lk_proctable);

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

	//DEBUG(DB_THREADS, "Process: %d, name:%s\n", proc->pid, proc->p_name);

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

	P(child->sem_fork);

    return 0;
}

static void reset_ppid_on_exit(pid_t pid){
    // Change child's ppid to ppid of parent

    struct proc *child = proctable_getchild(pid);
    if(child == NULL){
        return;
    }

    spinlock_acquire(child->p_lock);
    child->ppid = curproc->pid;
    spinlock_release(child->p_lock);
}



