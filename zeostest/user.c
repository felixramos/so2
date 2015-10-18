#include <libc.h>

int __attribute__ ((__section__(".text.main")))
main(void)
{
    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
    /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */
    
    int x;
    
    if (fork()>0) // parent
    {
        char *b = "hola";
        
        do
        {
            x = q3send(b, sizeof(b));
        }
        while (x != 0);
    }
    
    else // child
    {
        int s;
        char b[5];
        
        do
        {
            s = 5;
            x = q3recv(b, &s);
        }
        while (x != 0);
    }
    while (1);
}
