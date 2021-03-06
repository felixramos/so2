/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <errno.h>

#define LECTURA 0
#define ESCRIPTURA 1

int global_PID = 2;

int check_fd(int fd, int permissions)
{
    if (fd!=1) return -9; /*EBADF*/
    if (permissions!=ESCRIPTURA) return -13; /*EACCES*/
    return 0;
}

int sys_ni_syscall()
{
    return -38; /*ENOSYS*/
}

int sys_getpid()
{
    return current()->PID;
}

int ret_from_fork()
{
	return 0;
}

int sys_fork()
{
    // creates the child process
    
    /* any free PCB? */
    if (list_empty(&freequeue)) return -ENOMEM;
    
    /* getting a free PCB for the child process */
    struct list_head *lh = list_first(&freequeue);
    list_del(lh);
    
    /* copying the parent's task_union to the child */
    union task_union *tu_child = (union task_union*)list_head_to_task_struct(lh);
    copy_data(current(), tu_child, sizeof(union task_union));
    
    /* initializing the child's dir_pages_baseAddr field with a new directory */
    allocate_DIR((struct task_struct*)tu_child);
    
    /* searching physical pages in which to map the logical pages for data+stack of the child process */
    page_table_entry *child_PT = get_PT(&tu_child->task);
    int i;
    for (i=0; i<NUM_PAG_DATA; i++)
    {
        int new_ph_pag = alloc_frame();
        
        if (new_ph_pag != -1) // one physical page allocated
        {
            set_ss_pag(child_PT, PAG_LOG_INIT_DATA + i, new_ph_pag);
        }
        
        else // there isn't any frame (physical page) available
        {
            // deallocating allocated physical pages
            int j;
            for (j=0; j<i; j++)
            {
                free_frame(get_frame(child_PT, PAG_LOG_INIT_DATA + j));
                del_ss_pag(child_PT, PAG_LOG_INIT_DATA + j);
            }
            
            // freeing the PCB
            list_add_tail(lh, &freequeue);
            
            // returning the error
            return -EAGAIN;
        }
    }
    
	/* sharing parent's SYSTEM CODE+DATA and USER CODE with child */
	page_table_entry *parent_PT = get_PT(current());
	// associating system code+data logical pages with physical pages (frames)
	for (i=0; i<NUM_PAG_KERNEL; i++)
		set_ss_pag(child_PT, i, get_frame(parent_PT, i));
	// associating user code logical pages with physical pages (frames)
	for (i=0; i<NUM_PAG_CODE; i++)
		set_ss_pag(child_PT, PAG_LOG_INIT_CODE + i, get_frame(parent_PT, PAG_LOG_INIT_CODE + i));

	/* copying parent's USER DATA to child */
	for (i=NUM_PAG_KERNEL+NUM_PAG_CODE; i<NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA; i++)
	{	
		// mapping one child's physical page to parent's address space (only for the copy)
		set_ss_pag(parent_PT, i + NUM_PAG_DATA, get_frame(child_PT, i));
		// copying one page
		copy_data((void*)(i<<12), (void*)((i + NUM_PAG_DATA)<<12), PAGE_SIZE);

		del_ss_pag(parent_PT, i + NUM_PAG_DATA);
	}

	/* mapping parent's ebp to child's stack */
	int register_ebp;
	__asm__ __volatile__ (
                          "movl %%ebp, %0\n\t"
                          : "=g" (register_ebp)
                          :
                          );
	tu_child->task.register_esp = (register_ebp - (int)current()) + (int)tu_child;

	/* updating child's stack */
	*(DWord*)(tu_child->task.register_esp) = (DWord)&ret_from_fork;
	tu_child->task.register_esp -= sizeof(DWord);
	*(DWord*)(tu_child->task.register_esp) = 0; // this value does not matter (it will never be used)

	/* queuing child process into readyqueue */
	tu_child->task.PID = global_PID++;
	list_add_tail(&(tu_child->task.list), &readyqueue);

    return tu_child->task.PID;
}

int sys_clone(void (*function)(void), void *stack)
{
    // creates the child process
    
    /* any free PCB? */
    if (list_empty(&freequeue)) return -ENOMEM;
    
    /* getting a free PCB for the child process */
    struct list_head *lh = list_first(&freequeue);
    list_del(lh);
    
    /* copying the parent's task_union to the child */
    union task_union *tu_child = (union task_union*)list_head_to_task_struct(lh);
    copy_data(current(), tu_child, sizeof(union task_union));

	/* increasing current dir references */
	increase_DIR_refs(get_DIR(current()));
    
	/* mapping parent's ebp to child's stack */
	int register_ebp;
	__asm__ __volatile__ (
                          "movl %%ebp, %0\n\t"
                          : "=g" (register_ebp)
                          :
                          );
	tu_child->task.register_esp = (register_ebp - (int)current()) + (int)tu_child;

	/* updating child's stack */
	*(DWord*)(tu_child->task.register_esp) = (DWord)&ret_from_fork;
	tu_child->task.register_esp -= sizeof(DWord);
	*(DWord*)(tu_child->task.register_esp) = 0; // this value does not matter (it will never be used)
    tu_child->stack[KERNEL_STACK_SIZE - 5] = (unsigned long) function;
    tu_child->stack[KERNEL_STACK_SIZE - 2] = (unsigned long) stack;

	/* queuing child process into readyqueue */
	tu_child->task.PID = global_PID++;
    tu_child->task.state=READY;
	list_add_tail(&(tu_child->task.list), &readyqueue);

    return tu_child->task.PID;
}

void sys_exit()
{
	if (free_DIR(get_DIR(current())))
	{
		/* Deallocating all the propietary physical pages */
		page_table_entry *process_PT = get_PT(current());
		int i;
		for (i=0; i<NUM_PAG_DATA; i++)
		{
			free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA + i));
			del_ss_pag(process_PT, PAG_LOG_INIT_DATA + i);
		}
	}
	
	/* freeing the PCB */
	list_add_tail(&(current()->list), &freequeue);
	
	current()->PID=-1;
  
 	/* restarting execution of the next process */
	sched_next_rr();
}

/************************************************/

#define BUFFER_SIZE 512

int sys_write(int fd, char *buffer, int nbytes)
{    
    char localbuffer [BUFFER_SIZE];
    int bytes_left;
    int ret;
    
    if ((ret = check_fd(fd, ESCRIPTURA)))
        return ret;
    if (nbytes < 0)
        return -EINVAL;
    if (!access_ok(VERIFY_READ, buffer, nbytes))
        return -EFAULT;
    
    bytes_left = nbytes;
    while (bytes_left > BUFFER_SIZE) {
        copy_from_user(buffer, localbuffer, BUFFER_SIZE);
        ret = sys_write_console(localbuffer, BUFFER_SIZE);
        bytes_left-=ret;
        buffer+=ret;
    }
    if (bytes_left > 0) {
        copy_from_user(buffer, localbuffer,bytes_left);
        ret = sys_write_console(localbuffer, bytes_left);
        bytes_left-=ret;
    }
    return (nbytes-bytes_left);
}

extern int zeos_ticks;

int sys_gettime()
{
    return zeos_ticks;
}
