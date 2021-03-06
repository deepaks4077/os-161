/*
  (c) 2013
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Process support.
 *
 * There is (intentionally) not much here; you will need to add stuff
 * and maybe change around what's already present.
 *
 * p_lock is intended to be held when manipulating the pointers in the
 * proc structure, not while doing any significant work with the
 * things they point to. Rearrange this (and/or change it to be a
 * regular lock) as needed.
 *
 * Unless you're implementing multithreaded user processes, the only
 * process that will have more than one thread is the kernel process.
 */

#include <types.h>
#include <spl.h>
#include <array.h>
#include <lib.h>
#include <smpfs.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <vfs.h>
#include <kern/unistd.h>

/*
 * The process for the kernel; this holds all the kernel-only threads.
 */
struct proc *kproc;

/*
 * The master array of all processes
 */
 DECLARRAY(proc,static __UNUSED inline);
 DEFARRAY(proc,static __UNUSED inline);
 static struct procarray allprocs;
 static volatile unsigned numprocs;
 static volatile unsigned next_pid;

/* spinlocks used to protect numprocs and allprocs */
 static struct spinlock sp_numprocs;
 static struct spinlock sp_allprocs; // protect allprocs and next_pid

/*
 * Create a proc structure.
 */
static
struct proc *
proc_create(const char *name)
{
	spinlock_acquire(&sp_numprocs);
	if(numprocs >= MAX_PID){
		spinlock_release(&sp_numprocs);
		return NULL;
	}else{
		numprocs++;
	}
	spinlock_release(&sp_numprocs);

	struct proc *proc;

	proc = kmalloc(sizeof(*proc));
	if (proc == NULL) {
		spinlock_acquire(&sp_numprocs);
		numprocs--;
		spinlock_release(&sp_numprocs);
		return NULL;
	}

	proc->p_name = kstrdup(name);
	if (proc->p_name == NULL) {
		kfree(proc);
		spinlock_acquire(&sp_numprocs);
		numprocs--;
		spinlock_release(&sp_numprocs);
		return NULL;
	}

	proc->p_numthreads = 0;
	spinlock_init(&proc->p_lock);

	/* VM fields */
	proc->p_addrspace = NULL;

	/* VFS fields */
	proc->p_cwd = NULL;

	/* parent process is NULL by default. Assign the parent inside fork */
	proc->p_parent = NULL;

	DEBUG(DB_VFS, "Bootstrapping for process : %s\n", proc->p_name);

	/* The kernel process has a pid of 0 and add it to allprocs array */
	if(strcmp(name,KERNELPROC) == 0){
		KASSERT(numprocs == 1);
		proc->p_pid = KERNEL_PID;
		procarray_add(&allprocs,proc,(unsigned int *)&proc->p_pid); // does not need to be protected since no other process exists yet?
		KASSERT(proc->p_pid == KERNEL_PID);
	}else{
	/* Otherwise, assign the pid from the available pids */
	/* bootstrap the console file handles */

	/* In main.c the kernel process is bootstrapped before the vfs is bootstrapped,
		this means that we can't create file handles using _fh_create. Soln => Kernel process
		does not need these syscalls? */
		int ret = _fh_bootstrap(&proc->p_fhs);
		if(ret != 0){
			kfree(proc);
			spinlock_acquire(&sp_numprocs);
			numprocs--;
			spinlock_release(&sp_numprocs);
			return NULL;
		}
		
		if(fharray_get(&proc->p_fhs,0) == NULL || 
		fharray_get(&proc->p_fhs,1) == NULL || 
		fharray_get(&proc->p_fhs,2) == NULL ){
			
			kfree(proc);
			spinlock_acquire(&sp_numprocs);
			numprocs--;
			spinlock_release(&sp_numprocs);
			return NULL;
		}

		spinlock_acquire(&sp_allprocs);

		pid_t newpid = proc_assignpid(proc);
		proc->p_pid = newpid;
		procarray_set(&allprocs,newpid,proc);
		KASSERT(procarray_get(&allprocs,newpid) == proc);
		
		spinlock_release(&sp_allprocs);
	}

	return proc;
}

/*
 * Destroy a proc structure.
 *
 * Note: nothing currently calls this. Your wait/exit code will
 * probably want to do so.
 */
void
proc_destroy(struct proc *proc)
{
	/*
	 * You probably want to destroy and null out much of the
	 * process (particularly the address space) at exit time if
	 * your wait/exit design calls for the process structure to
	 * hang around beyond process exit. Some wait/exit designs
	 * do, some don't.
	 */

	KASSERT(proc != NULL);
	KASSERT(proc != kproc);

	/*
	 * We don't take p_lock in here because we must have the only
	 * reference to this structure. (Otherwise it would be
	 * incorrect to destroy it.)
	 */

	/* VFS fields */
	if (proc->p_cwd) {
		VOP_DECREF(proc->p_cwd);
		proc->p_cwd = NULL;
	}

	/* VM fields */
	if (proc->p_addrspace) {
		/*
		 * If p is the current process, remove it safely from
		 * p_addrspace before destroying it. This makes sure
		 * we don't try to activate the address space while
		 * it's being destroyed.
		 *
		 * Also explicitly deactivate, because setting the
		 * address space to NULL won't necessarily do that.
		 *
		 * (When the address space is NULL, it means the
		 * process is kernel-only; in that case it is normally
		 * ok if the MMU and MMU- related data structures
		 * still refer to the address space of the last
		 * process that had one. Then you save work if that
		 * process is the next one to run, which isn't
		 * uncommon. However, here we're going to destroy the
		 * address space, so we need to make sure that nothing
		 * in the VM system still refers to it.)
		 *
		 * The call to as_deactivate() must come after we
		 * clear the address space, or a timer interrupt might
		 * reactivate the old address space again behind our
		 * back.
		 *
		 * If p is not the current process, still remove it
		 * from p_addrspace before destroying it as a
		 * precaution. Note that if p is not the current
		 * process, in order to be here p must either have
		 * never run (e.g. cleaning up after fork failed) or
		 * have finished running and exited. It is quite
		 * incorrect to destroy the proc structure of some
		 * random other process while it's still running...
		 */
		struct addrspace *as;

		if (proc == curproc) {
			as = proc_setas(NULL);
			as_deactivate();
		}
		else {
			as = proc->p_addrspace;
			proc->p_addrspace = NULL;
		}
		as_destroy(as);
	}

	KASSERT(proc->p_numthreads == 0);
	spinlock_cleanup(&proc->p_lock);

	//TODO: decrement the number of processes and remove from allprocs
	spinlock_acquire(&sp_allprocs);
	procarray_set(&allprocs,proc->p_pid,NULL);
	spinlock_acquire(&sp_allprocs);

	spinlock_acquire(&sp_numprocs);
	numprocs--;
	spinlock_release(&sp_numprocs);

	/* All elements inside p_fhs need to be deallocated */
	int idx = 0;
	for(idx=0;idx<(int)fharray_num(&proc->p_fhs);idx++){
		_fhs_close(idx,&proc->p_fhs);
	}

	/* Cleanup the associated file handles array */
	fharray_cleanup(&proc->p_fhs);

	kfree(proc->p_name);
	kfree(proc);
}

/*
 * Create the process structure for the kernel. Initialize the master process array, allprocs
 */
void
proc_bootstrap(void)
{
	kproc = proc_create(KERNELPROC);
	if (kproc == NULL) {
		panic("proc_create for kproc failed\n");
	}

	procarray_init(&allprocs);
	procarray_setsize(&allprocs,MAX_PID);
	spinlock_init(&sp_numprocs);
	spinlock_init(&sp_allprocs);
	numprocs = 1;
	next_pid = 1;
}

/*
 * Create a fresh proc for use by runprogram.
 *
 * It will have no address space and will inherit the current
 * process's (that is, the kernel menu's) current directory.
 */
struct proc *
proc_create_runprogram(const char *name)
{
	struct proc *newproc;

	newproc = proc_create(name);
	if (newproc == NULL) {
		return NULL;
	}

	/* VM fields */

	newproc->p_addrspace = NULL;

	/* VFS fields */

	/*
	 * Lock the current process to copy its current directory.
	 * (We don't need to lock the new process, though, as we have
	 * the only reference to it.)
	 */
	spinlock_acquire(&curproc->p_lock);
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		newproc->p_cwd = curproc->p_cwd;
	}
	spinlock_release(&curproc->p_lock);

	return newproc;
}

/*
 * Add a thread to a process. Either the thread or the process might
 * or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
int
proc_addthread(struct proc *proc, struct thread *t)
{
	int spl;

	KASSERT(t->t_proc == NULL);

	spinlock_acquire(&proc->p_lock);
	proc->p_numthreads++;
	spinlock_release(&proc->p_lock);

	spl = splhigh();
	t->t_proc = proc;
	splx(spl);

	return 0;
}

/*
 * Remove a thread from its process. Either the thread or the process
 * might or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
void
proc_remthread(struct thread *t)
{
	struct proc *proc;
	int spl;

	proc = t->t_proc;
	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	KASSERT(proc->p_numthreads > 0);
	proc->p_numthreads--;
	spinlock_release(&proc->p_lock);

	spl = splhigh();
	t->t_proc = NULL;
	splx(spl);
}

/*
 * Fetch the address space of (the current) process.
 *
 * Caution: address spaces aren't refcounted. If you implement
 * multithreaded processes, make sure to set up a refcount scheme or
 * some other method to make this safe. Otherwise the returned address
 * space might disappear under you.
 */
struct addrspace *
proc_getas(void)
{
	struct addrspace *as;
	struct proc *proc = curproc;

	if (proc == NULL) {
		return NULL;
	}

	spinlock_acquire(&proc->p_lock);
	as = proc->p_addrspace;
	spinlock_release(&proc->p_lock);
	return as;
}

/*
 * Change the address space of (the current) process. Return the old
 * one for later restoration or disposal.
 */
struct addrspace *
proc_setas(struct addrspace *newas)
{
	struct addrspace *oldas;
	struct proc *proc = curproc;

	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	oldas = proc->p_addrspace;
	proc->p_addrspace = newas;
	spinlock_release(&proc->p_lock);
	return oldas;
}

/*
 * Assign an available pid to the process passed as argument
 * Since the pid assigned is incremented for the next process,
 * it is possible that we may reach the end of the pid_t type.
 * In such a case, reset to 0 and assign the smallest unassigned number.
 */
pid_t // TODO: test this function
proc_assignpid(struct proc *newproc){
	// use the value of next_pid, if overflow then set to 0 for the next process.
	KASSERT(newproc != NULL);
	
	// make sure that the number of processes is less than MAX_PID 
	KASSERT(numprocs < MAX_PID);

	pid_t ret = 0;
	for(ret = 1;ret < MAX_PID; ret++){
		if(procarray_get(&allprocs,ret) == NULL){
			break;
		}
	}

	/* proc_create will call this function only when numprocs < MAX_PID */
	KASSERT(ret != MAX_PID);
	KASSERT(procarray_get(&allprocs,ret) == NULL);

	return ret;
}

/*
 * Return the process with process id ppid
 */
struct proc*
get_process(pid_t ppid){
	if(ppid < 0 || ppid >= MAX_PID){
		return NULL;
	}

	return procarray_get(&allprocs,ppid);
}
