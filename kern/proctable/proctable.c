#include <types.h>
#include <proctable.h>

static void kill_zombies(void);
static pid_t proctable_getpid(void);

/*
 * An array of processes
 */
DECLARRAY(proc,static __UNUSED inline);
DEFARRAY(proc,static __UNUSED inline);

typedef struct p_table{
	pid_t limit;
	int num;
	struct lock *lk;
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

	/*process_t->lock = kmalloc(sizeof(struct lock));
	if(process_t->lock == NULL){
		kfree(process_t);
		return ERROR;
	}
	spinlock_init(process_t->lock);
	*/

	process_t->lk = lock_create("[SYSTEM]");	
	if(process_t->lk == NULL){
		kfree(process_t);
		return ERROR;
	}

	process_t->allprocs = kmalloc(sizeof(struct procarray));
	if(process_t->allprocs == NULL){
		lock_destroy(process_t->lk);
		kfree(process_t);
		return ERROR;
	}
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

	lock_acquire(process_t->lk);

	pid_t newpid = proctable_getpid();
	if(newpid == ERROR){
		return ERROR;
	}

	procarray_set(process_t->allprocs, newpid, proc);

	process_t->num = process_t->num + 1; 

	lock_release(process_t->lk);

	return newpid;
}

/* 	
*	Remove a process from the process table
*/
void proctable_remove(pid_t pid){

	// Obtain lock first
	lock_acquire(process_t->lk);

	procarray_set(process_t->allprocs, pid, NULL);

	process_t->num = process_t->num - 1;

	lock_release(process_t->lk);
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

	if(process_t->num== process_t->limit){
		return true;
	}

	return false;
}

bool is_valid_pid(pid_t pid){
	return ((pid < 0) || (pid >= process_t->limit)) ? false : true;  
}

static
void kill_zombies(void){
	int i = 0;
	for(i = 0; i < process_t->limit; i++){
		struct proc *tmp = procarray_get(process_t->allprocs,i);
		if(tmp != NULL){
			if(tmp->p_state == S_ZOMBIE && !tmp->iswaiting){
				proc_destroy(tmp);
				break;
			}
		}
	}
}

/* Find a new pid and remove all zombie procs */
static 
pid_t proctable_getpid(void){
	
	// re-check the number of processes
	KASSERT(process_t->num <= process_t->limit); 
	if(process_t->num == process_t->limit){
		return ERROR;
	}

	kill_zombies();

	int ret = 0;
	for(ret = 0;ret < process_t->limit; ret++){
		struct proc *tmp = procarray_get(process_t->allprocs,ret); 
		if(tmp == NULL){
			break;
		}
	}

	return ret;
}
