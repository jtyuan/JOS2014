#include <inc/lib.h>

void
umain(int argc, char **argv)
{
    int i;
    for (i = 0; i < 16; ++i) {
        int who;
        if ((who = pfork(i)) == 0) {
            cprintf("env[%d] is running with cur_pr=%d!\n", i, i);
            int j;
            for (j = 0; j < 16; ++j) {
                cprintf("env[%d] yields!\n", i);
                sys_yield();
            }
            break;
        }
    }
}