#include <types.h>
#include <proctable.h>

static void kill_zombies(void);
static pid_t proctable_getpid(void);
static void add_kernproc(void);

/*
 * An array of processes
 */
DECLARRAY(proc,static __UNUSED inline);
DEFARRAY(proc,static __UNUSED inline);

typedef struct p_table{
	pid_t limit;
	int num;
	struct procarray *allprocs;
} p_table;

static p_table *process_t;

int proctable_init(int SYS_PROC_LIMIT){

	process_t = kmalloc(sizeof(p_table));
	if(process_t == NULL){
		return ERROR;
	}

	process_t->limit = SYS_PROC_LIMIT;
	process_t->num = 0;

	/*process_t->lk = kmalloc(sizeof(struct spinlock));
	if(process_t->lk == NULL){
		kfree(process_t);
		return ERROR;
	}
	spinlock_init(process_t->lk);*/
	

	lk_proctable = lock_create("[SYSTEM]");	
	if(lk_proctable == NULL){
		kfree(process_t);
		return ERROR;
	}

	process_t->allprocs = kmalloc(sizeof(struct procarray));
	if(process_t->allprocs == NULL){
		lock_destroy(lk_proctable);
		//lock_cleanup(process_t->lk);
		//kfree(process_t->lk);
		kfree(process_t);
		return ERROR;
	}
	procarray_init(process_t->allprocs);
	procarray_setsize(process_t->allprocs,SYS_PROC_LIMIT);
	
	// Add kernel process to the proctable
	add_kernproc();

	return SUCC;
}

/* 	
*	Add a process to the process table,
*	assign the smallest available pid.
*	process is stored at idx = pid.
*/
pid_t proctable_add(struct proc* proc){
	// is table full?
	if(is_proctable_full()){
		return ERROR;
	}

	bool held = lock_do_i_hold(lk_proctable);
	if(!held){
		lock_acquire(lk_proctable);
	}

	kill_zombies();

	pid_t newpid = proctable_getpid();
	if(newpid == ERROR){
		if(!held){
			lock_release(lk_proctable);
		}

		return ERROR;
	}

	procarray_set(process_t->allprocs, newpid, proc);

	process_t->num = process_t->num + 1; 

	if(!held){
		lock_release(lk_proctable);
	}

	return newpid;
}

/* 	
*	Remove a process from the process table
*/
void proctable_remove(pid_t pid){

	bool held = lock_do_i_hold(lk_proctable);
	if(!held){
		lock_acquire(lk_proctable);
	}

	procarray_set(process_t->allprocs, pid, NULL);

	process_t->num = process_t->num - 1;

	if(!held){
		lock_release(lk_proctable);
	}
}

/*
*	Get process with given process id
*/
struct proc* proctable_get(pid_t pid){

	// validate pid
	if(!is_valid_pid(pid)){
		return NULL;
	}

	bool held = lock_do_i_hold(lk_proctable);
	if(!held){
		lock_acquire(lk_proctable);
	}

	struct proc *proc = procarray_get(process_t->allprocs, pid);

	if(!held){
		lock_release(lk_proctable);
	}

	return proc;
}

struct proc* proctable_getchild(pid_t pid){

	int idx = 0;
	struct proc* child = NULL;

	bool held = lock_do_i_hold(lk_proctable);
	if(!held){
		lock_acquire(lk_proctable);
	}

	for(idx = 1;idx < process_t->num; idx++){
		child = proctable_get(idx);
		if(child == NULL){
			continue;
		}else if(child->pid == pid){
			break;
		}
	}

	if(!held){
		lock_release(lk_proctable);
	}

	return child;
}

bool is_proctable_full(void){
	KASSERT(process_t != NULL);
	KASSERT(process_t->allprocs != NULL);

	bool ret = false;

	bool held = lock_do_i_hold(lk_proctable);
	if(!held){
		lock_acquire(lk_proctable);
	}

	if(process_t->num == process_t->limit){
		ret = true;
	}

	if(!held){
		lock_release(lk_proctable);
	}

	return ret;
}

bool is_valid_pid(pid_t pid){
	return ((pid < 0) || (pid >= process_t->limit)) ? false : true;  
}

bool proc_exists(pid_t pid){

	bool ret = true;
	bool held = lock_do_i_hold(lk_proctable);
	if(!held){
		lock_acquire(lk_proctable);
	}
	
	struct proc *proc = proctable_get(pid);
	if(proc == NULL){
		ret = false;
	}

	if(proc->p_state == S_ZOMBIE){
		ret = false;
	}

	if(!held){
		lock_release(lk_proctable);
	}

	return ret;
}

static
void add_kernproc(void){

	lock_acquire(lk_proctable);

	procarray_set(process_t->allprocs, KERNEL_PID, kproc);

	process_t->num = process_t->num + 1;

	lock_release(lk_proctable);
}

static
void kill_zombies(void){
	int i = 0;
	
	bool held = lock_do_i_hold(lk_proctable);
	if(!held){
		lock_acquire(lk_proctable);
	}

	for(i = 0; i < process_t->limit; i++){
		struct proc *tmp = procarray_get(process_t->allprocs,i);
		if(tmp != NULL){
			if(tmp->p_state == S_ZOMBIE && !tmp->iswaiting){
				proctable_remove(tmp->pid);
				proc_destroy(tmp);
			}

		}
	}

	if(!held){
		lock_release(lk_proctable);
	}
}

/* Find a new pid and remove all zombie procs */
static 
pid_t proctable_getpid(void){
	
	// re-check the number of processes
	KASSERT(process_t->num <= process_t->limit); 
	
	bool held = lock_do_i_hold(lk_proctable);
	if(!held){
		lock_acquire(lk_proctable);
	}
	
	if(process_t->num == process_t->limit){
		if(!held){
			lock_release(lk_proctable);
		}

		return ERROR;
	}

	int ret = 0;
	for(ret = 0;ret < process_t->limit; ret++){
		struct proc *tmp = procarray_get(process_t->allprocs,ret); 
		if(tmp == NULL){
			break;
		}
	}

	if(!held){
		lock_release(lk_proctable);
	}

	return ret;
}
