/*********************************************************
Programmer  : EOF
E-mail      : jasonleaster@gmail.com
Date        : 2015.04.23
File        : sfork.c

Description:
    This code is used for tesing function @sfork which
is implement in lib/fork.c

**********************************************************/
#include <inc/lib.h>

uint32_t val = 0;

void umain(int argc, char* argv[])
{
    envid_t who;
    if((who = sfork()) == 0)
    {
        cprintf("in child val = %d\n", val);
        val = 2;
        sys_yield();
        cprintf("in child val = %d\n", val);
        val = 10;

        return;
    }

    sys_yield();
    cprintf("in parent  val = %d\n", val);
    val++;
    sys_yield();
    cprintf("in parent  val = %d\n", val);
    return ;
}
