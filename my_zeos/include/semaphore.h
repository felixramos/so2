#ifndef _SEMAPHORE_H__
#define _SEMAPHORE_H__

void init_semaphores();

int sem_init(int n_sem, unsigned int value);
int sem_wait(int n_sem);
int sem_signal(int n_sem);

#endif /* _SEMAPHORE_H__ */
