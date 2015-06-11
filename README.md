# JOS Lab 2 Report

[TOC]

## 总体概述


## 完成情况

|#|E1|E2|E3|Q1|E4|E5|Q2~6|C1|C2|C3|C4|
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|Status|√|√|√|√|√|√|√|√|√|√|√|

<small>* 其中E#代表Exercise #, Q#代表Question #, C# 代表Challenge #</small>

### Part 1: Physical Page Management

#### Exercise 1
> In the file `kern/pmap.c`, you must implement code for the following functions (probably in the order given).

>      boot_alloc()
>      mem_init() (only up to the call to check_page_free_list(1))
>      page_init()
>      page_alloc()
>      page_free()

>`check_page_free_list()` and `check_page_alloc()` test your physical page allocator. You should boot JOS and see whether `check_page_alloc()` reports success. Fix your code so that it passes. You may find it helpful to add your own `assert()`s to verify that your assumptions are correct.

`boot_alloc():`这个函数仅在系统初始化虚拟内存时来给页目录和页表分配空间。该函数的功能是分配能包含`n`个bytes的连续的对齐到页的空间。当`n==0`时，可以用来找到下一个空闲页。实现代码如下：
```
result = nextfree;

if (n > 0) {
	if (PADDR(nextfree) + n > npages * PGSIZE) {
		panic("boot_alloc: Insufficient memory for initial allocation\n");
	}

	nextfree = ROUNDUP((char *)nextfree + n, PGSIZE);
}

return result;
```

`mem_init()`中添加了为`pages`分配空间的代码，其中每一个`PageInfo`保存了对应物理页的元数据：

```c
struct PageInfo {
	// Next page on the free list.
	struct PageInfo *pp_link;

	// pp_ref is the count of pointers (usually in page table entries)
	// to this page, for pages allocated using page_alloc.
	// Pages allocated at boot time using pmap.c's
	// boot_alloc do not have valid reference count fields.

	uint16_t pp_ref;
};
```

实现代码如下：

```c
pages = (struct PageInfo *) boot_alloc(npages * sizeof(struct PageInfo));
memset(pages, 0, npages * sizeof(struct PageInfo));
```

`page_init()`中初始化了页描述符表。空闲页列表由一个简单的链表结构实现，每一个页对应的`PageInfo`中的`pp_link`保存了这个页在空闲页列表中前一个空闲页的位置。若该页已被分配，则`pp_link==NULL`。

初始化空闲页列表时有几个部分需要进行考虑：

1. 物理页`0`，其中保存了`IDT`和`BIOS`；
2. 为各种IO设备预留的空间`[IOPHYSMEM, EXTPHYSMEM)`；
3. 其他已经被使用的空间(包括kernel代码、页目录、页描述符表占的空间)。

最后一部分只要直接调用前面写的`boot_alloc(0)`即可得到其结束的位置。

实现代码如下：
```
size_t i, range_io, range_ext, free_top;
for (i = 0; i < npages; i++) {
	pages[i].pp_ref = 0;
	pages[i].pp_link = page_free_list;
	page_free_list = &pages[i];
}
extern char end[];
range_io = PGNUM(IOPHYSMEM);
range_ext = PGNUM(EXTPHYSMEM);
free_top = PGNUM(PADDR(boot_alloc(0)));

// 1) Mark physical page 0 as in use.
pages[1].pp_link = pages[0].pp_link;
pages[0].pp_link = NULL;

// 3) IO hole [IOPHYSMEM, EXTPHYSMEM)
pages[range_ext].pp_link = pages[range_io].pp_link;
for (i = range_io; i < range_ext; i++)
	pages[i].pp_link = NULL;

// 4) extended memory
pages[free_top].pp_link = pages[range_ext].pp_link;
for (i = range_ext; i < free_top; i++)
	pages[i].pp_link = NULL;
```

`page_alloc()`：如其名，用来分配一个页，实现也很容易，只需将空闲列表最后一项指向其下一页，再将该页描述符的指针清空即可：

```
struct PageInfo *
page_alloc(int alloc_flags)
{
	struct PageInfo *pp;
	pp = page_free_list;

	if (pp) {
		page_free_list = pp->pp_link;
		if (alloc_flags & ALLOC_ZERO)
			memset(page2kva(pp), 0, PGSIZE);
		pp->pp_link = NULL;
	} else
		return NULL;

	return pp;
}
```

`page_free()`：释放一个页，仅在动态引用它的指针数量为0时调用。实现只需将其插入空闲页列表的最后即可：

```
void
page_free(struct PageInfo *pp)
{
	// Hint: You may want to panic if pp->pp_ref is nonzero or
	// pp->pp_link is not NULL.
	if (pp->pp_ref)
		panic("page_free: Page reference counter nonzero");
	if (pp->pp_link)
		panic("page_free: Double-free attemps");

	pp->pp_link = page_free_list;
	page_free_list = pp;
}
```

### Part 2: Virtual Memory

#### Exercise 2

> Look at chapters 5 and 6 of the [Intel 80386 Reference Manual](http://pdosnew.csail.mit.edu/6.828/2014/readings/i386/toc.htm), if you haven't done so already. Read the sections about page translation and page-based protection closely (5.2 and 6.4). We recommend that you also skim the sections about segmentation; while JOS uses paging for virtual memory and protection, segment translation and segment-based protection cannot be disabled on the x86, so you will need a basic understanding of it.

阅读了其中的第5章和第6章，内容都在上学期操统课程中学习过了。这里简单概述一下要点。

首先，在JOS的设计中，Linear Address的Segment Selector长度为0，其Offset就等于Virtual Address，也就是说在JOS中Linear Address与Virtual Address是等价的。

![Format of a Linear Address](http://pdosnew.csail.mit.edu/6.828/2014/readings/i386/fig5-8.gif)

在分页机制下，一个Linear Address被分为三个部分`DIR[22,31]`、`PAGE[12,21]`、`OFFSET[0,11]`，其中`DIR`用来找到该地址所在物理页对应的页目录项，之后再依次根据`PAGE`、`OFFSET`最终确定其物理地址。

![Page Translation](http://pdosnew.csail.mit.edu/6.828/2014/readings/i386/fig5-9.gif)

此外，页目录项和页表项的`1`和`2`位用来保存其权限设置。处理器执行指令时，会检查当先的`CPL`以及访问目标的权限设置，以判断该该指令的权限是否满足。

![enter image description here](http://pdosnew.csail.mit.edu/6.828/2014/readings/i386/fig6-10.gif)


#### Exercise 3

> While GDB can only access QEMU's memory by virtual address, it's often useful to be able to inspect physical memory while setting up virtual memory. Review the QEMU [monitor commands](http://pdosnew.csail.mit.edu/6.828/2014/labguide.html#qemu) from the lab tools guide, especially the xp command, which lets you inspect physical memory. To access the QEMU monitor, press `Ctrl-a c` in the terminal (the same binding returns to the serial console).

>Use the `xp` command in the QEMU monitor and the `x` command in GDB to inspect memory at corresponding physical and virtual addresses and make sure you see the same data.

>Our patched version of QEMU provides an `info pg` command that may also prove useful: it shows a compact but detailed representation of the current page tables, including all mapped memory ranges, permissions, and flags. Stock QEMU also provides an `info mem` command that shows an overview of which ranges of virtual memory are mapped and with what permissions.

这里提到的几个指令对后面的Exercise很有帮助，其中需要注意的是`Ctrl+a c`指令需要先按下`Ctrl+a`，放手后再按`c`。

#### Question 1

>Assuming that the following JOS kernel code is correct, what type should variable `x` have, `uintptr_t` or `physaddr_t`?
```
	mystery_t x;
	char* value = return_a_pointer();
	*value = 10;
	x = (mystery_t) value;
```
`uintptr_t`：`x`保存的是为`value`对应的地址，由于对`value`进行了解引用操作而且这个操作是“correct”的，所以可以得出`value`对应的地址是一个虚拟地址的结论，因此`x`的类型为`uintptr_t`。

#### Exercise 4

>In the `file kern/pmap.c`, you must implement code for the following functions.

>      pgdir_walk()
>      boot_map_region()
>      page_lookup()
>      page_remove()
>      page_insert()

>`check_page()`, called from `mem_init()`, tests your page table management routines. You should make sure it reports success before proceeding.

`pgdir_walk()`用来找到一个虚拟地址`va`所对应的页表项`PTE`，在后面很多地方都会用上这个函数。根据注释，由于x86 MMU会同时在页目录和页表中检查权限，因此页目录的权限设置会比较宽容。此外，在过程中如果将要访问未分配的页，则调用`page_alloc`分配一个页。实现如下：

```
pte_t *
pgdir_walk(pde_t *pgdir, const void *va, int create)
{
	struct PageInfo *pp;
	pde_t *pdp;

	pdp = &pgdir[PDX(va)];

	if (*pdp & PTE_P) 
		return (pte_t *) KADDR(PTE_ADDR(*pdp)) + PTX(va);
	else if (create && (pp = page_alloc(1))) {
		pp->pp_ref++;
		*pdp = page2pa(pp) | PTE_P | PTE_W | PTE_U;
		return (pte_t *) KADDR(PTE_ADDR(*pdp)) + PTX(va);
	}
	return NULL;
}
```

`boot_map_region()`马上就用到了刚才实现的`pgdir_walk`——映射的时候只需将每一页大小的物理内存映射到虚拟内存即可，过程中页表的访问通过`pgdir_walk`可完成。

```
static void
boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm)
{
	pte_t *ptep;
	uintptr_t cv;
	physaddr_t cp;

	for (cv = 0, cp = pa; cv < size; 
	     cv += PGSIZE, cp += PGSIZE) {
		ptep = pgdir_walk(pgdir, (const void *) (va + cv), 1);
		if (ptep)
			*ptep = cp | perm | PTE_P;
	}
}
```

`page_lookup()`查找`va`映射的物理页，根据需求保存对应`pte`到`pte_store`中。

```
struct PageInfo *
page_lookup(pde_t *pgdir, void *va, pte_t **pte_store)
{
	pte_t *ptep;

	ptep = pgdir_walk(pgdir, va, 0);
	if (ptep) {
		if (pte_store)
			*pte_store = ptep;
		return pa2page(PTE_ADDR(*ptep));
	} else
		return NULL;
}
```

`page_remove()`清除虚拟地址`va`的映射，并减小其之前对应的物理页的引用计数（为0时则调用`page_free`将物理页放回空闲列表）。此外还需要调用`tlb_invalidate`清除`tlb`缓存。

```
void
page_remove(pde_t *pgdir, void *va)
{
	struct PageInfo *pp;
	pte_t *pte;

	pp = page_lookup(pgdir, va, &pte);

	if (pp) {
		page_decref(pp);
		*pte = 0;
		tlb_invalidate(pgdir, va);
	}
}
```

`page_insert()`将一个增加一个物理页`pp`到虚拟地址`va`的映射，若`va`之前有映射到其他的地方，则先调用`page_remove`将之前的映射清除。

```
int
page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm)
{
	pte_t *ptep;

	ptep = pgdir_walk(pgdir, va, 1);
	if (ptep) {
		pp->pp_ref++;
		if (PTE_ADDR(*ptep))
			page_remove(pgdir, va);
		*ptep = page2pa(pp) | perm | PTE_P;
		return 0;
	} else
		return -E_NO_MEM;
}
```

### Part 3: Kernel Address Space


#### Exercise 5

>Fill in the missing code in `mem_init()` after the call to `check_page()`.

>Your code should now pass the `check_kern_pgdir()` and `check_page_installed_pgdir()` checks.

这道题在之前几个Exercise基础上实现非常容易。分别将页描述符表、内核栈空间的物理空间映射到了虚拟地址空间，然后将KERNBASE之上的所有内容与物理地址从`0`开始的位置映射起来。直接调用`boot_map_region`即可完成。其中`0x10000000=0xFFFFFFFF-KERNBASE+1`，即为KERNBASE上所有地址空间的大小。实现如下：

```
boot_map_region(kern_pgdir, UPAGES, PTSIZE, PADDR(pages), PTE_U);

boot_map_region(kern_pgdir, KSTACKTOP-KSTKSIZE, KSTKSIZE, PADDR(bootstack), PTE_W);

boot_map_region(kern_pgdir, KERNBASE, 0x10000000, 0, PTE_W);
```

`make grade`结果：

![enter image description here](http://ww3.sinaimg.cn/large/6313a6d8jw1eqaz3fkz56j211g0n8agf.jpg)

#### Question 2~6

##### Question 2
>What entries (rows) in the page directory have been filled in at this point? What addresses do they map and where do they point? In other words, fill out this table as much as possible:

|Entry|Base Virtual Address|Points to (logically):|
|:--:|:--:|:--|
|1023|0xFFC00000|Page table for top 4MB of phys memory|
|1022|0xFF800000|Page table for the next 4MB of phys memory|
|.|?|?|
|960|0xF0000000|Page table for bottom 4MB of phys memory|
|959|0xEFC00000|Kernel Stack|
|958|0xEF800000|Memory-mapped I/O|
|957|0xEF400000|Read-only copies of page structures|
|956|0xEF000000|User read-only virtual page table|
|.|?|?|
|2|0x00800000|?|
|1|0x00400000|?|
|0|0x00000000|[see next question]|

##### Question 3

>We have placed the kernel and user environment in the same address space. Why will user programs not be able to read or write the kernel's memory? What specific mechanisms protect the kernel memory?

Page Directory Entry和Page Table Entry中的低12位中保存了相关的权限位，包括`PTE_U`、`PTE_W`；`PTE_U`位表示用户特权，`PTE_W`位设置为1表示可写。根据[Intel® 64 and IA-32 Architectures Software Developer’s Manual](http://pdosnew.csail.mit.edu/6.828/2014/readings/ia32/IA32-3A.pdf)中，页目录项与页表项权限位设置与实际访问/读写权限的关系如下表所示：

![enter image description here](http://ww4.sinaimg.cn/large/6313a6d8jw1eq7bnrw8enj216k10i7g2.jpg)

在分页机制开启后，将虚拟内存映射到物理内存过程中，kernel会去检查权限位的设置，以实现保护。

##### Question 4

> What is the maximum amount of physical memory that this operating system can support? Why?

UPAGES区的大小为PTSIZE，所有的PageInfo都必须能放在其中，即最多有`PTSIZE/sizeof(PageInfo) = (PGSIZE*NPTENTRIES)/sizeof(PageInfo) = 512K`个PageInfo。

每个PageInfo对应1个物理页，即4096 Bytes。因此总的最大能覆盖的物理内存的大小是：$512K \times 4096 Bytes =  2 GBytes$。

##### Question 5

> How much space overhead is there for managing memory, if we actually had the maximum amount of physical memory? How is this overhead broken down?

Page Directory 需要 `1 * PGSIZE`
Page Tables 需要`NPTENTRIES * PGSIZE`
PageInfo数组需要`1 PTSIZE`(在内存中预留了这么多空间)
总计：`4K + 4M + 4M = 8M + 4K (Bytes)`

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

>We consumed many physical pages to hold the page tables for the KERNBASE mapping. Do a more space-efficient job using the PTE_PS ("Page Size") bit in the page directory entries. This bit was not supported in the original 80386, but is supported on more recent x86 processors. You will therefore have to refer to [Volume 3](http://pdosnew.csail.mit.edu/6.828/2014/readings/ia32/IA32-3A.pdf) of the current Intel manuals. Make sure you design the kernel to use this optimization only on processors that support it!

由 [the Intel manual](http://pdosnew.csail.mit.edu/6.828/2014/readings/ia32/IA32-3A.pdf)，

>**3.7.3. Mixing 4-KByte and 4-MByte Pages**
>When the PSE flag in CR4 is set, both 4-MByte pages and page tables for 4-KByte pages can be accessed from the same page directory. If the PSE flag is clear, only page tables for 4-KByte pages can be accessed (regardless of the setting of the PS flag in a page-directory entry).
>A typical example of mixing 4-KByte and 4-MByte pages is to place the operating system or executive’s kernel in a large page to reduce TLB misses and thus improve overall system performance.
The processor maintains 4-MByte page entries and 4-KByte page entries in separate TLBs. So, placing often used code such as the kernel in a large page, frees up 4-KByte-page TLB entries for application programs and tasks.

要打开`4M`页表首先要设置cr4中的`PSE`标志。之后在若将页目录项中的7号位`PS`置为1则可将其设置为`4M`模式，其页目录项的页基址直接指向内存中大小为`4MB`的物理页，对于未设置`PS`标志的则与之前一样指向大小为`4KB`的页表。

![enter image description here](http://ww1.sinaimg.cn/large/6313a6d8jw1eqadpc95psj212e0lojvi.jpg)

`4MB`的页目录项结构如上图所示，与`4KB`的页目录项和页表项的主要区别在于其中`[13,21]`位被设置为`0`，这是因为虚拟地址（如下图所示）中低21位`[0,20]`都要用被用来作为`Offset`，为了兼容性依然只有`[0,12]`位用于各种标记，因此`[13,21]`位就被保留为了`0`。

![enter image description here](http://ww3.sinaimg.cn/large/6313a6d8jw1eqadr81nlhj212s0l2ju9.jpg)

参考上面引用的Intel Manual的内容，`4MB`页的一个用法就是用`4MB`页来保存`kernel`部分的内容，并使用分别的`TLB`来管理，以降低访问内核的`TLB`miss率以及总体性能。

**具体实现**

首先在`kern/pmap.c mem_init()`中设置`cr4`：

```c
	// Set cr4 to enable 4-MByte paging
	cr4 = rcr4();
	cr4 |= CR4_PSE;
	lcr4(cr4);
```

并修改`KERNBASE`以上区域的映射时的全选设置，添加进`PTE_PS`标记：

```c
boot_map_region(kern_pgdir, KERNBASE, 0x10000000, 0, PTE_W | (PTE_PS & PSE_ENABLED));
```

其中`PSE_ENABLED`是自己定义的全局变量，用来开启/关闭`PSE`机制。

现在由于页目录项的结构有所变化，因此`boot_map_region`函数内部的实现也需要有所改变，在需要设置`PTE_PS`时，直接改变对应页目录项的值：

```
static void
boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm)
{
	pte_t *ptep;
	uintptr_t cv;
	physaddr_t cp;

	if (perm & PTE_PS) {
		for (cv = 0, cp = pa; cv < size; cv += PTSIZE, cp += PTSIZE) {
			ptep = &pgdir[PDX(va + cv)];
			*ptep = cp | perm | PTE_P;
		}
	} else {
		for (cv = 0, cp = pa; cv < size; cv += PGSIZE, cp += PGSIZE) {
			ptep = pgdir_walk(pgdir, (const void *) (va + cv), 1);
			if (ptep)
				*ptep = cp | perm | PTE_P;
		}
	}
}
```

到这里`4MB`页的设置就完成了。但是如果这个时候执行`make grade`的话`check_va2pa`中会报错。这是因为`check_va2pa`将所有的页目录项一视同仁，进行两级的寻址；然而在`4MB`页开启之后，有些页目录项之后直接指向的是对应的物理页，而不能进行两级查找。因此我在`check_va2pa`中添加了如下代码作为应对：

```
if (*pgdir & PTE_PS & PSE_ENABLED)
	return PTE_ADDR(*pgdir) | (PTX(va) << PTXSHIFT);
```

这段代码只有在`PSE`机制开启后才会生效。

**测试运行结果**

在`qemu`中输入`info pg`打印出页表的状态：

![Page Table](http://ww3.sinaimg.cn/large/6313a6d8jw1eqatry3ea5j21160d0n49.jpg)

其中`PDE[3bd]`即为页目录的自映射，其中每一项对应着页目录中的每一项。`PDE[3c0-3d0]`、`PDE[3d1-3ff]`即为`KERNBASE`之上，也就是刚才设置为了`4MB`页模式的目录项。可以看到这两个目录项下并没有`PTE`，并且对应的物理页恰好覆盖了物理内存的前`256MB`。

现在`4MB`的静态映射虽然已经成功了，但是由于`JOS`的代码中目前物理页分配完全基于针对`4KB`页的PageInfo数组和空闲页链表，无法直接进行`4MB`页的动态分配。如果要实现对`4MB`页的分配就需要将整个物理页管理的方式进行重写过于复杂了`_(:з」∠)_`。目前这样也能工作暂且就不修改了。。


#### Challenge 2

>Extend the JOS kernel monitor with commands to:

>- Display in a useful and easy-to-read format all of the physical page mappings (or lack thereof) that apply to a particular range of virtual/linear addresses in the currently active address space. For example, you might enter 'showmappings 0x3000 0x5000' to display the physical page mappings and corresponding permission bits that apply to the pages at virtual addresses 0x3000, 0x4000, and 0x5000.
- Explicitly set, clear, or change the permissions of any mapping in the current address space.
- Dump the contents of a range of memory given either a virtual or physical address range. Be sure the dump code behaves correctly when the range extends across page boundaries!
- Do anything else that you think might be useful later for debugging the kernel. (There's a good chance it will be!)

实现了`mon_showmappings`、`mon_setperm`、`mon_dump`、`mon_shutdown`几个函数。前面3个函数有大量的处理格式化和边界的代码，并且针对了`4MB`页进行了处理，代码比较长，因此这里只写一下其中关键的功能部分。

**`mon_showmappings`**：使用`showmappings start_addr [end_addr]`，输出在`start_addr`和`end_addr`之间开始的页的映射。如果只输入`start_addr`，则输出`start_addr`之后第一个页的映射。

```
for (i = va1; va1 <= i && i <= va2; i = pttop) {
	if (over || flushscreen(count, 20, 1))
		break;
	pttop = ROUNDUP(i+1, PTSIZE);
	pdep = &kern_pgdir[PDX(i)];
	if (pdep && (*pdep & PTE_P)) {
		if (!(*pdep & PTE_PS)) {
			// 4-KByte
			for (i; i < pttop; i += PGSIZE) {
				if ((over = (flushscreen(count, 20, 1) == 1 || 
					i > va2 || i < va1)))
					break;
				ptep = pgdir_walk(kern_pgdir, (const void *)i, 0);
				if (ptep &&(*ptep & PTE_P))
					printmap(ptep, i, PGSIZE);
				else // pte unmapped
					printmap(ptep, i, -PGSIZE);	
				count++;
			}
		} else {
			// 4-MByte
			printmap(pdep, ROUNDDOWN(i, PTSIZE), PTSIZE);
			count++;
		}
	} else {
		// pde unmapped
		printmap(pdep, i, -PTSIZE);
		count++;
	}
}
```
外层循环与页表为单位执行，内层循环以页为单位执行。其中`flushscreen`在输出过多时使用，每20行提示一次是否要停止输出；`printmap`负责格式化输出每一行的结果。

![test](http://ww3.sinaimg.cn/large/6313a6d8jw1eqaus5ni0rj21100n27ia.jpg)

测试输入：`showmappings 0xef000000 0xffff0000`，从kernel stack顶部几个页到`KERNBASE`底部几个页的映射情况，可以看出输出结果正常：`KERNBASE`之下前8块页为`CPU0' kernel stack`的内容，在此之前为`Invalid Memory`，因此映射显示为`no mapping`；`KERNBASE`之上为`4MB`的页，因此以`4-MByte`为单位进行显示。

**`mon_setperm`**：使用`setperm addr [+|-]perm`，`perm`可为`P`、`U`、`W`其中之一或组合，大小写无关。在终端会先后输出对`addr`**所在的页**修改权限前的状态和修改后的结果。

```
pdep = &kern_pgdir[PDX(addr)];
if (pdep) { // no need to judge whether PTE_P stands
	if (*pdep & PTE_PS) {
		// 4-MByte
		addr = ROUNDDOWN(addr, PTSIZE);
		printmap(pdep, addr, PTSIZE);
		setperm(pdep, perms);
		cprintf(" ---->\n");
		printmap(pdep, addr, PTSIZE);
	} else {
		// 4-KByte
		ptep = pgdir_walk(kern_pgdir, (const void *) addr, 0);
		if (ptep) {
			printmap(ptep, addr, PGSIZE);
			setperm(ptep, perms);
			cprintf(" ---->\n");
			printmap(ptep, addr, PGSIZE);
		} else
			printmap(ptep, addr, -PGSIZE);
	}
} else {
	// pde unmapped
	addr = ROUNDDOWN(addr, PTSIZE);
	printmap(pdep, addr, -PTSIZE);
}
```
`perms`数组的内容由输入而定，`perm`串以`+`开头或直接以`P`、`U`、`W`开头的情况为1，以`-`开头则为`-1`，除此之外的情况为`0`。权限设置的功能由`setperm`实现：

```
void
setperm(pte_t *ptep, int perms[])
{
	if (perms[0] == 1)
		*ptep |= PTE_P;
	else if (perms[0] == -1)
		*ptep &= ~PTE_P;
	if (perms[1] == 1)
		*ptep |= PTE_U;
	else if (perms[1] == -1)
		*ptep &= ~PTE_U;
	if (perms[2] == 1)
		*ptep |= PTE_W;
	else if (perms[2] == -1)
		*ptep &= ~PTE_W;
}
```

测试权限增减：

![test setperm](http://ww2.sinaimg.cn/large/6313a6d8jw1eqavggcaxkj213g08owi7.jpg)

测试`4MB`页权限设置：

![test setperm 4mb](http://ww2.sinaimg.cn/large/6313a6d8jw1eqavcuynw4j211o04s40a.jpg)

结果正确。

**`mon_dump`**：使用`dump start_addr len`，输出两个地址之间的内容，模仿gdb的格式，4字节为单位。若没有物理地址映射到对应虚拟地址范围，则输出`pgunmapped`。

```
for (i = 0; va1 < va2; va1 = (uintptr_t)((uint32_t *)va1 + 1), i++){
	if (flushscreen(i, 23*4, 0))
		break;
	if (!(i % 4))
		cprintf("0x%08x:", va1);

	if (checkmapping(va1))
		cprintf("\t0x%08x", *(uint32_t *)va1);
	else
		cprintf("\tpgunmapped");

	if (i % 4 == 3)
		cprintf("\n");
}
```

测试：与`gdb`结果对比

`gdb`：

![gdb](http://ww4.sinaimg.cn/large/6313a6d8jw1eqaymkfcpyj20yc0mgk4i.jpg)

`mon_dump`：

![mon_dump](http://ww3.sinaimg.cn/large/6313a6d8gw1eqbaq0lra2j20su0iqajt.jpg)

结果完全一致。

**`mon_shutdown`**：使用`shutdown`，关闭`JOS`和`qemu`。由于调试过程中经常需要关掉`qemu`重新编译再运行，而关掉`qemu`这个过程操作次数太多(`Ctrl+a c`-->`quit <Enter>`)，而且在`qemu`本身的窗口中按`Ctrl+a c`并无法召出`qemu`控制台，所以就想要写这么一个接口。参考了Github上[chaOS](https://github.com/Kijewski/chaOS/)的代码，具体实现：

```
int
mon_shutdown(int argc, char **argv, struct Trapframe *tf)
{
	const char *s = "Shutdown";

	__asm __volatile ("cli");
	
	for (;;) {
		// (phony) ACPI shutdown (http://forum.osdev.org/viewtopic.php?t=16990)
		// Works for qemu and bochs.
		outw (0xB004, 0x2000);

		// Magic shutdown code for bochs and qemu.
		while(*s) {
			outb (0x8900, *s);
			++s;
		}

		// Magic code for VMWare. Also a hard lock.
		__asm __volatile ("cli; hlt");
	}
}

```

#### Challenge 3

>Write up an outline of how a kernel could be designed to allow user environments unrestricted use of the full 4GB virtual and linear address space. Hint: the technique is sometimes known as "follow the bouncing kernel." In your design, be sure to address exactly what has to happen when the processor transitions between kernel and user modes, and how the kernel would accomplish such transitions. Also describe how the kernel would access physical memory and I/O devices in this scheme, and how the kernel would access a user environment's virtual address space during system calls and the like. Finally, think about and describe the advantages and disadvantages of such a scheme in terms of flexibility, performance, kernel complexity, and other factors you can think of.

网上能搜到的资料全是Lab的题目`_(:з」∠)_`。参考了Github上一个叫[`jos-mmap`](https://github.com/cmjones/jos-mmap/blob/master/answers-lab2.txt)的项目中的描述，结合自己的理解，应该只要在用户访问虚存中原本无权限的页（比如保存了`IDT`和`BIOS`的物理页`0`、页表结构，还有一些其他预留的页）时，将对应的物理页映射到虚存中另一个位置，再重新为用户分配该页(这个过程只改变映射关系，不改变物理内存中的内容)。这样用户就能使用整个4G的虚存空间了。

**用户态和内核态的转换**

用户态-->内核态过程中，需要`IDT`以定位中断处理例程。要找到`IDT`需要将其从物理内存中映射到虚存空间中某一个固定的位置(比如`0xF0000000`)。此时只需先备份`0xF0000000`之前的页表项，再将`IDT`映射过去即可。后面就跟普通的中断处理过程一样了。

内核态-->用户态：恢复现场，改写PSW转回用户态，然后恢复用户空间的备份的页表项。

**优缺点**

**优点**：用户能用整个4G虚存空间。

**缺点**：这种每次发生权限错误都会重新进行页表的映射，Overhead非常大。


#### Challenge 4

>Since our JOS kernel's memory management system only allocates and frees memory on page granularity, we do not have anything comparable to a general-purpose malloc/free facility that we can use within the kernel. This could be a problem if we want to support certain types of I/O devices that require physically contiguous buffers larger than `4KB` in size, or if we want user-level environments, and not just the kernel, to be able to allocate and map `4MB` superpages for maximum processor efficiency. (See the earlier challenge problem about `PTE_PS`.)
Generalize the kernel's memory allocation system to support pages of a variety of power-of-two allocation unit sizes from `4KB` up to some reasonable maximum of your choice. Be sure you have some way to divide larger allocation units into smaller ones on demand, and to coalesce multiple small allocation units back into larger units when possible. Think about the issues that might arise in such a system.

这个Challenge的要求就是实现一个类似伙伴系统的算法，思路如下：

1. 为4KB~4MB的页面以2为公比，分别维护各自的`page_free_list[i]`，其中$i = \log_2(Pagesize/4KB)$

2. 初始化时（遍历原先的`page_free_list`）将每`1024`块相邻的`page`作为一个`4MB`的free page插入相应的`page_free_list`。

3. 当分配一个大小为`s`的空间时，
	- 若`page_free_list[u]`($2^{u-1}<s\leq2^u$)不为空，则分配`page_free_list[u]`中的一块空间；
	- 否则检查`page_free_list[u+1]`是否为空，不为空则进入下一步，为空则迭代进行，直到找到不为空的free list；若不存在这样的free list，则返回错误；
	- 从找到的`page_free_list[k]`中取出一个块，将其均分后放入`page_free_list[k-1]`，然后对`page_free_list[k-1]`进行同样的操作，直到`k == u`为止；
	- 此时`page_free_list[u]`不为空，从中分配一个块给`s`即可。

4. 当释放一个块的空间时，
	- 将其插回对应的`page_free_list[u]`；
	- 遍历一遍`page_free_list[u]`链表，看有没有与其空间连续的块，若有，则从`page_free_list[u]`中删除这两个块，将它们合并并插入`page_free_list[u+1]`；
	- 若有$2^u == 4MB$，则不进行合并。

具体实现要大量改动lab的代码，而且在`ICS`也做过类似的东西了，当时调试的绝望依然难以忘怀`_(:з」∠)_`，就不实现到代码了。

## 遇到的困难以及解决方法

可能是由于之前对于内存管理这块的学习不够扎实，拿到lab开始纠结了很久不知道怎么下手。一开始甚至卡了壳儿一下没理解什么叫`alloc`一块空间。不过冷静下来好好整理了一下之前学过的东西，也就能够起手了。再加上lab中大量的注释，让代码编写的整体思路变得很清晰。只要按着注释来做，几个`check`的函数都能过。

剩下的问题就是不时会遇到的`triple fault`，先用`make qemu-gdb`，再运行`gdb`，在`gdb`中进行调试，很快就能锁定出错的位置，然后加以解决了。

## 收获及感想

这次的lab“强迫”我把之前关于内存管理没有搞透彻的地方弄明白了，从前一次lab中设置的线性映射，到此次lab中完整的页表机制，以及在课上对诸如“自映射”之类问题的讨论，让我对物理页管理、虚存初始化和管理这块有了比较系统、深入的理解。很多东西真的是要认真去思考过了，动手做过了才能明白的。

在反复调试`4MB`页的设置，以及后一个Challenge中针对`4MB`页表的处理也让我经常需要去看`4KB`和`4MB`的虚拟地址结构、页目录项、页表项结构。这几个地方从之前ICS开始就是每次用上都得重新去查这次lab总算是把它们深深地印在我脑子里了。

## 参考文献

[1] [Intel 80386 Reference Manual](http://pdosnew.csail.mit.edu/6.828/2014/readings/i386/toc.htm)
[2] [QEMU monitor commands](http://pdosnew.csail.mit.edu/6.828/2014/labguide.html#qemu) 
[3] [Intel® 64 and IA-32 Architectures Software Developer’s Manual Volume3A](http://pdosnew.csail.mit.edu/6.828/2014/readings/ia32/IA32-3A.pdf)
[4] [Kijewski/chaOS](https://github.com/Kijewski/chaOS/)
[5] [ACPI poweroff](http://forum.osdev.org/viewtopic.php?t=16990)
[6] [cmjones/jos-mmap](https://github.com/cmjones/jos-mmap/blob/master/answers-lab2.txt)