# JOS Lab 3 Report<br/><small><small><small><small>江天源 1100016614</small></small></small></small>

## 总体概述


## 完成情况

|#|E1|E2|E3|E4&C1|E5|E6|C2|E7|C3|E8|E9|E10|
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|Status|√|√|√|√|√|√|√|√|x|√|√|√|

|#|Q1|Q2|Q3|Q4|
|:-:|:-:|:-:|:-:|:-:|
|Status|√|√|√|√|

<small>* 其中E#代表Exercise #, Q#代表Question #, C# 代表Challenge #</small>

`make grade`结果

![](http://ww3.sinaimg.cn/large/6313a6d8jw1er61jhpdfij20xk0mugs3.jpg =600x)

### Part A: User Environments and Exception Handling

#### Exercise 1

>Modify `mem_init()` in `kern/pmap.c` to allocate and map the `envs` array. This array consists of exactly `NENV` instances of the `Env` structure allocated much like how you allocated the `pages` array. Also like the `pages` array, the memory backing `envs` should also be mapped user read-only at `UENVS` (defined in `inc/memlayout.h`) so user processes can read from this array.

对envs数组模仿着Lab 2中的做法声明空间并映射到虚存即可。

```c
envs = (struct Env *) boot_alloc(NENV * sizeof(struct Env));
...
boot_map_region(kern_pgdir, UPAGES, PTSIZE, PADDR(pages), PTE_U);
```

#### Exercise 2

>In the file env.c, finish coding the following functions:
>
   env_init()
   env_setup_vm()
   region_alloc()
   load_icode()
   env_create()
   env_run()


**`env_init()`** 注释里说明了要求`env_free_list`中的顺序与`envs`数组的顺序一致，因此需要倒着将`envs`中的元素插入`env_free_list`：

```c
void
env_init(void)
{
   // Set up envs array
   // LAB 3: Your code here.
   int i;
   for (i = NENV-1; i >= 0; i--) {
      envs[i].env_status = ENV_FREE;
      envs[i].env_id = 0;
      envs[i].env_link = env_free_list;
      env_free_list = envs + i;
   }

   // Per-CPU part of the initialization
   env_init_percpu();
}
```

**`env_setup_vm()`** 将已经申请好的页分配给`env_pgdir`，再将`kern_pgdir`的内容直接拷贝给它既可，由于用户环境的页是动态映射过来的，需要增加pp_ref计数：

```c
static int
env_setup_vm(struct Env *e)
{
   int i;
   struct PageInfo *p = NULL;

   // Allocate a page for the page directory
   if (!(p = page_alloc(ALLOC_ZERO)))
      return -E_NO_MEM;

   // LAB 3: Your code here.
   e->env_pgdir = (pde_t *) page2kva(p);
   p->pp_ref++;

   memcpy(e->env_pgdir, kern_pgdir, PGSIZE);

   // UVPT maps the env's own page table read-only.
   // Permissions: kernel R, user R
   e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_P | PTE_U;

   return 0;
}
```

**`region_alloc()`** 为了方便使用，`va`和`va + len`不要求按页对齐，所以进来之后需要将`va` ROUNDDOWN，将`va + len` ROUNDUP；之后再在范围内逐页分配即可：


```c
static void
region_alloc(struct Env *e, void *va, size_t len)
{
   // LAB 3: Your code here.
   // (But only if you need it for load_icode.)
   //
   // Hint: It is easier to use region_alloc if the caller can pass
   //   'va' and 'len' values that are not page-aligned.
   //   You should round va down, and round (va + len) up.
   //   (Watch out for corner-cases!)
   void *vas, *vat;

   vas = ROUNDDOWN(va, PGSIZE);
   vat = ROUNDUP(va + len, PGSIZE);

   for (; vas < vat; vas += PGSIZE) {
      struct PageInfo *pp = page_alloc(0);
      if (pp == NULL)
         panic("region_alloc: allocation failed.");
      page_insert(e->env_pgdir, pp, vas, PTE_U | PTE_W);
   }

}
```

**`load_icode()`** 仿照`boot/main.c`中读取kernel的部分写即可；其中需要即将读取的空间清零，再读入数据；过程中应该使用对应环境的页目录，并在之后恢复；再设置好新环境的`eip`执行文件的入口`e_entry`，最后为新环境分配好初始栈即可：

```c
static void
load_icode(struct Env *e, uint8_t *binary)
{
   // LAB 3: Your code here.
   struct Elf *ELFHDR;
   struct Proghdr *ph, *eph;

   ELFHDR = (struct Elf *) binary;

   if (ELFHDR->e_magic != ELF_MAGIC)
      panic("load_icode: not ELF executable.");

   ph = (struct Proghdr *) (binary + ELFHDR->e_phoff);
   eph = ph + ELFHDR->e_phnum;

   lcr3(PADDR(e->env_pgdir));

   for (; ph < eph; ph++) {
      if (ph->p_type == ELF_PROG_LOAD) {
         region_alloc(e, (void *) ph->p_va, ph->p_memsz);
         memset((void *) ph->p_va, 0, ph->p_memsz);
         memcpy((void *) ph->p_va, binary + ph->p_offset, ph->p_filesz);
      }
   }

   lcr3(PADDR(kern_pgdir));

   e->env_tf.tf_eip = ELFHDR->e_entry;

   // Now map one page for the program's initial stack
   // at virtual address USTACKTOP - PGSIZE.

   // LAB 3: Your code here.
   region_alloc(e, (void *) (USTACKTOP - PGSIZE), PGSIZE);
}
```

**`env_create()`** 调用`load_icode`读取`binary`，并设置好`env_type`就行了：

```c
void
env_create(uint8_t *binary, enum EnvType type)
{
   // LAB 3: Your code here.
   struct Env *e;
   int errorcode;

   if ((errorcode=env_alloc(&e, 0)) < 0)
      panic("env_create: %e", errorcode);

   load_icode(e, binary);

   e->env_type = type;
}
```

**`env_run()`** 改变当前环境的状态为`ENV_RUNNABLE`，并设置将要执行的环境的状态为正在运行`ENV_RUNNING`；之后再将页目录设置为新环境的页目录，并将用户环境的`TrapFrame`恢复即可：

```c
void
env_run(struct Env *e)
{
   // LAB 3: Your code here.

   if (curenv)
      curenv->env_status = ENV_RUNNABLE;
   curenv = e;
   e->env_status = ENV_RUNNING;
   e->env_runs++;
   lcr3(PADDR(e->env_pgdir));
   env_pop_tf(&e->env_tf);
}

```

#### Exercise 3

>Read [Chapter 9, Exceptions and Interrupts](http://pdosnew.csail.mit.edu/6.828/2014/readings/i386/c09.htm) in the [80386 Programmer's Manual](http://pdosnew.csail.mit.edu/6.828/2014/readings/i386/toc.htm) (or Chapter 5 of the [IA-32 Developer's Manual](http://pdosnew.csail.mit.edu/6.828/2014/readings/ia32/IA32-3A.pdf)), if you haven't already.

![](http://ww4.sinaimg.cn/large/6313a6d8gw1er4xrqw3cnj20ue108qgt.jpg =600x)

![](http://ww3.sinaimg.cn/large/6313a6d8gw1er4xssvikcj20ue0g60zq.jpg =600x)

#### Exercise 4 & Challenge 1

>**Exercise 4**. Edit `trapentry.S` and `trap.c` and implement the features described above. 

>Your `_alltraps` should:
>
- push values to make the stack look like a `struct Trapframe`
- load `GD_KD` into `%ds` and `%es`
- `pushl %esp` to pass a pointer to the `Trapframe` as an argument to `trap()`
- call `trap` (can trap ever return?)
- Consider using the `pushal` instruction; it fits nicely with the layout of the `struct Trapframe`.

>Test your trap handling code using some of the test programs in the user directory that cause exceptions before making any system calls, such as user/divzero. You should be able to get make grade to succeed on the divzero, softint, and badsegment tests at this point.

>**Challenge**. You probably have a lot of very similar code right now, between the lists of `TRAPHANDLER` in `trapentry.S` and their installations in `trap.c`. Clean this up. Change the macros in `trapentry.S` to automatically generate a table for `trap.c` to use. Note that you can switch between laying down code and data in the assembler by using the directives `.text` and `.data`.

看了Challenge 1发现是要改变Exercise 4里面的写法，所以就直接按照Challenge要求的做法做了。

先把`_alltraps`实现了：

> - push values to make the stack look like a `struct Trapframe`
- load `GD_KD` into `%ds` and `%es`
- `pushl %esp` to pass a pointer to the `Trapframe` as an argument to `trap()`
- call `trap` (can trap ever return?)


在`inc/trap.h`中：

```c
struct Trapframe {
   struct PushRegs tf_regs;
   uint16_t tf_es;
   uint16_t tf_padding1;
   uint16_t tf_ds;
   uint16_t tf_padding2;
   uint32_t tf_trapno;
   /* below here defined by x86 hardware */
   uint32_t tf_err;
   uintptr_t tf_eip;
   uint16_t tf_cs;
   uint16_t tf_padding3;
   uint32_t tf_eflags;
   /* below here only when crossing rings, such as from user to kernel */
   uintptr_t tf_esp;
   uint16_t tf_ss;
   uint16_t tf_padding4;
} __attribute__((packed));

```

从最下到`tf_err`都是有硬件自动完成，`trapno`在`TRAPHANDLER(_NOEC)`中推入栈中，这里需要压入的就是`tf_trapno`之上的几个参数；剩下的按照题目写就好了：

```asm
pushl %ds
pushl %es
pushal
movw $GD_KD, %ax
movw %ax, %ds
movw $GD_KD, %ax
movw %ax, %es
pushl %esp
call trap
```

之后开始初始化trap。为了在`trap_init()`中能循环调用SETGATE，需要将这些trap的handler放到一个全局的数组里；为了做到这一点需要将每一个handler的名称在`.data`中声明。修改后的`TRAPHANDLER`如下(`TRAPHANDLER_NOEC`类似)：

```asm
#define TRAPHANDLER(name, num)                  \
.data;\
   .long name;\
.text;\
   .globl name;      /* define global symbol for 'name' */  \
   .type name, @function;  /* symbol type is function */    \
   .align 2;      /* align function definition */     \
   name:       /* function starts here */    \
   pushl $(num);                    \
   jmp _alltraps
```

根据Exercise 3中读到的各个Trap的使用情况，在`kern/trapentry.S`中声明handler数组：

```asm
.data
   .globl handler
   .align 4
handler:
.text
/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
   TRAPHANDLER_NOEC(handler0, T_DIVIDE);
   TRAPHANDLER_NOEC(handler1, T_DEBUG);
   TRAPHANDLER_NOEC(handler2, T_NMI);
   TRAPHANDLER_NOEC(handler3, T_BRKPT);
   TRAPHANDLER_NOEC(handler4, T_OFLOW);
   TRAPHANDLER_NOEC(handler5, T_BOUND);
   TRAPHANDLER_NOEC(handler6, T_ILLOP);
   TRAPHANDLER_NOEC(handler7, T_DEVICE);
   TRAPHANDLER(handler8, T_DBLFLT);
   # TRAPHANDLER(handler9, 9);
   dummy();
   TRAPHANDLER(handler10, T_TSS);
   TRAPHANDLER(handler11, T_SEGNP);
   TRAPHANDLER(handler12, T_STACK);
   TRAPHANDLER(handler13, T_GPFLT);
   TRAPHANDLER(handler14, T_PGFLT);
   # TRAPHANDLER(handler15, 15);
   dummy();
   TRAPHANDLER_NOEC(handler16, T_FPERR);
   TRAPHANDLER_NOEC(handler17, T_ALIGN);
   TRAPHANDLER_NOEC(handler18, T_MCHK);
   TRAPHANDLER_NOEC(handler19, T_SIMDERR);
```

其中`dummy()`用于占位，否则数组中元素对应的trapno会错位。`dummy`定义如下：

```asm
#define dummy()\
.data;\
   .long 0
```

最后完成`kern/trap.c`中`trap_init`即可，其中3号Trap需要在用户态触发所以dpl为3：

```c
void
trap_init(void)
{
   extern struct Segdesc gdt[];

   // LAB 3: Your code here.
   extern void (*handler[])();
   int dpl;
   
   int i;
   for (i = 0; i <= 19; i++) {
      if (i != 9 && i != 15) {
         dpl = 0;
         if (i == T_BRKPT)
            dpl = 3;
         SETGATE(idt[i], 0, GD_KT, handler[i], dpl);
      }
   }

   // Per-CPU setup 
   trap_init_percpu();
}

```


#### Question 1

>What is the purpose of having an individual handler function for each exception/interrupt? (i.e., if all exceptions/interrupts were delivered to the same handler, what feature that exists in the current implementation could not be provided?)

分别使用handler是为了能够方便快捷地直接为对应的异常/中断找到处理函数的入口，如果使用统一的handler处理所有的中断，需要添加参数来表示这是哪一个中断，而且对于errorcode需要进行判断才能决定是否要压入一个0来做padding还是系统已经自动地压入了。

#### Question 2

>Did you have to do anything to make the user/softint program behave correctly? The grade script expects it to produce a general protection fault (trap 13), but softint's code says `int $14`. Why should this produce interrupt vector 13? What happens if the kernel actually allows softint's `int $14` instruction to invoke the kernel's page fault handler (which is interrupt vector 14)?

用户态没有权限直接触发page fault，因此产生general protection fault是正常的现象，无需进行修改。如果kernel允许用户`int $14`的话可能就会有错误的或者有恶意的程序疯狂地申请新的页，耗尽内存资源。

### Part B: Page Faults, Breakpoints Exceptions, and System Calls

#### Exercise 5

>Modify `trap_dispatch()` to dispatch page fault exceptions to `page_fault_handler()`. You should now be able to get make grade to succeed on the `faultread`, `faultreadkernel`, `faultwrite`, and `faultwritekernel` tests. If any of them don't work, figure out why and fix them. Remember that you can boot JOS into a particular user program using `make run-x` or `make run-x-nox`.

修改`trap_dispatch()`，直接按题目意思写就过了：

```c
if (tf->tf_trapno == T_PGFLT) {
   page_fault_handler(tf);
   return;
}

```

#### Exercise 6

>Modify `trap_dispatch()` to make breakpoint exceptions invoke the kernel monitor. You should now be able to get `make grade` to succeed on the breakpoint test.

在`trap_dispatch()`中添加对breakpoint事件的处理，到达breakpoint后暂停当前用户程序进入monitor：

```
if (tf->tf_trapno == T_BRKPT) {
   monitor(tf);
   return;
}
```

#### Challenge 2

>Modify the JOS kernel monitor so that you can 'continue' execution from the current location (e.g., after the `int3`, if the kernel monitor was invoked via the breakpoint exception), and so that you can single-step one instruction at a time. You will need to understand certain bits of the `EFLAGS` register in order to implement single-stepping.

>Optional: If you're feeling really adventurous, find some x86 disassembler source code - e.g., by ripping it out of QEMU, or out of GNU binutils, or just write it yourself - and extend the JOS kernel monitor to be able to disassemble and display instructions as you are stepping through them. Combined with the symbol table loading from lab 2, this is the stuff of which real kernel debuggers are made.

`EFLAGS`的每一位具体含义在`inc/mmu.h`中定义，其中`FL_TF`为单步中断的标记。

开启单步：

```c
int
jdb_si(int argc, char **argv, struct Trapframe *tf) {
   if (tf == NULL || !(tf->tf_trapno == T_BRKPT || tf->tf_trapno == T_DEBUG))
      return -1;

   tf->tf_eflags |= FL_TF;

   return -1;
}
```

关闭单步：

```c
int jdb_con(int argc, char **argv, struct Trapframe *tf) {
   if (tf == NULL || !(tf->tf_trapno == T_BRKPT || tf->tf_trapno == T_DEBUG))
      return -1;

   tf->tf_eflags &= ~FL_TF;
   return -1;
}
```

把这两个函数加入`monitor.c`即可完成single-stepping和continue。

后来想要模仿一下gdb，就专门加了一个指令jdb：

```c
static struct Command commands[] = {
   ...
   { "jdb", "Run JOS debugger", mon_jdb },
};
```

```c
int
mon_jdb(int argc, char **argv, struct Trapframe *tf) {
   monitor_jdb(tf);
   return -1;
}
```

其中`monitor_jdb`是我重新写的一个monitor，会输出当前地址，并调用我重写的`runcmd_jdb`来处理指令：

```c
void
monitor_jdb(struct Trapframe *tf)
{
   char *buf;

   if (tf) 
      cprintf("=> 0x%08x\n", tf->tf_eip);

   while (1) {
      buf = readline("(jdb) ");
      if (buf != NULL)
         if (runcmd_jdb(buf, tf) < 0)
            break;
   }
}
```

```c
static int
runcmd_jdb(char *buf, struct Trapframe *tf)
{
   int argc;
   char *argv[MAXARGS];
   int i;

   // Parse the command buffer into whitespace-separated arguments
   argc = 0;
   argv[argc] = 0;
   while (1) {
      // gobble whitespace
      while (*buf && strchr(WHITESPACE, *buf))
         *buf++ = 0;
      if (*buf == 0)
         break;

      // save and scan past next arg
      if (argc == MAXARGS-1) {
         cprintf("Too many arguments (max %d)\n", MAXARGS);
         return 0;
      }
      argv[argc++] = buf;
      while (*buf && !strchr(WHITESPACE, *buf))
         buf++;
   }
   argv[argc] = 0;

   // Lookup and invoke the command
   if (argc == 0)
      return 0;
   for (i = 0; i < NCOMMANDS_DEBUG; i++) {
      if (strcmp(argv[0], commands_debug[i].name) == 0)
         return commands_debug[i].func(argc, argv, tf);
   }
   cprintf("Unknown command '%s'\n", argv[0]);
   return 0;
}
```

`commands_debug`里存放的就是刚才写的那两个single-stepping和continue的函数：

```c
static struct Command commands_debug[] = {
   { "help", "Display this list of commands", jdb_help },
   { "si", "Single Step", jdb_si },
   { "c", "Continue execution", jdb_con },
   { "quit", "Exit debugger", jdb_quit },
};
```

**测试**：执行`user/breakpoint`后单步2次，再continue

![](http://ww2.sinaimg.cn/large/6313a6d8jw1er5at1tu9tj20vc0mkq9a.jpg =600x)

#### Question 3

>The break point test case will either generate a break point exception or a general protection fault depending on how you initialized the break point entry in the `IDT` (i.e., your call to `SETGATE` from `trap_init`). Why? How do you need to set it up in order to get the breakpoint exception to work as specified above and what incorrect setup would cause it to trigger a general protection fault?

在`trap_init()`中调用`SETGATE`时将breakpoint异常特权级设置为`dpl=3`即可；未设置时用户态直接触发breakpoint会引发general protection fault。

#### Question 4

>What do you think is the point of these mechanisms, particularly in light of what the `user/softint` test program does?

这种机制一是可以将用户(可)触发的中断与只有内核可以触发的中断区分开来，对于只有可以允许触发的中断可以以比较容易的方式直接进行操作，而不用再在里面检查权限；二是可以避免用户用某些方式破坏kernel或者窃取其他进程的私有数据。

#### Exercise 7

>Add a handler in the kernel for interrupt vector `T_SYSCALL`. You will have to edit `kern/trapentry.S` and `kern/trap.c`'s `trap_init()`. You also need to change `trap_dispatch()` to handle the system call interrupt by calling `syscall()` (defined in `kern/syscall.c`) with the appropriate arguments, and then arranging for the return value to be passed back to the user process in `%eax`. Finally, you need to implement `syscall()` in `kern/syscall.c`. Make sure `syscall()` returns `-E_INVAL` if the system call number is invalid. You should read and understand `lib/syscall.c` (especially the inline assembly routine) in order to confirm your understanding of the system call interface. Handle all the systems calls listed in `inc/syscall.h` by invoking the corresponding kernel function for each call.

阅读了这几个文件的代码，总结一下`syscall`在JOS代码中的流程：

>1. 用户进程调用`lib/syscall.c`中定义的几个函数之一(`sys_cputs`, `sys_cgetc`, `sys_env_destroy`, `sys_getenvid`)

>2. 调用`lib/syscall.c`中的`syscall`函数，传入系统调用码和其他参数

>3. `lib/syscall.c`的`syscall`中根据传入参数执行`int %1`汇编代码，触发系统调用中断

>4. 经过中间特权级变换、栈切换、保护现场等一系列过程，陷入内核态

>5. 根据`trap_init()`中初始化设置好的IDT项，找到对应handler入口地址

>6. handler中建好Trapframe后调用`trap()`

>7. `trap()`经过一些检查后调用`trap_dispatch()`

>8. `trap_dispatch()`发现现在发生的是系统调用中断，调用`kern/syscall.c`中的`syscall`函数来进行处理

>9. 回到`trap()`，调用`env_run()`=>`env_pop_tf()`返回用户态

清楚了过程，下面来进行具体实现。

先在`kern/trapentry.S`和`kern/trap.c trap_init()`中添加相关代码：

`kern/trapentry.S`:

```asm
.data
   .space 112 # (47-19)*4
.text
   TRAPHANDLER_NOEC(handler48, T_SYSCALL);
```
其中`.data`部分是为了让syscall刚好在handler数组的第48位而设的padding

`trap_init()`初始化的循环后添加一行:

```c
SETGATE(idt[T_SYSCALL], 0, GD_KT, handler[T_SYSCALL], 3);
```

然后把比较简单的`kern/syscall.c`中的`syscall`实现：

```c
// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
   // Call the function corresponding to the 'syscallno' parameter.
   // Return any appropriate return value.
   // LAB 3: Your code here.

   switch (syscallno) {
   case SYS_cputs:
      sys_cputs((const char *)a1, (size_t)a2);
      return 0;
   case SYS_cgetc:
      return sys_cgetc();
   case SYS_getenvid:
      return (int32_t) sys_getenvid();
   case SYS_env_destroy:
      return sys_env_destroy(a1);
   default:
      return -E_NO_SYS;
   }

   panic("syscall not implemented");
}
```

之后在`trap_dispatch`中添加系统调用的相关代码：

```
if (tf->tf_trapno == T_SYSCALL) {
   
   tf->tf_regs.reg_eax = 
      syscall(tf->tf_regs.reg_eax, tf->tf_regs.reg_edx, tf->tf_regs.reg_ecx,
         tf->tf_regs.reg_ebx, tf->tf_regs.reg_edi, tf->tf_regs.reg_esi);
   
   if (tf->tf_regs.reg_eax < 0) {
      panic("trap_dispatch: %e", tf->tf_regs.reg_eax);
   }

   return;
}
```


#### Challenge 3

>Implement system calls using the `sysenter` and `sysexit` instructions instead of using `int 0x30` and `iret`.

>下略

感觉这个Challenge太复杂了，时间上有点来不及了所以没有进行尝试。

#### Exercise 8

>Add the required code to the user library, then boot your kernel. You should see `user/hello` print "hello, world" and then print "i am environment `00001000`". `user/hello` then attempts to "exit" by calling `sys_env_destroy()` (see `lib/libmain.c` and `lib/exit.c`). Since the kernel currently only supports one user environment, it should report that it has destroyed the only environment and then drop into the kernel monitor. You should be able to get make grade to succeed on the hello test.

在`lib/libmain.c libmain()`中将
```c
thisenv = 0
```

改为

```c
thisenv = &envs[ENVX(sys_getenvid())];
```

即可。其中sys_getenvid()会活动当前env的envid，ENVX会根据envid计算当前env在envs数组中的编号。

#### Exercise 9

>Change `kern/trap.c` to panic if a page fault happens in kernel mode.

```c
if (tf->tf_cs == GD_KT) {
   // Trapped from kernel mode
   panic("page_fault_handler: page fault in kernel mode");
}
```

>Read `user_mem_assert` in `kern/pmap.c` and implement `user_mem_check` in that same file.

这个问题需要一点特殊处理。最开始错了几次，最后看到qemu输出的信息后发现在[va, va+len)范围中，第一个页是从va开始判断，如果这个页错了就返回va的地址，但是后面的页都是按页对齐的。

```c
int
user_mem_check(struct Env *env, const void *va, size_t len, int perm)
{
   // LAB 3: Your code here.
   void *cur = (void *)(uintptr_t)va;
   void *top = ((void *)(uintptr_t)cur) + len;
   pte_t *ptep;

   perm |= PTE_P;

   for (; cur < top; cur = ROUNDDOWN(cur+PGSIZE, PGSIZE)) {
      if ((uint32_t) cur > ULIM) {
         user_mem_check_addr = (uintptr_t) cur;
         return -E_FAULT;
      }
      page_lookup(env->env_pgdir, cur, &ptep);
      if (!(ptep && ((*ptep & perm) == perm))) {
         user_mem_check_addr = (uintptr_t) cur;
         return -E_FAULT;
      }
   }

   return 0;
}
```

>Change `kern/syscall.c` to sanity check arguments to system calls.

`kern/syscall.c`只有`sys_cputs`函数会去访问内存空间，所以在这个函数里调用`user_mem_assert`检查权限即可：

```c
static void
sys_cputs(const char *s, size_t len)
{
   // Check that the user has permission to read memory [s, s+len).
   // Destroy the environment if not.

   // LAB 3: Your code here.
   user_mem_assert(curenv, s, len, PTE_U | PTE_W);
   
   // Print the string supplied by the user.
   cprintf("%.*s", len, s);
}

```

>Boot your kernel, running `user/buggyhello`. The environment should be destroyed, and the kernel should not panic. You should see:
>
   [00001000] user_mem_check assertion failure for va 00000001
   [00001000] free env 00001000
   Destroyed the only environment - nothing more to do!

![](http://ww4.sinaimg.cn/large/6313a6d8jw1er5gk3g6grj20xu0o8tj4.jpg =600x)

so far so good~

>Finally, change `debuginfo_eip` in `kern/kdebug.c` to call `user_mem_check` on `usd`, `stabs`, and `stabstr`. 

在`debuginfo_eip`里使用`usd`、`stabs`、`stabstr`三个指针前检查其访问空间的权限即可：

**`usd`**:

```c
if (user_mem_check(curenv, usd, sizeof(struct UserStabData), PTE_U) < 0) {
   cprintf("debuginfo_eip: invalid usd addr %08x", usd);
   return -1;
}
```

**`stabs` & `stabstr`**:

```c
if (user_mem_check(curenv, stabs, stab_end-stabs+1, PTE_U) < 0){
   cprintf("debuginfo_eip: invalid stabs addr %08x", stabs);
   return -1;
}
if (user_mem_check(curenv, stabstr, stabstr_end-stabstr+1, PTE_U) < 0) {
   cprintf("debuginfo_eip: invalid stabstr addr %08x", stabstr);
   return -1;
}
```

>If you now run `user/breakpoint`, you should be able to run backtrace from the kernel monitor and see the backtrace traverse into `lib/libmain.c` before the kernel panics with a page fault. What causes this page fault? You don't need to fix it, but you should understand why it happens.

![](http://ww2.sinaimg.cn/large/6313a6d8jw1er5gqradj0j214k0t0tlu.jpg =600x)

这样看不太出来是什么引起的这个错误，所以我改写了一下backtrace函数

![](http://ww4.sinaimg.cn/large/6313a6d8jw1er5hbo2zerj20xy0oa47u.jpg =600x)

可以看出是在`(uintptr_t *)ebp + 4`，即`0xeebfe000`处发生了Page Fault，对照`inc/memlayout.h`发现这个地址刚好超过了`USTACKTOP`，所以发生了错误。

#### Exercise 10

>Boot your kernel, running `user/evilhello`. The environment should be destroyed, and the kernel should not panic. You should see:
>
   [00000000] new env 00001000
   [00001000] user_mem_check assertion failure for va f010000c
   [00001000] free env 00001000
   
![](http://ww4.sinaimg.cn/large/6313a6d8jw1er5hjudusej20xu0oaqe2.jpg =600x)

至此Lab 3完成。

## 遇到的困难以及解决方法

遇到的困难主要在于Part A的Challenge 1，刚拿到题的以为直接在`.text`段前面声明一个数组就行了，结果一直`triple fault`，最后在吕鑫同学的指导下才指导需要为每个handler都声明一个`.data`和函数名称对应的`name`才能被前面声明数组当成其中的一个元素。

## 收获及感想

在之前的课程中虽然学习过中断，但那仅限于一个比较粗略的过程，在这次的lab中在自己阅读文档、代码并且动手编写之后才对中断的整个过程有了比较细节的了解。

## 参考文献

\[1\] [Intel 80386 Reference Manual](http://pdosnew.csail.mit.edu/6.828/2014/readings/i386/toc.htm)
\[2\] [QEMU monitor commands](http://pdosnew.csail.mit.edu/6.828/2014/labguide.html#qemu) 
\[3\] [Intel® 64 and IA-32 Architectures Software Developer’s Manual Volume3A](http://pdosnew.csail.mit.edu/6.828/2014/readings/ia32/IA32-3A.pdf), Chapter 5
\[4\] 吴昱东，田堃，刘铭名. [JOS 异常处理流程](http://course.pku.edu.cn/courses/1/048-04830191-0006161023-1/db/_543158_1/x86_exception.html)