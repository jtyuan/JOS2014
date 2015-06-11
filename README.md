# JOS Lab 4 Report<br/><small><small><small><small>江天源 1100016614</small></small></small></small>

## 总体概述


### 完成情况

|#|E1|E2|E3|E4|E5|E6|E7|E8|E9|E10|E11|E12|E13|E14|E15|
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|Status|√|√|√|√|√|√|√|√|√|√|√|√|√|√|√|

|#|Q1|Q2|Q3|Q4|C1|C2|C3|C4|C5|C6|C7|C8|C9|C10|C11|
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|Status|√|√|√|√|×|√|×|×|×|√|×|×|×|×|×|

<small>* 其中E#代表Exercise #, Q#代表Question #, C# 代表Challenge #</small>

共完成了2个Challenge。

`make grade`结果

![](http://ww2.sinaimg.cn/large/6313a6d8jw1es2scvk8poj20xo0mu79i.jpg =600x)

### Part A: User Environments and Exception Handling

#### Exercise 1

>Implement `mmio_map_region` in `kern/pmap.c`. To see how this is used, look at the beginning of `lapic_init` in kern/lapic.c. You'll have to do the next exercise, too, before the tests for `mmio_map_region` will run.

调用之前lab中编写的`boot_map_region`进行映射即可，更新`base`并返回旧的起始地址`old_base`

```java
void *
mmio_map_region(physaddr_t pa, size_t size)
{
   static uintptr_t base = MMIOBASE;

   size = ROUNDUP(size, PGSIZE);

   if (base + size > MMIOLIM) 
      panic("mmio_map_region: overflow");
   
   boot_map_region(kern_pgdir, base, size, pa, PTE_PCD | PTE_PWT | PTE_W);

   uintptr_t old_base = base;
   base += size;
   return (void*)old_base;
}
```

#### Exercise 2

>Exercise 2. Read `boot_aps()` and `mp_main()` in `kern/init.c`, and the assembly code in `kern/mpentry.S`. Make sure you understand the control flow transfer during the bootstrap of APs. Then modify your implementation of `page_init()` in `kern/pmap.c` to avoid adding the page at `MPENTRY_PADDR` to the free list, so that we can safely copy and run AP bootstrap code at that physical address. Your code should pass the updated `check_page_free_list()` test (but might fail the updated `check_kern_pgdir()` test, which we will fix soon).

在`page_init`的最后加上

```java
pages[range_mmio+1].pp_link = pages[range_mmio].pp_link;
pages[range_mmio].pp_link = NULL;
```

将`MPENTRY_PADDR`所在的页从空闲页表链中移除即可，其中`range_mmio = PGNUM(MPENTRY_PADDR)`。

#### Question 1

>Compare `kern/mpentry.S` side by side with `boot/boot.S`. Bearing in mind that `kern/mpentry.S` is compiled and linked to run above `KERNBASE` just like everything else in the kernel, what is the purpose of macro `MPBOOTPHYS`? Why is it necessary in `kern/mpentry.S` but not in `boot/boot.S`? In other words, what could go wrong if it were omitted in `kern/mpentry.S`? 
Hint: recall the differences between the link address and the load address that we have discussed in Lab 1.

`mpentry.S`的代码运行在物理内存中`MPENTRY_PADDR(0x7000)`开始的位置，而宏`MPBOOTPHYS`则是将给定的链接地址转化成对应的物理地址。如果不使用这个宏则无法正确的找到物理地址。

#### Exercise 3

>Modify `mem_init_mp()` (in `kern/pmap.c`) to map per-CPU stacks starting at `KSTACKTOP`, as shown in `inc/memlayout.h`. The size of each stack is `KSTKSIZE` bytes plus `KSTKGAP` bytes of unmapped guard pages. Your code should pass the new check in `check_kern_pgdir()`.

按照`memlayout.h`中所示的结构，将每个CPU的Kernel Stack依次映射过去即可

![](http://ww4.sinaimg.cn/large/6313a6d8jw1es2t5qd2nwj20ty09m767.jpg =600x)

```java
static void
mem_init_mp(void)
{
   int i;
   uintptr_t kstacktop_i;
   for (i = 0; i < NCPU; ++i) {
      kstacktop_i = KSTACKTOP - i * (KSTKSIZE + KSTKGAP);
      boot_map_region(kern_pgdir, kstacktop_i-KSTKSIZE, 
         KSTKSIZE, PADDR(percpu_kstacks[i]), PTE_W);
   }
}
```

#### Exercise 4

>The code in `trap_init_percpu()` (`kern/trap.c`) initializes the `TSS` and `TSS descriptor` for the `BSP`. It worked in Lab 3, but is incorrect when running on other CPUs. Change the code so that it can work on all CPUs. (Note: your new code should not use the global ts variable any more.)

将lab3中的全局变量替换成对应每个CPU的变量即可：

```java
void
trap_init_percpu(void)
{
   int cpuid = cpunum();

   thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - cpuid * (KSTKSIZE + KSTKGAP);
   thiscpu->cpu_ts.ts_ss0 = GD_KD;

   // Initialize the TSS slot of the gdt.
   gdt[(GD_TSS0 >> 3) + cpuid] = SEG16(STS_T32A, (uint32_t) (&thiscpu->cpu_ts),
               sizeof(struct Taskstate) - 1, 0);
   gdt[(GD_TSS0 >> 3) + cpuid].sd_s = 0;

   // Load the TSS selector (like other segment selectors, the
   // bottom three bits are special; we leave them 0)
   ltr(GD_TSS0 + 8 * cpuid);

   // Load the IDT
   lidt(&idt_pd);
}

```

#### Exercise 5

>Apply the big kernel lock as described above, by calling `lock_kernel()` and `unlock_kernel()` at the proper locations.

根据提示在`i386_init()`中`BSP`唤醒其他CPU前加锁，在`mp_main()`中初始化AP前加锁，在`trap()`中陷入用户态前加锁，在`env_run()`中切换到用户态前解锁即可。

#### Question 2

>It seems that using the big kernel lock guarantees that only one CPU can run the kernel code at a time. Why do we still need separate kernel stacks for each CPU? Describe a scenario in which using a shared kernel stack will go wrong, even with the protection of the big kernel lock.

如果多个CPU共享一个栈，CPU之间使用栈(push、pop)的顺序是不一定的，可能出现一个CPU push的内容被另一个CPU pop出来的情况。


#### Challenge 1

>The big kernel lock is simple and easy to use. Nevertheless, it eliminates all concurrency in kernel mode. Most modern operating systems use different locks to protect different parts of their shared state, an approach called fine-grained locking. Fine-grained locking can increase performance significantly, but is more difficult to implement and error-prone. If you are brave enough, drop the big kernel lock and embrace concurrency in JOS!

>It is up to you to decide the locking granularity (the amount of data that a lock protects). As a hint, you may consider using spin locks to ensure exclusive access to these shared components in the JOS kernel:

>
- The page allocator.
- The console driver.
- The scheduler.
- The inter-process communication (IPC) state that you will implement in the part C.

### Part B: Page Faults, Breakpoints Exceptions, and System Calls

#### Exercise 6

>Implement round-robin scheduling in `sched_yield()` as described above. Don't forget to modify `syscall()` to dispatch `sys_yield()`.

>Make sure to invoke `sched_yield()` in mp_main.

>Modify `kern/init.c` to create three (or more!) environments that all run the program user/yield.c.

在`kern/sched.c`中实现`sched_yield`函数，若当前有正在运行的环境，则执行其在`envs`数组中的下一个可执行`ENV_RUNNABLE`的环境；否则执行其中第一个环境；若没有其他环境，而之前有正在运行的环境，则继续执行它；除此之外的情况调用`sched_halt`：

```java
// Choose a user environment to run and run it.
void
sched_yield(void)
{
   struct Env *idle;

   // LAB 4: Your code here.
   int i, k, envidx;

   if (curenv)
      envidx = ENVX(curenv->env_id);
   else
      envidx = 0;

   for (i = 0; i < NENV; ++i) {
      k = (envidx + i) % NENV;
      if (envs[k].env_status == ENV_RUNNABLE)
         env_run(&envs[k]);
   }
   if (curenv && curenv->env_status == ENV_RUNNING)
      env_run(curenv);

   // sched_halt never returns
   sched_halt();
}

```

然后在`kern/syscall.c`中补完`sys_yield`系统调用，直接调用刚才实现的`sched_yield`即可：

```java
// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
   sched_yield();
}
```

最后在负责dispatch的`syscall`函数中加入对应的情况：

```java
   ...
   case SYS_yield:
      sys_yield();
      return 0;
   ...
```

测试：

在`i386_init`中加入几个对`user/yield`程序的执行：

```java
   ENV_CREATE(user_yield, ENV_TYPE_USER);
   ENV_CREATE(user_yield, ENV_TYPE_USER);
   ENV_CREATE(user_yield, ENV_TYPE_USER);
```

结果：

![](http://ww1.sinaimg.cn/large/6313a6d8jw1es2tsj63faj20xs11andh.jpg =600x)

一切正常。

#### Question 3

>In your implementation of `env_run()` you should have called `lcr3()`. Before and after the call to `lcr3()`, your code makes references (at least it should) to the variable `e`, the argument to `env_run`. Upon loading the `%cr3` register, the addressing context used by the `MMU` is instantly changed. But a virtual address (namely `e`) has meaning relative to a given address context--the address context specifies the physical address to which the virtual address maps. Why can the pointer `e` be dereferenced both before and after the addressing switch?

因为用户环境的页表初始化时是通过`memcpy(e->env_pgdir, kern_pgdir, PGSIZE);`语句直接从内核页表复制过来的，所以转换前后`e`所对应的虚拟地址也是一致的。

#### Question 4

>Whenever the kernel switches from one environment to another, it must ensure the old environment's registers are saved so they can be restored properly later. Why? Where does this happen?

因为不保存下来就无法正确地恢复到原来的环境。

发生在`trap.c`的`trap`函数中：`curenv->env_tf = *tf;`

#### Challenge 2

>Add a less trivial scheduling policy to the kernel, such as a fixed-priority scheduler that allows each environment to be assigned a priority and ensures that higher-priority environments are always chosen in preference to lower-priority environments. If you're feeling really adventurous, try implementing a Unix-style adjustable-priority scheduler or even a lottery or stride scheduler. (Look up "lottery scheduling" and "stride scheduling" in Google.)

>Write a test program or two that verifies that your scheduling algorithm is working correctly (i.e., the right environments get run in the right order). It may be easier to write these test programs once you have implemented fork() and IPC in parts B and C of this lab.

参考了[陈诗安师兄的静态优先级的调度策略](https://github.com/Clann24/jos/tree/master/lab4)，实现了一个简单的动态优先级的scheduling policy，在一个进程yield的时候会选择envs数组中优先级高于当前进程的第一个进程执行；每一个当前过程没能执行的进程优先级会`+1`。当一个进程被执行时优先级恢复成进程创建时的初始值。

```java
void
sched_yield(void)
{
   struct Env *idle;
   int i, k, envidx;

   if (curenv)
      envidx = ENVX(curenv->env_id);
   else
      envidx = 0;

   for (i = 0; i < NENV; ++i) {
      k = (envidx + i) % NENV;
      if (envs[k].env_status == ENV_RUNNABLE) {
         if (curenv == NULL || envs[k].env_cur_pr > curenv->env_cur_pr) {
            cprintf("env[%d] is taking over with priority %d\n", k, envs[k].env_cur_pr);
            envs[k].env_cur_pr = envs[k].env_def_pr;
            env_run(&envs[k]);
         } else {
            envs[k].env_cur_pr++;
         }
      }
   }
   if (curenv && curenv->env_status == ENV_RUNNING)
      env_run(curenv);

   // sched_halt never returns
   sched_halt();
}
```

优先级需要在进程创建时设置，因此为此设计了一个`pfork`来专门设置进程优先级（若使用`fork`来创建进程，则默认优先级为0）

```
envid_t
pfork(int priority)
{
   int r;
   envid_t envid;
   uintptr_t addr;

   set_pgfault_handler(pgfault);

   if ((envid = sys_exofork()) == 0) {
      // child
      thisenv = &envs[ENVX(sys_getenvid())];
      return 0;
   }

   for (addr = 0; addr < USTACKTOP; addr += PGSIZE) {
      if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P)
         && (uvpt[PGNUM(addr)] & PTE_U))
         duppage(envid, PGNUM(addr));
   }

   if ((r = sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), 
      PTE_P | PTE_U | PTE_W)) < 0)
      panic("fork: %e", r);

   extern void _pgfault_upcall();
   sys_env_set_pgfault_upcall(envid, _pgfault_upcall);

   if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
      panic("fork: %e", r);

   if ((r = sys_env_set_priority(envid, priority)) < 0)
      panic("fork: %e", r);

   return envid;
   panic("fork not implemented");
}

```

其中使用了新定义的系统调用`sys_env_set_priority`（需要在`inc/lib.c`，`lib/syscall.c`，`kern/syscall.c`中添加相应的代码）：

```java
// inc/lib.c
envid_t pfork(int priority);

// lib/syscall.c
int
sys_env_set_priority(envid_t envid, int priority)
{
   return syscall(SYS_env_set_priority, 0, envid, priority, 0, 0, 0);
}

// kern/syscall.c
static int
sys_env_set_priority(envid_t envid, int priority) {
   int r;
   struct Env *env;
   if ((r = envid2env(envid, &env, 1)) < 0)
      return -E_BAD_ENV;
   env->env_def_pr = priority;
   env->env_cur_pr = env->env_def_pr;
   return 0;
}

int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
   ...
   case SYS_env_set_priority:
      return sys_env_set_priority(a1, a2);
   ...
}
```

测试代码：

```java
// user/pfork.c

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
```

为了能够`make run-pfork`执行这个程序，需要在`kern/Makefrag`中添加

```
# Self defined files for challenges
KERN_BINFILES +=  user/pfork
```

执行结果：

![](http://ww2.sinaimg.cn/large/6313a6d8jw1es2yerk5y6j20rw14e7hs.jpg =600x)

#### Challenge 3

>The JOS kernel currently does not allow applications to use the x86 processor's x87 floating-point unit (FPU), MMX instructions, or Streaming SIMD Extensions (SSE). Extend the Env structure to provide a save area for the processor's floating point state, and extend the context switching code to save and restore this state properly when switching from one environment to another. The FXSAVE and FXRSTOR instructions may be useful, but note that these are not in the old i386 user's manual because they were introduced in more recent processors. Write a user-level test program that does something cool with floating-point.

#### Exercise 7

>Implement the system calls described above in `kern/syscall.c`. You will need to use various functions in `kern/pmap.c` and `kern/env.c`, particularly `envid2env()`. For now, whenever you call `envid2env()`, pass `1` in the checkperm parameter. Be sure you check for any invalid system call arguments, returning `-E_INVAL` in that case. Test your JOS kernel with `user/dumbfork` and make sure it works before proceeding.

照着lab的说明和注释依次实现即可，没什么难度：

`sys_exofork`：

```java
static envid_t
sys_exofork(void)
{
   // LAB 4: Your code here.
   struct Env *newenv;
   int errorcode;
   if ((errorcode=env_alloc(&newenv, curenv->env_id)) < 0)
      return errorcode;
   newenv->env_status = ENV_NOT_RUNNABLE;
   newenv->env_tf = curenv->env_tf;
   newenv->env_tf.tf_regs.reg_eax = 0;
   return newenv->env_id;
}
```

`sys_env_set_status`：

```java
static int
sys_env_set_status(envid_t envid, int status)
{
   // LAB 4: Your code here.
   struct Env *env;
   switch (status) {
      case ENV_NOT_RUNNABLE:
      case ENV_RUNNABLE:
         if (envid2env(envid, &env, 1) < 0)
            return -E_BAD_ENV;
         env->env_status = status;
         break;
      default:
         return -E_INVAL;
   }
   return 0;
}
```

`sys_page_alloc`，权限检查比较繁琐，不过按照注释一步一步做也没有什么难度：

```java
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
   // LAB 4: Your code here.
   struct PageInfo *page;
   struct Env *env;

   if (envid2env(envid, &env, 1) < 0)
      return -E_BAD_ENV;

   if (va >= (void *) UTOP || va != ROUNDDOWN(va,PGSIZE))
      return -E_INVAL;

   int flag = PTE_U | PTE_P;

   if ((perm & flag) != flag)
      return -E_INVAL;

   if (perm & (~(PTE_U | PTE_P | PTE_AVAIL | PTE_W)))
      return -E_INVAL;

   if (!(page=page_alloc(1)))
      return -E_NO_MEM;

   page->pp_ref++;

   if (page_insert(env->env_pgdir, page, va, perm) < 0) {
      page_free(page);
      return -E_NO_MEM;
   }

   return 0;
}
```

`sys_page_map`：

```java
static int
sys_page_map(envid_t srcenvid, void *srcva,
        envid_t dstenvid, void *dstva, int perm)
{
   // LAB 4: Your code here.
   struct Env *srcenv, *dstenv;
   struct PageInfo *page;
   pte_t *pte;

   if (envid2env(srcenvid, &srcenv, 1) < 0)
      return -E_BAD_ENV;

   if (envid2env(dstenvid, &dstenv, 1) < 0)
      return -E_BAD_ENV;

   if (srcva >= (void *) UTOP || srcva != ROUNDDOWN(srcva, PGSIZE) || 
      dstva >= (void *) UTOP || dstva != ROUNDDOWN(dstva, PGSIZE))
      return -E_INVAL;

   if ((page=page_lookup(srcenv->env_pgdir, srcva, &pte)) ==  NULL)
      return -E_INVAL;

   if ((!(perm & (PTE_U | PTE_P))) || (perm & (~(PTE_U | PTE_P | PTE_AVAIL | PTE_W))))
      return -E_INVAL;

   if ((perm & PTE_W) && !(*pte & PTE_W))
      return -E_INVAL;

   if (page_insert(dstenv->env_pgdir, page, dstva, perm) < 0)
      return -E_NO_MEM;

   return 0;
}
```

`sys_page_unmap`：

```java
static int
sys_page_unmap(envid_t envid, void *va)
{
   // LAB 4: Your code here.
   struct Env *env;

   if (envid2env(envid, &env, 1) < 0)
      return -E_BAD_ENV;

   if (va >= (void *) UTOP || va != ROUNDDOWN(va, PGSIZE))
      return -E_INVAL;

   page_remove(env->env_pgdir, va);

   return 0;
}
```

最后在`syscall`中添加相应的情况即可：

```
   ...
   case SYS_exofork:
      return sys_exofork();
   case SYS_page_alloc:
      return sys_page_alloc(a1, (void *) a2, a3);
   case SYS_page_map:
      return sys_page_map(a1, (void *) a2, a3, (void *) a4, a5);
   case SYS_page_unmap:
      return sys_page_unmap(a1, (void *) a2);
   case SYS_env_set_status:
      return sys_env_set_status(a1, a2);
   case SYS_env_set_pgfault_upcall:
      return sys_env_set_pgfault_upcall(a1, (void *) a2);
   ...
```

`user/dumbfork`运行结果：

![](http://ww4.sinaimg.cn/large/6313a6d8jw1es2ub6tr82j20ug0z6gv7.jpg =600x)

So far, so good.

#### Challenge 4

>Add the additional system calls necessary to read all of the vital state of an existing environment as well as set it up. Then implement a user mode program that forks off a child environment, runs it for a while (e.g., a few iterations of sys_yield()), then takes a complete snapshot or checkpoint of the child environment, runs the child for a while longer, and finally restores the child environment to the state it was in at the checkpoint and continues it from there. Thus, you are effectively "replaying" the execution of the child environment from an intermediate state. Make the child environment perform some interaction with the user using sys_cgetc() or readline() so that the user can view and mutate its internal state, and verify that with your checkpoint/restart you can give the child environment a case of selective amnesia, making it "forget" everything that happened beyond a certain point.

### Part B: Copy-on-Write Fork

#### Exercise 8

>Implement the `sys_env_set_pgfault_upcall` system call. Be sure to enable permission checking when looking up the environment ID of the target environment, since this is a "dangerous" system call.

这题估计是拿来熟悉`envid2env`的，非常容易，因为要检查权限，所以权限检查的标记设为`1`，此外`env`是要保存下来的，所以传入的时候要取地址`&`：

```java
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
   // LAB 4: Your code here.
   struct Env *env;
   if (envid2env(envid, &env, 1) < 0)
      return -E_BAD_ENV;

   env->env_pgfault_upcall = func;
   return 0;
}
```

#### Exercise 9

>Implement the code in `page_fault_handler` in `kern/trap.c` required to dispatch page faults to the user-mode handler. Be sure to take appropriate precautions when writing into the exception stack. (What happens if the user environment runs out of space on the exception stack?)

异常栈`UXSTACKTOP`中的内容：

```c
                    <-- UXSTACKTOP
trap-time esp
trap-time eflags
trap-time eip
trap-time eax       start of struct PushRegs
trap-time ecx
trap-time edx
trap-time ebx
trap-time esp
trap-time ebp
trap-time esi
trap-time edi       end of struct PushRegs
tf_err (error code)
fault_va            <-- %esp when handler is run
```

根据上面栈中所涉及的内容扔进`utf`结构即可。最后将`curenv->env_pgfault_upcall`赋给`curenv->env_tf.tf_eip`以从设置好的pgfault upcall的位置开始执行。

```java
void
page_fault_handler(struct Trapframe *tf)
{
   uint32_t fault_va;

   // Read processor's CR2 register to find the faulting address
   fault_va = rcr2();

   // Handle kernel-mode page faults.

   // LAB 3: Your code here.
   if (tf->tf_cs == GD_KT) {
      // Trapped from kernel mode
      panic("page_fault_handler: page fault in kernel mode");
   }

   // LAB 4: Your code here.

   if (curenv->env_pgfault_upcall) {
      struct UTrapframe *utf;
      uintptr_t utf_addr;
      if (UXSTACKTOP-PGSIZE <= tf->tf_esp && tf->tf_esp <= UXSTACKTOP-1)
         utf_addr = tf->tf_esp - sizeof(struct UTrapframe) - 4;
      else
         utf_addr = UXSTACKTOP - sizeof(struct UTrapframe);
      
      user_mem_assert(curenv, (void *) utf_addr, sizeof(struct UTrapframe), PTE_W);

      utf = (struct UTrapframe *) utf_addr;
      utf->utf_fault_va = fault_va;
      utf->utf_err = tf->tf_err;
      utf->utf_regs = tf->tf_regs;
      utf->utf_eip = tf->tf_eip;
      utf->utf_eflags = tf->tf_eflags;
      utf->utf_esp = tf->tf_esp;

      curenv->env_tf.tf_eip = (uintptr_t) curenv->env_pgfault_upcall;
      curenv->env_tf.tf_esp = utf_addr;
      env_run(curenv);
   }

   // Destroy the environment that caused the fault.
   cprintf("[%08x] user fault va %08x ip %08x\n",
      curenv->env_id, fault_va, tf->tf_eip);
   print_trapframe(tf);
   env_destroy(curenv);
}

```

#### Exercise 10

>Implement the _pgfault_upcall routine in lib/pfentry.S. The interesting part is returning to the original point in the user code that caused the page fault. You'll return directly there, without going back through the kernel. The hard part is simultaneously switching stacks and re-loading the EIP.

根据注释一点一点填完，其中`subl $0x4, 0x30(%esp)`是因为使用`popal`后不能使用`general-purpose`的寄存器，而在`popfl`后甚至不能进行算术运算了，所以就直接在最开始在栈中做好。

```asm
.text
.globl _pgfault_upcall
_pgfault_upcall:
   // Call the C page fault handler.
   pushl %esp        // function argument: pointer to UTF
   movl _pgfault_handler, %eax
   call *%eax
   addl $4, %esp        // pop function argument
   
   // LAB 4: Your code here.
   movl 0x28(%esp), %eax
   subl $0x4, 0x30(%esp)
   movl 0x30(%esp), %edx
   movl %eax, (%edx)
   addl $0x8, %esp

   // Restore the trap-time registers.  After you do this, you
   // can no longer modify any general-purpose registers.
   // LAB 4: Your code here.
   popal

   // Restore eflags from the stack.  After you do this, you can
   // no longer use arithmetic operations or anything else that
   // modifies eflags.
   // LAB 4: Your code here.
   addl $0x4, %esp
   popfl

   // Switch back to the adjusted trap-time stack.
   // LAB 4: Your code here.
   popl %esp

   // Return to re-execute the instruction that faulted.
   // LAB 4: Your code here.
   ret
```

#### Exercise 11

>Finish `set_pgfault_handler()` in `lib/pgfault.c`.

第一次执行该函数时需要声明一段异常栈空间（`_pgfault_upcall`是定义在`pfentry.S`中的一个全局变量，如果发现它还没有被赋值则代表是第一次执行），然后只用将传入的参数`handler`赋给`_pgfault_handler`，再调用系统调用`sys_env_set_pgfault_upcall`设置pgfault handler。

```java
void
set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
{
   int r;

   if (_pgfault_handler == 0) {
      // First time through!
      // LAB 4: Your code here.     
      if ((r = sys_page_alloc(0, (void *) (UXSTACKTOP-PGSIZE), 
            PTE_W | PTE_U | PTE_P)) < 0)
         panic("set_pgfault_handler: %e\n", r);
   }

   // Save handler pointer for assembly to call.
   _pgfault_handler = handler;
   if ((r = sys_env_set_pgfault_upcall(0, _pgfault_upcall)) < 0)
      panic("set_pgfault_handler: %e\n", r);
}
```

在这里执行`user/faultalloc`错了，结果发现是测试的时候加入的`cprintf`没有换行，导致评分脚本对输出进行匹配时匹配不上。。删掉多余的`cprintf`就完美解决了这个问题。

#### Challenge 5

>Extend your kernel so that not only page faults, but all types of processor exceptions that code running in user space can generate, can be redirected to a user-mode exception handler. Write user-mode test programs to test user-mode handling of various exceptions such as divide-by-zero, general protection fault, and illegal opcode.

#### Exercise 12

>Implement `fork`, `duppage` and `pgfault` in `lib/fork.c`.

按着`fork.c`中代码从上到下的顺序依次实现。

先是一个`fork`过程自定义的名为`pgfault`的page fault handler。先根据lab的提示，对错误码`err`检查，确定这个页错误来自于写操作，然后对引发页错误的地址本身是否存在映射以及是否是一个Copy-on-Write页进行判断。通过检查后需要先将引发错误的地址`addr`按页对齐，之后分配一个临时的页，并将`addr`中的内容拷贝进去，然后再将那个新的页映射到`addr`即可。具体实现如下：

```java
static void
pgfault(struct UTrapframe *utf)
{
   void *addr = (void *) utf->utf_fault_va;
   uint32_t err = utf->utf_err;
   int r;

   // LAB 4: Your code here.

   if (!(
         (err & FEC_WR) && (uvpd[PDX(addr)] & PTE_P) &&
         (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_COW)
      ))
      panic("pgfault: faulting access is either not a write or not to a COW page");

   // LAB 4: Your code here.

   addr = ROUNDDOWN(addr, PGSIZE);
   if ((r = sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W)) < 0)
      panic("pgfault: %e", r);
   memcpy(PFTEMP, addr, PGSIZE);
   if ((r = sys_page_map(0, PFTEMP, 0, addr, PTE_P | PTE_U | PTE_W)) < 0)
      panic("pgfault: %e", r);
   if ((r = sys_page_unmap(0, PFTEMP)) < 0)
      panic("pgfault: %e", r);

   return;

   panic("pgfault not implemented");
}
```

`duppage`：如果目标是只读的，则普通地映射一下就行了；否则需要先将当前环境对应的页映射到目标环境并标记为COW，然后将当前环境这个页本身标记为COW（因为这个页改变的时候也需要走Copy-on-Write那套流程，对指向它的页进行真实的拷贝）。具体实现：

```java
static int
duppage(envid_t envid, unsigned pn)
{
   int r;

   // LAB 4: Your code here.
   void *addr = (void *) (pn*PGSIZE);

   if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)) {
      if ((r = sys_page_map(0, addr, envid, addr, PTE_P | PTE_U | PTE_COW)) < 0)
         panic("duppage: %e", r);
      if ((r = sys_page_map(0, addr, 0, addr, PTE_P | PTE_U | PTE_COW)) < 0)
         panic("duppage: %e", r);
   } else if ((r = sys_page_map(0, addr, envid, addr, PTE_P | PTE_U)) < 0)m
      panic("duppage: %e", r);


   // panic("duppage not implemented");
   return 0;
}
```

`fork`：先调用`sys_exofork`产生一个子进程，对子进程设置好`thisenv`（最开始我做的时候忘了设置，结果在后面做IPC的时候`thisenv`没内容才意识到问题，赶紧回来改了）；在父进程中，将父进程的页Copy-on-Write到子进程里，然后为子进程申请好异常栈空间和页错误处理函数，并将子进程的状态设置为`ENV_RUNNABLE`。

```java
envid_t
fork(void)
{
   // LAB 4: Your code here.

   int r;
   envid_t envid;
   uintptr_t addr;

   set_pgfault_handler(pgfault);

   if ((envid = sys_exofork()) == 0) {
      // child
      thisenv = &envs[ENVX(sys_getenvid())];
      return 0;
   }

   for (addr = 0; addr < USTACKTOP; addr += PGSIZE) {
      if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P)
         && (uvpt[PGNUM(addr)] & PTE_U))
         duppage(envid, PGNUM(addr));
   }

   if ((r = sys_page_alloc(envid, (void *) (UXSTACKTOP - PGSIZE), 
      PTE_P | PTE_U | PTE_W)) < 0)
      panic("fork: %e", r);

   extern void _pgfault_upcall();
   sys_env_set_pgfault_upcall(envid, _pgfault_upcall);

   if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
      panic("fork: %e", r);


   return envid;
   panic("fork not implemented");
}
```

#### Challenge 6

>Implement a shared-memory `fork()` called `sfork()`. This version should have the parent and child share all their memory pages (so writes in one environment appear in the other) except for pages in the stack area, which should be treated in the usual copy-on-write manner. Modify user/forktree.c to use `sfork()` instead of regular `fork()`. Also, once you have finished implementing IPC in part C, use your `sfork()` to run `user/pingpongs`. You will have to find a new way to provide the functionality of the global thisenv pointer.

这个Challenge实现的关键就是把parent和child内存空间相互共享。先将parent的内存拷贝到child中（栈使用COW，异常栈相互独立），然后将其他部分直接将parent的页映射到child即可：

```java
// Challenge!
int
sfork(void)
{
   envid_t envid, thisenvid = sys_getenvid();
   int perm;
   int r;
   uint32_t i, j, pn;

   set_pgfault_handler(pgfault);

   if ((envid = sys_exofork()) == 0) {
      // child
      thisenv = &envs[ENVX(sys_getenvid())];
      return 0;
   }

   for (i = PDX(UXSTACKTOP-PGSIZE); i >= PDX(UTEXT) ; i--) {
      if (uvpd[i] & PTE_P) {
         for (j = 0; j < NPTENTRIES; j++) {
            pn = PGNUM(PGADDR(i, j, 0));
            if (pn == PGNUM(UXSTACKTOP-PGSIZE))
               break;

            if (pn == PGNUM(USTACKTOP-PGSIZE))
                duppage(envid, pn);
            else if (uvpt[pn] & PTE_P) {   
               perm = uvpt[pn] & ~(uvpt[pn] & ~(PTE_P |PTE_U | PTE_W | PTE_AVAIL));
               
               if ((r = sys_page_map(thisenvid, (void *)(PGADDR(i, j, 0)), 
                     envid, (void *)(PGADDR(i, j, 0)), perm)) < 0)
                  return r;
            }
         }
      }
   }

   if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_U | PTE_P | PTE_W)) < 0)
      return r;

   if ((r = sys_page_map(envid, (void *)(UXSTACKTOP - PGSIZE), 
      thisenvid, PFTEMP, PTE_U | PTE_P | PTE_W)) < 0)
      return r;

   memmove((void *)(UXSTACKTOP - PGSIZE), PFTEMP, PGSIZE);

   if ((r = sys_page_unmap(thisenvid, PFTEMP)) < 0)
      return r;

   extern void _pgfault_upcall(void);
   sys_env_set_pgfault_upcall(envid, _pgfault_upcall);

   if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
      return r;

   return envid;
}
```

测试代码：

```java
// user/sfork.c

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
```

为了能够`make run-sfork`执行这段代码，需要在`kern/Makefrag`中加入

```
KERN_BINFILES +=  user/sfork
```

测试结果：

![](http://ww2.sinaimg.cn/large/6313a6d8jw1es303z6ltxj20x20mmgu0.jpg =600x)

刚开始时`shared_val`为0，child将其改为`1000`，父亲处输出了`parent: 1000`，然后父亲将其加1，child输出`child: 1001`并将其改为`10000`，父亲输出了`10000`。正确。

#### Challenge 7

>Your implementation of fork makes a huge number of system calls. On the x86, switching into the kernel using interrupts has non-trivial cost. Augment the system call interface so that it is possible to send a batch of system calls at once. Then change fork to use this interface.

>How much faster is your new fork?

>You can answer this (roughly) by using analytical arguments to estimate how much of an improvement batching system calls will make to the performance of your fork: How expensive is an int 0x30 instruction? How many times do you execute int 0x30 in your fork? Is accessing the TSS stack switch also expensive? And so on...

>Alternatively, you can boot your kernel on real hardware and really benchmark your code. See the RDTSC (read time-stamp counter) instruction, defined in the IA32 manual, which counts the number of clock cycles that have elapsed since the last processor reset. QEMU doesn't emulate this instruction faithfully (it can either count the number of virtual instructions executed or use the host TSC, neither of which reflects the number of cycles a real CPU would require).

### Part C: Preemptive Multitasking and Inter-Process communication (IPC)

#### Exercise 13

>Modify `kern/trapentry.S` and `kern/trap.c` to initialize the appropriate entries in the IDT and provide handlers for IRQs `0` through `15`. Then modify the code in `env_alloc()` in `kern/env.c` to ensure that user environments are always run with interrupts enabled.

在前一次lab challenge1的基础上改就非常方便了，

先在`kern/trapentry.S`中加上IRQ对应的语句：

```asm
.data
   .space 48# (31-19)*4
.text
   TRAPHANDLER_NOEC(handler32, IRQ_OFFSET);
   TRAPHANDLER_NOEC(handler33, IRQ_OFFSET+1);
   TRAPHANDLER_NOEC(handler34, IRQ_OFFSET+2);
   TRAPHANDLER_NOEC(handler35, IRQ_OFFSET+3);
   TRAPHANDLER_NOEC(handler36, IRQ_OFFSET+4);
   TRAPHANDLER_NOEC(handler37, IRQ_OFFSET+5);
   TRAPHANDLER_NOEC(handler38, IRQ_OFFSET+6);
   TRAPHANDLER_NOEC(handler39, IRQ_OFFSET+7);
   TRAPHANDLER_NOEC(handler40, IRQ_OFFSET+8);
   TRAPHANDLER_NOEC(handler41, IRQ_OFFSET+9);
   TRAPHANDLER_NOEC(handler42, IRQ_OFFSET+10);
   TRAPHANDLER_NOEC(handler43, IRQ_OFFSET+11);
   TRAPHANDLER_NOEC(handler44, IRQ_OFFSET+12);
   TRAPHANDLER_NOEC(handler45, IRQ_OFFSET+13);
   TRAPHANDLER_NOEC(handler46, IRQ_OFFSET+14);
   TRAPHANDLER_NOEC(handler47, IRQ_OFFSET+15);
   TRAPHANDLER_NOEC(handler48, T_SYSCALL);
```

再在`kern/trap.c`的`trap_init`函数中进行好设置即可：

```java
   ...
   for (i = 0; i < 16; i++)
      SETGATE(idt[IRQ_OFFSET+i], 0, GD_KT, handler[IRQ_OFFSET+i], 0);
   ...
```

此外由于JOS在内核态中强制关闭了设备中断，进入用户态时需要将其开启：在`env.c`的`env_alloc`里加入

```c
   e->env_tf.tf_eflags |= FL_IF;
```

#### Exercise 14

>Modify the kernel's `trap_dispatch()` function so that it calls `sched_yield()` to find and run a different environment whenever a clock interrupt takes place.

>You should now be able to get the `user/spin` test to work: the parent environment should fork off the child, `sys_yield()` to it a couple times but in each case regain control of the CPU after one time slice, and finally kill the child environment and terminate gracefully.

实现抢占，让时钟计时到的环境yield执行权限：在`trap.c`的`trap_dispatch`中加入

```java
   if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
      lapic_eoi();
      sched_yield();
      return;
   }
```

![](http://ww1.sinaimg.cn/large/6313a6d8jw1es2vucj2rvj20xo0mun6j.jpg =600x)

父亲干掉儿子之后自己优雅地结束了！

#### Exercise 15

>Implement `sys_ipc_recv` and `sys_ipc_try_send` in `kern/syscall.c`. Read the comments on both before implementing them, since they have to work together. When you call envid2env in these routines, you should set the checkperm flag to 0, meaning that any environment is allowed to send IPC messages to any other environment, and the kernel does no special permission checking other than verifying that the target envid is valid.

>Then implement the `ipc_recv` and `ipc_send` functions in `lib/ipc.c`.

>Use the `user/pingpong` and `user/primes` functions to test your IPC mechanism. user/primes will generate for each prime number a new environment until JOS runs out of environments. You might find it interesting to read `user/primes.c` to see all the forking and IPC going on behind the scenes.

`sys_ipc_try_send`没什么难点，注意按着注释一步一步检查各种东西就不会有问题：

```java
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
   // LAB 4: Your code here.
   // panic("sys_ipc_try_send not implemented");
   struct Env *env;
   int r;
   if ((r = envid2env(envid, &env, 0)) < 0)
      return -E_BAD_ENV;
   if (env->env_ipc_recving == 0)
      return -E_IPC_NOT_RECV;
   if (srcva < (void *) UTOP) {

      if (srcva != ROUNDDOWN(srcva, PGSIZE))
         return -E_INVAL;

      pte_t *pte;
      struct PageInfo *page = page_lookup(curenv->env_pgdir, srcva, &pte);
      if (!page)
         return -E_INVAL;

      if ((*pte & perm) != perm) return -E_INVAL;

      if ((perm & PTE_W) && !(*pte & PTE_W))
         return -E_INVAL;

      if (env->env_ipc_dstva < (void *) UTOP) {
         if ((r = page_insert(env->env_pgdir, page, env->env_ipc_dstva, perm)) < 0)
            return -E_NO_MEM;
         env->env_ipc_perm = perm;
      }
   }
   env->env_ipc_recving = 0;
   env->env_ipc_from = curenv->env_id;
   env->env_ipc_value = value;
   env->env_status = ENV_RUNNABLE;
   env->env_tf.tf_regs.reg_eax = 0;
   return 0;
}
```

`sys_ipc_recv`：同上

```java
static int
sys_ipc_recv(void *dstva)
{
   // LAB 4: Your code here.

   if (dstva < (void *) UTOP && dstva != ROUNDDOWN(dstva, PGSIZE))
      return -E_INVAL;

   curenv->env_ipc_recving = 1;
   curenv->env_ipc_dstva = dstva;
   curenv->env_status = ENV_NOT_RUNNABLE;
   sys_yield();

   return 0;
}
```

`ipc_recv`，值得注意的是表示`no page`的地址的设计。考虑到前面的代码都是由`va < UTOP`来判断的，为了不更改之前的代码，就让`0xffffffff`，即`(void *)-1`，表示`no page`了。

```java
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
   // LAB 4: Your code here.

   int r;

   if (from_env_store)
      *from_env_store = 0;

   if (perm_store)
      *perm_store = 0;

   if (!pg)
      pg = (void *) -1;

   if ((r = sys_ipc_recv(pg)) < 0) {
      cprintf("im dead");
      return r;
   }

   if (from_env_store)
      *from_env_store = thisenv->env_ipc_from;

   if (perm_store)
      *perm_store = thisenv->env_ipc_perm;
   
   return thisenv->env_ipc_value;
}
```

`ipc_send`：同上

```java
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
   // LAB 4: Your code here.
   int r;

   if (!pg)
      pg = (void *) -1;

   while ((r = sys_ipc_try_send(to_env, val, pg, perm)) < 0) {
      if (r != -E_IPC_NOT_RECV)
         panic("ipc_send: %e", r);
      sys_yield();
   }
}
```

#### Challenge 8

>Why does ipc_send have to loop? Change the system call interface so it doesn't have to. Make sure you can handle multiple environments trying to send to one environment at the same time.

#### Challenge 9

>The prime sieve is only one neat use of message passing between a large number of concurrent programs. Read C. A. R. Hoare, ``Communicating Sequential Processes,'' Communications of the ACM 21(8) (August 1978), 666-667, and implement the matrix multiplication example.

#### Challenge 10

>One of the most impressive examples of the power of message passing is Doug McIlroy's power series calculator, described in M. Douglas McIlroy, ``Squinting at Power Series,'' Software--Practice and Experience, 20(7) (July 1990), 661-683. Implement his power series calculator and compute the power series for sin(x+x^3).

#### Challenge 11

>Make JOS's IPC mechanism more efficient by applying some of the techniques from Liedtke's paper, Improving IPC by Kernel Design, or any other tricks you may think of. Feel free to modify the kernel's system call API for this purpose, as long as your code is backwards compatible with what our grading scripts expect.

## 遇到的困难以及解决方法

遇到的困难主要在两个方面，一个是忘记在`kern/syscall.c`的`syscall`函数中加入对应的系统调用，导致一直通过不了测试；最后回想起做前面的lab时的同样的经历，就解决了`_(:з」∠)_`。

另一个是在完成**Exercise 15**后，死活通过不了`make grade`。为了锁定问题在哪儿我反复检查了lab 4所有相关的代码，但是还是没有找到问题。最后发现原来问题出在lab 3的`env_run`函数中，在那里我忘了检查`curenv`的状态（即不处于`ENV_RUNNABLE`状态的环境也会被改写为`ENV_RUNNING`状态）。这里在lab 3中没有出问题是因为只有在多CPU的情况才容易因此发生问题，而在lab 4实现IPC过程中，如果一个进程等待接收通信，进入block状态，在这里就会马上便会`ENV_RUNNING`；那个进程会以为成功接收了消息而继续执行然后结束。此时发送方在`envid2env`发现目标`env`并不存在，就出错了。。修正了`env_run`里的疏漏后，问题也随之而解。

## 收获及感想

这次lab跟之前不同的地方在于，尽管注释提示也十分充足，但是真正实现起来还是有各种容易疏忽的问题，特别是多CPU的情况下，调试起来相对难以锁定位置。在这个调试过程中，反复阅读代码，对JOS的结构的理解也进一步得到了加深。