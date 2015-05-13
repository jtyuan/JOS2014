#include <inc/lib.h>

uint32_t share_val = 0;

void umain(int argc, char* argv[]) {
    envid_t who;
    if((who = sfork()) == 0) {
        cprintf("child: %d\n", share_val);
        share_val = 1000;
        sys_yield();
        cprintf("child: %d\n", share_val);
        share_val = 10000;
        return;
    }


    sys_yield();
    sys_yield();
    cprintf("parent: %d\n", share_val);
    share_val++;
    sys_yield();
    sys_yield();
    cprintf("parent: %d\n", share_val);
    return;
}
