#ifndef JOS_KERN_MONITOR_H
#define JOS_KERN_MONITOR_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

struct Trapframe;

// Activate the kernel monitor,
// optionally providing a trap frame indicating the current state
// (NULL if none).
void monitor(struct Trapframe *tf);
void monitor_jdb(struct Trapframe *tf);

// Functions implementing monitor commands.
int mon_help(int argc, char **argv, struct Trapframe *tf);
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);
int mon_backtrace(int argc, char **argv, struct Trapframe *tf);
int mon_showmappings(int argc, char **argv, struct Trapframe *tf);
int mon_setperm(int argc, char **argv, struct Trapframe *tf);
int mon_dump(int argc, char **argv, struct Trapframe *tf);
int mon_shutdown(int argc, char **argv, struct Trapframe *tf);
int mon_jdb(int argc, char **argv, struct Trapframe *tf);

int jdb_help(int argc, char **argv, struct Trapframe *tf);
int jdb_si(int argc, char **argv, struct Trapframe *tf);
int jdb_con(int argc, char **argv, struct Trapframe *tf);
int jdb_quit(int argc, char **argv, struct Trapframe *tf);

#endif	// !JOS_KERN_MONITOR_H
