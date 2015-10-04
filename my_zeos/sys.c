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
    
    return tu_child->task.PID;
}

void sys_exit()
{
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
