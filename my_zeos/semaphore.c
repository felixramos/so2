#include <semaphore.h>
#include <list.h>
#include <errno.h>
#include <sched.h>

#define MAX_SEM 20

struct semaphore {
	int counter;
	int owner;
	struct list_head blocked;
};

struct semaphore semaphores[MAX_SEM];

void init_semaphores()
{
	int i;
	for(i=0; i<MAX_SEM; i++) {
		semaphores[i].owner = -1;
		INIT_LIST_HEAD(&semaphores[i].blocked);
	}
}

int sys_sem_init(int n_sem, unsigned int value)
{
	if ((n_sem<0) || (n_sem>=MAX_SEM)) return -EBADF;
	if (semaphores[n_sem].owner != -1) return -EPERM; // already initialized 
	semaphores[n_sem].owner = current()->PID;
	semaphores[n_sem].counter = value;
	return 0;
}

int sys_sem_wait(int n_sem)
{
	if ((n_sem<0) || (n_sem>=MAX_SEM)) return -EBADF;
	if (semaphores[n_sem].owner == -1) return -EBADF;	// already dead
	
	if (semaphores[n_sem].counter <=0) {
		update_process_state_rr(current(), &semaphores[n_sem].blocked);
		sched_next_rr();
		if (semaphores[n_sem].owner == -1) return -1;
	}

    else
    {
		semaphores[n_sem].counter--;
	}
		
	return 0;
}
