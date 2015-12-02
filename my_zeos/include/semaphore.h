#ifndef _SEMAPHORE_H__
#define _SEMAPHORE_H__

void init_semaphores();

int sem_init(int n_sem, unsigned int value);

#endif /* _SEMAPHORE_H__ */
