/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>

/**
 * Container for the Task array and 2 additional pages (the first and the last one)
 * to protect against out of bound accesses.
 */
union task_union protected_tasks[NR_TASKS+2]
  __attribute__((__section__(".data.task")));

union task_union *task = &protected_tasks[1]; /* == union task_union task[NR_TASKS] */

#if 0
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}
#endif

extern struct list_head blocked;


/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t) 
{
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t) 
{
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}


int allocate_DIR(struct task_struct *t) 
{
	int pos;

	pos = ((int)t-(int)task)/sizeof(union task_union);

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos]; 

	return 1;
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

struct task_struct *idle_task = NULL;

void init_idle (void)
{
	struct list_head *l = list_first(&freequeue); // get an available task_union from the freequeue

	list_del(l);                                  // delete it from the freequeue

	struct task_struct *pcb = list_head_to_task_struct(l);

	pcb->PID = 0;

	allocate_DIR(pcb); //assign the process address space (initialize pcb->dir_pages_baseAddr field)

	union task_union *tu = (union task_union*)pcb;

	tu->stack[KERNEL_STACK_SIZE-1] = (unsigned long)&cpu_idle;

	tu->stack[KERNEL_STACK_SIZE-2] = 0;

	pcb->register_esp = (int)&(tu->stack[KERNEL_STACK_SIZE-2]);

	idle_task = pcb;
}

void init_task1(void)
{
	struct list_head *l = list_first(&freequeue); // get an available task_union from the freequeue

	list_del(l);                                  // delete it from the freequeue

	struct task_struct *pcb = list_head_to_task_struct(l);

	pcb->PID = 1;

	allocate_DIR(pcb); //assign the process address space (initialize pcb->dir_pages_baseAddr field)

	set_user_pages(pcb);

	union task_union *tu = (union task_union*)pcb;

	tss.esp0 = (DWord)&(tu->stack[KERNEL_STACK_SIZE]);

	set_cr3(pcb->dir_pages_baseAddr);
}

struct list_head freequeue;  // Free task structs
struct list_head readyqueue; // Ready queue

void init_sched(){

  INIT_LIST_HEAD(&freequeue);	// initialize freequeue

  int i;
  for(i=0; i<NR_TASKS; i++)	// adding ALL task_structs to freequeue
  {
    task[i].task.PID = -1;
    list_add_tail(&task[i].task.list, &freequeue);
  }

  INIT_LIST_HEAD(&readyqueue);	// initialize readyqueue
}

struct task_struct* current()
{
  int ret_value;
  
  __asm__ __volatile__(
  	"movl %%esp, %0"
	: "=g" (ret_value)
  );
  return (struct task_struct*)(ret_value&0xfffff000);
}

struct task_struct* list_head_to_task_struct(struct list_head *l)
{
	return (struct task_struct*)((int)l&0xfffff000);
}

void inner_task_switch(union task_union *new)
{
	/* updating TSS to make it point to the new (system) stack */
	tss.esp0 = (DWord)&new->stack[KERNEL_STACK_SIZE];

	/* changing the user address space (TLB flush) */
	set_cr3(get_DIR(&new->task));

	/* storing the current value of the ebp resgister in the (current) PCB */
	__asm__ __volatile__(
		"movl %%ebp, %0\n\t"
		: "=g" (current()->register_esp)
		:
	);

	/* changing the current system stack by setting the esp register (restoring new esp) */
	__asm__ __volatile__(
		"movl %0, %%esp\n\t"
		:
		: "g" (new->task.register_esp)
	);

	/* Now, we are in the NEW process!! */

	/* restoring new ebp from the (new) stack */
	__asm__ __volatile__(
		"popl %%ebp\n\t"
		::
	);

	/* return to the routine that called this one */
	__asm__ __volatile__(
		"ret\n\t"
		::
	);
}

void task_switch(union task_union *new)
{
	__asm__ __volatile__(
		"pushl %esi\n\t"
		"pushl %edi\n\t"
		"pushl %ebx\n\t"
	);
	
	inner_task_switch(new);

	__asm__ __volatile__(
		"popl %ebx\n\t"
		"popl %edi\n\t"
		"popl %esi\n\t"
	);
}
