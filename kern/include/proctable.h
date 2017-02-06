#ifndef _PROCLIST_H_
#define _PROCLIST_H_

#include <limits.h>
#include <lib.h>
#include <spinlock.h>
#include <synch.h>
#include <array.h>
#include <thread.h>
#include <proc.h>

int proctable_init(int);
pid_t proctable_add(struct proc*);
void proctable_remove(pid_t);
struct proc* proctable_get(pid_t);
struct proc* proctable_getchild(pid_t);
bool is_proctable_full(void);
bool is_valid_pid(pid_t);
bool proc_exists(pid_t pid);

extern struct lock* lk_proctable;

#endif /* _PROCLIST_H_ */
