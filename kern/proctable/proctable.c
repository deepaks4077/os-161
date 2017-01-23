#include <proctable.h>

static pid_t proctable_getpid(void);

/*
 * An array of processes
 */
DECLARRAY(proc,static __UNUSED inline);
DEFARRAY(proc,static __UNUSED inline);

static typedef struct p_table{
	uint32_t limit;
	uint32_t num;
	struct lock *lock;
	struct procarray *allprocs;
} *process_t;

int proctable_init(uint32_t SYS_PROC_LIMIT){

	process_t->limit = SYS_PROC_LIMIT;
	process_t->lock = lock_create("SYSTEM");

	if(process_t->lock == NULL){
		return ERROR;
	}

	process_t->num = 0;
	procarray_init(process_t->allprocs);
	procarray_setsize(process_t->allprocs,SYS_PROC_LIMIT);

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

	lock_acquire(process_t->t_lock);

	pid_t newpid = proctable_getpid();
	if(newpid == ERROR){
		return ERROR;
	}

	procarray_set(process_t->allprocs, newpid, proc);

	process_t->num = process_t->num + 1; 

	lock_release(process_t->lock);

	return newpid;
}

/* 	
*	Remove a process from the process table
*/
void proctable_remove(pid_t pid){

	// Obtain lock first
	lock_acquire(process_t->lock);

	procarray_set(process_t->allprocs, newpid, NULL);

	process_t->num = process_t->num - 1;

	lock_release(process_t->lock);
}

/*
*	Get process with given process id
*/
struct proc* proctable_get(pid_t pid){

	// validate pid
	if(!is_valid_pid(pid)){
		return NULL;
	}

	return procarray_get(process_t->allprocs, pid);
}

bool is_proctable_full(void){
	KASSERT(process_t != NULL);
	KASSERT(process_t->allprocs != NULL);

	if(procarray_num(process_t->allprocs) == process_t->max){
		return false;
	}

	return true;
}

bool is_valid_pid(pid_t pid){
	return ((pid < 0) || (pid >= process_t->max)) ? true : false;  
}

static 
pid_t proctable_getpid(void){
	
	// re-check the number of processes
	KASSERT(process_t->num <= process_t->num); 
	if(process_t->num == process_t->num){
		return ERROR;
	}

	pid_t ret = 0;
	for(ret = 1;ret < process_t->num; ret++){
		if(procarray_get(process_t->allprocs,ret) == NULL){
			break;
		}
	}

	return ret;
}