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
    return curprocess->pid;
}

void sys_exit(){
    // call thread_exit 
    // wait for number of processes waiting to be 0 before calling proc_destroy and pass the exitcode

    // get a reference to the current proc before destroying curthread
    struct proc *current_proc = curproc;
    thread_exit();
    proc_destroy(current_proc); 
}

// create a new process
// copy:
//      name
//      numthreads
//      state
//      address space
//      p_cwd
//      file handler array
//      fork thread
//      
//int sys_fork(struct trapframe *tf, struct proc* parent, int* retval){
    
//}



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

int sys_fork(struct trapframe *tf, struct proc *proc, struct thread *thread, int32_t *retval){

    int error = 0;

    /* Create the child process */
    struct proc *child = proc_create_runprogram(proc->p_name);
    if(child == NULL){
        return ENOMEM;
    }

    /* Now, copy all other properties to it */
    child->p_fhs = _fhs_clone(proc->p_fhs);
    if(child->p_fhs == NULL){
        proc_destroy(child);
        return ENOMEM;
    }

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
    error = thread_fork(thread, child, enter_new_process, child_tf, 0);
    if (error != 0){
        proc_destroy(child);
        kfree(child_tf);
        return ENOMEM;
    }

    *retval = child->pid;
    return 0;
}



