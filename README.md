# JOS Lab 2 Report

<!--TOC-->

## 总体概述


## 完成情况

|#|E1|E2|E3|Q1|E4|E5|Q2~6|C1|C2|C3|C4|
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|Status|√|√|√| | | | |√| | | |

<small>* 其中E#代表Exercise #, Q#代表Question #, C# 代表Challenge #</small>

### Part 1: Physical Page Management

#### Exercise 1
> In the file `kern/pmap.c`, you must implement code for the following functions (probably in the order given).

>`boot_alloc()`
`mem_init()` (only up to the call to `check_page_free_list(1)`)
`page_init()`
`page_alloc()`
`page_free()`

>`check_page_free_list()` and `check_page_alloc()` test your physical page allocator. You should boot JOS and see whether `check_page_alloc()` reports success. Fix your code so that it passes. You may find it helpful to add your own `assert()`s to verify that your assumptions are correct.

参照着前后的代码就可以完成了。

//TODO

### Part 2: Virtual Memory

#### Exercise 2

> Look at chapters 5 and 6 of the [Intel 80386 Reference Manual](http://pdosnew.csail.mit.edu/6.828/2014/readings/i386/toc.htm), if you haven't done so already. Read the sections about page translation and page-based protection closely (5.2 and 6.4). We recommend that you also skim the sections about segmentation; while JOS uses paging for virtual memory and protection, segment translation and segment-based protection cannot be disabled on the x86, so you will need a basic understanding of it.

阅读了其中的第5章和第6章，内容都在上学期操统课程中学习过了。这里简单概述一下要点。

//TODO


#### Exercise 3

> While GDB can only access QEMU's memory by virtual address, it's often useful to be able to inspect physical memory while setting up virtual memory. Review the QEMU [monitor commands](http://pdosnew.csail.mit.edu/6.828/2014/labguide.html#qemu) from the lab tools guide, especially the xp command, which lets you inspect physical memory. To access the QEMU monitor, press `Ctrl-a c` in the terminal (the same binding returns to the serial console).

>Use the `xp` command in the QEMU monitor and the `x` command in GDB to inspect memory at corresponding physical and virtual addresses and make sure you see the same data.

>Our patched version of QEMU provides an `info pg` command that may also prove useful: it shows a compact but detailed representation of the current page tables, including all mapped memory ranges, permissions, and flags. Stock QEMU also provides an `info mem` command that shows an overview of which ranges of virtual memory are mapped and with what permissions.

// TODO

#### Question 1

>Assuming that the following JOS kernel code is correct, what type should variable `x` have, `uintptr_t` or `physaddr_t`?
```
	mystery_t x;
	char* value = return_a_pointer();
	*value = 10;
	x = (mystery_t) value;
```
`uintptr_t`：`x`保存的是为`value`对应的地址，由于对`value`进行了解引用操作而且这个操作是“correct”的，所以可以得出`value`对应的地址是一个虚拟地址的结论，因此`x`的类型为`uintptr_`。

#### Exercise 4

>In the `file kern/pmap.c`, you must implement code for the following functions.

>      pgdir_walk()
>      boot_map_region()
>      page_lookup()
>      page_remove()
>      page_insert()

>`check_page()`, called from `mem_init()`, tests your page table management routines. You should make sure it reports success before proceeding.

// TODO

### Part 3: Kernel Address Space


#### Exercise 5

>Fill in the missing code in `mem_init()` after the call to `check_page()`.

>Your code should now pass the `check_kern_pgdir()` and `check_page_installed_pgdir()` checks.

// TODO

#### Question 2~6

##### Question 2
>What entries (rows) in the page directory have been filled in at this point? What addresses do they map and where do they point? In other words, fill out this table as much as possible:

|Entry|Base Virtual Address|Points to (logically):|
|:--:|:--:|:--:|
|1023|0xFFC00000|Page table for top 4MB of phys memory|
|1022|0xFF800000|Page table for the next 4MB of physical memory|
|.|?|?|
|.|?|?|
|.|?|?|
|2|0x00800000|?|
|1|0x00400000|?|
|0|0x00000000|[see next question]|

// TODO

##### Question 3

>We have placed the kernel and user environment in the same address space. Why will user programs not be able to read or write the kernel's memory? What specific mechanisms protect the kernel memory?

Page Directory Entry和Page Table Entry中的低12位中保存了相关的权限位，包括`PTE_U`、`PTE_W`；`PTE_U`位表示用户特权，`PTE_W`位设置为1表示可写。根据[Intel® 64 and IA-32 Architectures Software Developer’s Manual](http://pdosnew.csail.mit.edu/6.828/2014/readings/ia32/IA32-3A.pdf)中，页目录项与页表项权限位设置与实际访问/读写权限的关系如下表所示：

![enter image description here](http://ww4.sinaimg.cn/large/6313a6d8jw1eq7bnrw8enj216k10i7g2.jpg)

在分页机制开启后，将虚拟内存映射到物理内存过程中，kernel会去检查权限位的设置，以实现保护。

##### Question 4

> What is the maximum amount of physical memory that this operating system can support? Why?

2G

// TODO

##### Question 5

> How much space overhead is there for managing memory, if we actually had the maximum amount of physical memory? How is this overhead broken down?

Page Directory 1 PGSIZE
Page Tables 1 PTSIZE

// TODO

##### Question 6
> Revisit the page table setup in `kern/entry.S` and `kern/entrypgdir.c`. Immediately after we turn on paging, EIP is still a low number (a little over 1MB). At what point do we transition to running at an EIP above KERNBASE? What makes it possible for us to continue executing at a low EIP between when we enable paging and when we begin running at an EIP above KERNBASE? Why is this transition necessary?

```
   # kern/Entry.S
   
   59  	# Turn on paging.
   60  	movl	%cr0, %eax
   61 	orl	$(CR0_PE|CR0_PG|CR0_WP), %eax
   62  	movl	%eax, %cr0
   63  
   64 	# Now paging is enabled, but we're still running at a low EIP
   65  	# (why is this okay?).  Jump up above KERNBASE before entering
   66  	# C code.
   67 	mov	$relocated, %eax
   68  	jmp	*%eax
   69  relocated:
   ...
```

`kern/Entry.S`中67、68两行非直接跳转到`relocated`，就是这两句话将`EIP`跳转到`KERNBASE`之上。

在开启分页之后、转移之前仍能在低地址运行时因为在`kern/entrypgdir.c`同时定义了从低地址`[0, 4MB)`和高地址`[KERNBASE, KERNBASE+4MB)`到物理内存`[0, 4MB)`的映射。

跳转到69行`relocated`之后`EIP`开始在`KERNBASE`之上跑。这个跳转是必要的是因为如果不进行跳转，则计算机并不知道下一条指令是在高地址还是在低地址，只会继续在低地址的范围运行。

#### Challenge 1

>We consumed many physical pages to hold the page tables for the KERNBASE mapping. Do a more space-efficient job using the PTE_PS ("Page Size") bit in the page directory entries. This bit was not supported in the original 80386, but is supported on more recent x86 processors. You will therefore have to refer to Volume 3 of the current Intel manuals. Make sure you design the kernel to use this optimization only on processors that support it!

from manual

>Page size (PS) flag, bit 7 page-directory entries for 4-KByte pages
Determines the page size. When this flag is clear, the page size is 4 KBytes and the page-directory entry points to a page table. When the flag is set, the page size is 4 MBytes for normal 32-bit addressing (and 2 MBytes if extended physical addressing is enabled) and the page- directory entry points to a page. If the page-directory entry points to a page table, all the pages associated with that page table will be 4-KByte pages.

...
>**3.7.3. Mixing 4-KByte and 4-MByte Pages**
>When the PSE flag in CR4 is set, both 4-MByte pages and page tables for 4-KByte pages can be accessed from the same page directory. If the PSE flag is clear, only page tables for 4-KByte pages can be accessed (regardless of the setting of the PS flag in a page-directory entry).
>A typical example of mixing 4-KByte and 4-MByte pages is to place the operating system or executive’s kernel in a large page to reduce TLB misses and thus improve overall system performance.
The processor maintains 4-MByte page entries and 4-KByte page entries in separate TLBs. So, placing often used code such as the kernel in a large page, frees up 4-KByte-page TLB entries for application programs and tasks.

```c
	// Set cr4 to enable 4-MByte paging
	cr4 = rcr4();
	cr4 |= CR4_PSE;
	lcr4(cr4);
```
// TODO

#### Challenge 2

>Extend the JOS kernel monitor with commands to:

>- Display in a useful and easy-to-read format all of the physical page mappings (or lack thereof) that apply to a particular range of virtual/linear addresses in the currently active address space. For example, you might enter 'showmappings 0x3000 0x5000' to display the physical page mappings and corresponding permission bits that apply to the pages at virtual addresses 0x3000, 0x4000, and 0x5000.
- Explicitly set, clear, or change the permissions of any mapping in the current address space.
- Dump the contents of a range of memory given either a virtual or physical address range. Be sure the dump code behaves correctly when the range extends across page boundaries!
- Do anything else that you think might be useful later for debugging the kernel. (There's a good chance it will be!)

// TODO

#### Challenge 3

>Write up an outline of how a kernel could be designed to allow user environments unrestricted use of the full 4GB virtual and linear address space. Hint: the technique is sometimes known as "follow the bouncing kernel." In your design, be sure to address exactly what has to happen when the processor transitions between kernel and user modes, and how the kernel would accomplish such transitions. Also describe how the kernel would access physical memory and I/O devices in this scheme, and how the kernel would access a user environment's virtual address space during system calls and the like. Finally, think about and describe the advantages and disadvantages of such a scheme in terms of flexibility, performance, kernel complexity, and other factors you can think of.

// TODO

#### Challenge 4

>Since our JOS kernel's memory management system only allocates and frees memory on page granularity, we do not have anything comparable to a general-purpose malloc/free facility that we can use within the kernel. This could be a problem if we want to support certain types of I/O devices that require physically contiguous buffers larger than 4KB in size, or if we want user-level environments, and not just the kernel, to be able to allocate and map 4MB superpages for maximum processor efficiency. (See the earlier challenge problem about PTE_PS.)
Generalize the kernel's memory allocation system to support pages of a variety of power-of-two allocation unit sizes from 4KB up to some reasonable maximum of your choice. Be sure you have some way to divide larger allocation units into smaller ones on demand, and to coalesce multiple small allocation units back into larger units when possible. Think about the issues that might arise in such a system.

// TODO

## 遇到的困难以及解决方法

// TODO

## 收获及感想

// TODO

## 对课程的意见和建议

// TODO

## 参考文献

// TODO
