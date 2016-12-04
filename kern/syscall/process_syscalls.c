#include <types.h>
#include <kern/errno.h>
#include <syscall.h>
#include <lib.h>
#include <thread.h>
#include <proc.h>
#include <current.h>
#include <copyinout.h>
#include <spinlock.h>

/*
 * get process id of the current process
 */

pid_t sys_getpid(struct proc *curprocess){
    return curprocess->p_pid;
}

void sys_exit(int exitcode){
    // call thread_exit 
    // wait for number of processes waiting to be 0 before calling proc_destroy and pass the exitcode

    // get a reference to the current proc before destroying curthread
    struct proc *current_proc = curproc;
    thread_destroy(curthread);
    proc_destroy(current_proc);
}

/*
pid_t sys_waitpid(pid_t chpid, userptr_t status, int option, int *retval){
    // invalid status pointer == address value if outside the address space of the program?
    // invalid option ==
    // childproc is not a child of the current process
    // childproc does not exist
    // childproc has already exited.
    // status pointer is NULL (still valid!!)

    // use copyin to copy from user address space status to a dummy kernel address space
    // if this fails then status ptr is an invalid userspace address

    int ret;
    int* dummy = kmalloc(sizeof(int));
    ret = copyin(status,dummy,sizeof(int));
    if(ret == EFAULT){
        //invalid status pointer
        *retval = EINVAL;
        ret = 1;
    }

    struct proc *chproc;
    chproc = get_process(chpid); // child does not exist
    if(chproc == NULL){
        *retval = ESRCH;
        ret = 1;
    }

    if(chproc->p_parent != curproc){ // curproc is not the parent
        *retval = ECHILD;
        ret = 1;
    }

    spinlock_acquire(&chproc->p_lock);
    if(chproc->p_numthreads == 0){
        // process has already exited so its pid cannot be collected anymore
        *retval = ESRCH;
        ret = 1;
    }
    spinlock_release(&chproc->p_lock);

    KASSERT(chproc->p_parent == curproc);
    KASSERT(chproc->p_numthreads > 0);

}
*/



