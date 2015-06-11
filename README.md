# JOS Lab 1 Report

[TOC]

## 总体概述
本次Lab分为对实验环境的配置和熟悉，Bootloader，JOS kernel这三个部分，一步一步引入，对OS启动和初始化的过程进行了学习。

## 完成情况

|Exercise#|01|02|03|04|05|06|07|08|ch|09|10|11|12|
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|Status|√|√|√|√|√|√|√|√|√|√|√|√|√|

<small>* 其中ch代表challenge</small>

### Part 1: PC Bootstrap

#### Exercise 1
> Familiarize yourself with the assembly language materials available on [the 6.828 reference page](http://pdos.csail.mit.edu/6.828/2012/reference.html). You don't have to read them now, but you'll almost certainly want to refer to some of this material when reading and writing x86 assembly.
> 
> We do recommend reading the section "The Syntax" in [Brennan's Guide to Inline Assembly](http://www.delorie.com/djgpp/doc/brennan/brennan_att_inline_djgpp.html). It gives a good (and quite brief) description of the AT&T assembly syntax we'll be using with the GNU assembler in JOS.

之前在ICS、计算机组成、计算机体系结构课程中都学过/使用过了，帮助比较大的是AT&T和intel风格对比那篇文章。


#### Exercise 2

> Use GDB's si (Step Instruction) command to trace into the ROM BIOS for a few more instructions, and try to guess what it might be doing. You might want to look at [Phil Storrs I/O Ports Description](http://web.archive.org/web/20040404164813/members.iweb.net.au/~pstorr/pcbook/book2/book2.htm), as well as other materials on [the 6.828 reference materials page](http://pdosnew.csail.mit.edu/6.828/2014/reference.html). No need to figure out all the details - just the general idea of what the BIOS is doing first.


```c
The target architecture is assumed to be i8086
[f000:fff0]    0xffff0:	ljmp   $0xf000,$0xe05b
[f000:e05b]    0xfe05b:	jmp    0xfc85e
```
由于处理器的设计，BIOS代码从`0xffff0`开始执行，这里离给BIOS分配的空间的顶端只有16字节的空间，因此连续执行了两次跳转指令，从BIOS真正开始的地方执行。
```c
[f000:c85e]    0xfc85e:	mov    %cr0,%eax	
[f000:c861]    0xfc861:	and    $0x9fffffff,%eax
[f000:c867]    0xfc867:	mov    %eax,%cr0	
```
这3句设置了控制寄存器cr0，将CD和NW标志清零(参考[维基百科](http://en.wikipedia.org/wiki/Control_register))。gdb查看cr0 = 0x00000010（此时处于实模式）。

```c
[f000:c86a]    0xfc86a:	cli    
[f000:c86b]    0xfc86b:	cld    
[f000:c86c]    0xfc86c:	mov    $0x8f,%eax			
[f000:c872]    0xfc872:	out    %al,$0x70
[f000:c874]    0xfc874:	in     $0x71,%al
[f000:c876]    0xfc876:	cmp    $0x0,%al
[f000:c878]    0xfc878:	jne    0xfc88d
```
这几句是在设置NMI，读写CMOS，根据网上查到的资料
>Whenever you send a byte to IO port 0x70, the high order bit tells the hardware whether to disable NMIs from reaching the CPU. If the bit is on, NMI is disabled (until the next time you send a byte to Port 0x70). The low order 7 bits of any byte sent to Port 0x70 are used to address CMOS registers.  [*[CMOS - OSDev Wiki](http://wiki.osdev.org/CMOS)*]
>

```c
 void NMI_enable(void)
 {
    outb(0x70, inb(0x70)&0x7F);
 }
 
 void NMI_disable(void)
 {
    outb(0x70, inb(0x70)|0x80);
 }
```


可此可知，0x8f中最高位为1，即关闭NMI，这里的作用应该是为了让后面读0x71端口的时候的值避免NMI可能产生的影响。0x8f低7位为0x0f，即选择了CMOS的0x0f号寄存器。这个寄存器中的值表示计算机的关闭状态(shutdown status)，0x0代表正常启动。(参考[Bochs Developers Guide](http://bochs.sourceforge.net/doc/docbook/development/cmos-map.html))

简单来说，这几句代码的意思就是在`CMOS`中检查计算机开启/关闭的状态，若正常则继续执行；否则跳转至`0xfc88d`处对其他情况进行处理。

```c
[f000:c87a]    0xfc87a:	xor    %ax,%ax
[f000:c87c]    0xfc87c:	mov    %ax,%ss
[f000:c87e]    0xfc87e:	mov    $0x7000,%esp
[f000:c884]    0xfc884:	mov    $0xf4b2c,%edx
```
这几句设置了栈的段寄存器`%ss`和栈定寄存器`%esp`，栈的空间为`0x00000`到`0x07000`。

```c
[f000:c88a]    0xfc88a:	jmp    0xfc719
[f000:c719]    0xfc719:	mov    %eax,%ecx
[f000:c71c]    0xfc71c:	cli    
[f000:c71d]    0xfc71d:	cld    
[f000:c71e]    0xfc71e:	mov    $0x8f,%eax
[f000:c724]    0xfc724:	out    %al,$0x70
[f000:c726]    0xfc726:	in     $0x71,%al
[f000:c728]    0xfc728:	in     $0x92,%al
[f000:c72a]    0xfc72a:	or     $0x2,%al
[f000:c72c]    0xfc72c:	out    %al,$0x92
```
后面这几行代码是用来通过`System Control Port A`(`0x92`)激活`A20`总线（激活前所有地址中的20位将被清零，具体参见[这篇文章](http://www.win.tue.nl/~aeb/linux/kbd/A20.html)），准备进入保护模式。

```c
[f000:c72e]    0xfc72e:	lidtw  %cs:-0x31cc
[f000:c734]    0xfc734:	lgdtw  %cs:-0x3188
[f000:c73a]    0xfc73a:	mov    %cr0,%eax
[f000:c73d]    0xfc73d:	or     $0x1,%eax
[f000:c741]    0xfc741:	mov    %eax,%cr0
[f000:c744]    0xfc744:	ljmpl  $0x8,$0xfc74c
```
加载全局/终端描述符表寄存器，设置`cr0`最低位为1，进入保护模式，并跳转到对应的代码。

```c
The target architecture is assumed to be i386
=> 0xfc74c:	mov    $0x10,%eax
=> 0xfc751:	mov    %eax,%ds
=> 0xfc753:	mov    %eax,%es
=> 0xfc755:	mov    %eax,%ss
=> 0xfc757:	mov    %eax,%fs
=> 0xfc759:	mov    %eax,%gs
=> 0xfc75b:	mov    %ecx,%eax
=> 0xfc75d:	jmp    *%edx
```
设置保护模式下的段寄存器，然后`%edx`中保存的位置。此后有很长很长的循环执行的代码，应该是`BIOS`在对各种设备进行测试和初始化。

在这个过程中CPU不断地在保护模式和实模式之间转换，指令的地址也在`0xcxxxx`~`0xfxxxx`和`0xfxxxxxxx`之间转换。

指令在高位时CPU处于保护模式，将高位的设备映射到低位：
```c
=> 0xffff3d5d:	rep movsb %ds:(%esi),%es:(%edi)
0xffff3d5d in ?? ()
(gdb) p/x $ds
$1 = 0x10
(gdb) p/x $esi
$2 = 0xfffe667a
(gdb) p/x $es
$3 = 0x10
(gdb) p/x $edi
$4 = 0xe667a
```
在低位时，CPU处于实模式，执行对设备的检查、测试、初始化等操作。

自检完成之后`BIOS`会寻找一个可启动的设备（硬盘、光驱、软盘等），并从中（硬盘中的前512字节）载入并将控制权转交给`bootloader`。

### Part 2: The Boot Loader

#### Exercise 3

> Take a look at the lab tools guide, especially the section on GDB commands. Even if you're familiar with GDB, this includes some esoteric GDB commands that are useful for OS work.
>
>Set a breakpoint at address 0x7c00, which is where the boot sector will be loaded. Continue execution until that breakpoint. Trace through the code in boot/boot.S, using the source code and the disassembly file obj/boot/boot.asm to keep track of where you are. Also use the x/i command in GDB to disassemble sequences of instructions in the boot loader, and compare the original boot loader source code with both the disassembly in obj/boot/boot.asm and GDB.

>Trace into bootmain() in boot/main.c, and then into readsect(). Identify the exact assembly instructions that correspond to each of the statements in readsect(). Trace through the rest of readsect() and back out into bootmain(), and identify the begin and end of the for loop that reads the remaining sectors of the kernel from the disk. Find out what code will run when the loop is finished, set a breakpoint there, and continue to that breakpoint. Then step through the remainder of the boot loader.

- At what point does the processor start executing 32-bit code？ 
 `ljmp    $PROT_MODE_CSEG, $protcseg`跳转之后从`movw    $PROT_MODE_DSEG, %ax`开始执行32-bit代码。

- What exactly causes the switch from 16- to 32-bit mode?
 ```
  lgdt    gdtdesc
  movl    %cr0, %eax
  orl     $CR0_PE_ON, %eax
  movl    %eax, %cr0
 ```
其中加载了全局描述符表，然后将`cr0`中的PE位置`1`，即实现从实模式到保护模式的转换。

- What is the last instruction of the boot loader executed? 
`main.c`中的`((void (*)(void)) (ELFHDR->e_entry))();`在GDB中反汇编代码中对应：`0x7d5e:	call   *0x10018`，即跳转到`kernel`去。

- What is the first instruction of the kernel it just loaded?
在GDB中查看：`0x10000c:	movw   $0x1234,0x472`；
在`entry.S`中对应：
 ```
.globl entry
entry:
	movw	$0x1234,0x472			# warm boot
 ```

- Where is the first instruction of the kernel?
由上一问可得，或者直接在终端中输入：
```
$ objdump -f obj/kern/kernel

obj/kern/kernel:     file format elf32-i386
architecture: i386, flags 0x00000112:
EXEC_P, HAS_SYMS, D_PAGED
start address 0x0010000c

$
```
`kernel`第一条指令的地址为`0x0010000c`，在`entry.S`中的对应第`44`行。

- How does the boot loader decide how many sectors it must read in order to fetch the entire kernel from disk? Where does it find this information?
`bootloader`会先从硬盘中读入`ELF File Header`：

 ```c
readseg((uint32_t) ELFHDR, SECTSIZE*8, 0);
 ```
 
 由(`ELFHDR`的地址 + 程序头表的文件便宜`e_phoff`)能得到开始其中保存的起始程序头的地址`ph`，`eph = ph + ELF Header中总的程序头个数e_phnum`为结束地址。

 利用`ph`和`eph`可遍历每一个程序头，并依次从中读取出`kernel`的内容:
 
 ```c
 readseg(ph->p_pa, ph->p_memsz, ph->p_offset)
 ```

#### Exercise 4

>Read about programming with pointers in C. The best reference for the C language is The C Programming Language by Brian Kernighan and Dennis Ritchie (known as 'K&R'). We recommend that students purchase this book (here is an [Amazon Link](http://www.amazon.com/C-Programming-Language-2nd/dp/0131103628/sr=8-1/qid=1157812738/ref=pd_bbs_1/104-1502762-1803102?ie=UTF8&s=books)) or find one of [MIT's 7 copies](http://library.mit.edu/F/AI9Y4SJ2L5ELEE2TAQUAAR44XV5RTTQHE47P9MKP5GQDLR9A8X-10422?func=item-global&doc_library=MIT01&doc_number=000355242&year=&volume=&sub_library=).

>Read 5.1 (Pointers and Addresses) through 5.5 (Character Pointers and Functions) in K&R. Then download the code for [pointers.c](http://pdosnew.csail.mit.edu/6.828/2014/labs/lab1/pointers.c), run it, and make sure you understand where all of the printed values come from. In particular, make sure you understand where the pointer addresses in lines 1 and 6 come from, how all the values in lines 2 through 4 get there, and why the values printed in line 5 are seemingly corrupted.

> There are other references on pointers in C (e.g., A tutorial by [Ted Jensen](http://pdosnew.csail.mit.edu/6.828/2014/readings/pointers.pdf) that cites K&R heavily), though not as strongly recommended.
> 
> Warning: Unless you are already thoroughly versed in C, do not skip or even skim this reading exercise. If you do not really understand pointers in C, you will suffer untold pain and misery in subsequent labs, and then eventually come to understand them the hard way. Trust us; you don't want to find out what "the hard way" is.

浏览了一遍K&R的指针部分，读了`pointers.c`，里面涉及的问题原来都见过的，这部分比较简单。主要值得注意的就是C中指针做加法时增加的大小为指针类型对应的单位长度。

#### Exercise 5

>Trace through the first few instructions of the boot loader again and identify the first instruction that would "break" or otherwise do the wrong thing if you were to get the boot loader's link address wrong. Then change the link address in `boot/Makefrag` to something wrong, run `make clean`, recompile the lab with `make`, and trace into the boot loader again to see what happens. Don't forget to change the link address back and `make clean` again afterward!

将link address改成了`0x7d00`，执行时发生了错误，
```
The target architecture is assumed to be i8086
[f000:fff0]    0xffff0:	ljmp   $0xf000,$0xe05b
0x0000fff0 in ?? ()
+ symbol-file obj/kern/kernel
(gdb) c
Continuing.

Program received signal SIGTRAP, Trace/breakpoint trap.
[   0:7c2d] => 0x7c2d:	ljmp   $0x8,$0x7d32
0x00007c2d in ?? ()
(gdb) 
```
从`0x7c00`开始单步执行，中间出现了这三处跳转：
```
...
[   0:7c0e] => 0x7c0e:	jne    0x7c0a
0x00007c0e in ?? ()
(gdb) 
...
[   0:7c18] => 0x7c18:	jne    0x7c14
0x00007c18 in ?? ()
(gdb) 
...
[   0:7c2d] => 0x7c2d:	ljmp   $0x8,$0x7d32
0x00007c2d in ?? ()
(gdb) 
...
```

其中前两处与link address设置为`0x7c00`时一致，因为这里使用的条件跳转指令为相对跳转，与link address的值无关，不会受到影响；而相反的，执行到第三条跳转`ljmp`时发生了错误，这是因为`ljmp`是利用其后两个参数跳转到某一绝对的地址上，此时如果link address与load address不一致了，那么跳转的目标地址也是错误的。

### Part 3: The Kernel

#### Exercise 6

>We can examine memory using GDB's `x` command. The [GDB manual](https://sourceware.org/gdb/current/onlinedocs/gdb/Memory.html#Memory) has full details, but for now, it is enough to know that the command `x/Nx ADDR` prints N words of memory at ADDR. (Note that both 'x's in the command are lowercase.) Warning: The size of a word is not a universal standard. In GNU assembly, a word is two bytes (the 'w' in xorw, which stands for word, means 2 bytes).

>Reset the machine (exit QEMU/GDB and start them again). Examine the 8 words of memory at 0x00100000 at the point the BIOS enters the boot loader, and then again at the point the boot loader enters the kernel. Why are they different? What is there at the second breakpoint? (You do not really need to use QEMU to answer this question. Just think.)

结果会不同，从`kernel.asm`可知`0xf0100000`是`kernel`的代码的起始处。在之前的练习中`0x0010000c`为`kernel`第一条语句的link address，其对应的load address为`0xf010000c`。
```
f0100000:	02 b0 ad 1b 00 00    	add    0x1bad(%eax),%dh
f0100006:	00 00                	add    %al,(%eax)
f0100008:	fe 4f 52             	decb   0x52(%edi)
f010000b:	e4 66                	in     $0x66,%al

f010000c <entry>:
f010000c:	66 c7 05 72 04 00 00 	movw   $0x1234,0x472
f0100013:	34 12 
f0100015:	b8 00 00 11 00       	mov    $0x110000,%eax
f010001a:	0f 22 d8             	mov    %eax,%cr3
f010001d:	0f 20 c0             	mov    %cr0,%eax
```

由此可知`0x00100000`为`kernel`的起始位置的link address，其后开始8个字应该为`kernel`代码开始的8个字（这里按照gdb的默认值，认为4bytes为1word，参考[GDB Manual](https://sourceware.org/gdb/current/onlinedocs/gdb/Memory.html#Memory)），即
```
02 b0 ad 1b 00 00 00 00 fe 4f 52 e4 66 c7 05 72
04 00 00 34 12 b8 00 00 11 00 0f 22 d8 0f 20 c0
```
其中注意`f010000b`后的第二个字节`66`与`f010000c`后的第一个字节`66`为同一字节，只能算一次。

按小端法以字为单位组合：
```
0x1badb002 0x00000000 0xe4524ffe 0x7205c766
0x34000004 0x0000b812 0x220f0011 0xc0200fd8
```

下面用gdb设置断点运行验证猜想：
```
(gdb) break *0x7c00
Breakpoint 1 at 0x7c00
(gdb) break *0x10000c
Breakpoint 2 at 0x10000c
(gdb) c
Continuing.
[   0:7c00] => 0x7c00:	cli    

Breakpoint 1, 0x00007c00 in ?? ()
(gdb) x/8x 0x00100000
0x100000:	0x00000000	0x00000000	0x00000000	0x00000000
0x100010:	0x00000000	0x00000000	0x00000000	0x00000000
(gdb) c
Continuing.
The target architecture is assumed to be i386
=> 0x10000c:	movw   $0x1234,0x472

Breakpoint 2, 0x0010000c in ?? ()
(gdb) x/8x 0x00100000
0x100000:	0x1badb002	0x00000000	0xe4524ffe	0x7205c766
0x100010:	0x34000004	0x0000b812	0x220f0011	0xc0200fd8
(gdb) 

```
最后的结果和之前完全一致，猜想得证。

#### Exercise 7

>Use QEMU and GDB to trace into the JOS kernel and stop at the `movl %eax, %cr0`. Examine memory at 0x00100000 and at 0xf0100000. Now, single step over that instruction using the `stepi` GDB command. Again, examine memory at 0x00100000 and at 0xf0100000. Make sure you understand what just happened.

>What is the first instruction after the new mapping is established that would fail to work properly if the mapping weren't in place? Comment out the `movl %eax, %cr0` in `kern/entry.S`, trace into it, and see if you were right.

```c
The target architecture is assumed to be i386
=> 0x100025:	mov    %eax,%cr0

Breakpoint 1, 0x00100025 in ?? ()
(gdb) x/8x 0x00100000 
0x100000:	0x1badb002	0x00000000	0xe4524ffe	0x7205c766
0x100010:	0x34000004	0x0000b812	0x220f0011	0xc0200fd8
(gdb) x/8x 0xf0100000 
0xf0100000 <_start+4026531828>:	0xffffffff	0xffffffff	0xffffffff	0xffffffff
0xf0100010 <entry+4>:	0xffffffff	0xffffffff	0xffffffff	0xffffffff
```

```c
(gdb) si
=> 0x100028:	mov    $0xf010002f,%eax
0x00100028 in ?? ()
(gdb) x/8x 0x00100000 
0x100000:	0x1badb002	0x00000000	0xe4524ffe	0x7205c766
0x100010:	0x34000004	0x0000b812	0x220f0011	0xc0200fd8
(gdb) x/8x 0xf0100000 
0xf0100000 <_start+4026531828>:	0x1badb002	0x00000000	0xe4524ffe	0x7205c766
0xf0100010 <entry+4>:	0x34000004	0x0000b812	0x220f0011	0xc0200fd8
(gdb) 
```
对比可以看出执行`movl %eax, %cr0`语句前，`0xf0100000`后的内容为某种默认值，与`0x00100000`后的内容不同；而执行后两处的却变得相同了。

这是因为
```
    # Turn on paging.
	movl	%cr0, %eax
	orl	$(CR0_PE|CR0_PG|CR0_WP), %eax
	movl	%eax, %cr0
```
这段代码打开了paging机制，虚拟内存地址被映射到物理内存空间。

在`kern/entrypgdir.c`中定义用`[KERNBASE, KERNBASE+4MB)`的虚拟地址映射物理地址`[0, 4MB)`的空间。`KERNBASE`在`inc/memlayout.h`中定义为`0xF0000000`。

因此在这句话执行后，`0xf0100000`后的地址也自然也被映射到了`0x00100000`后。

尝试注释掉`movl %eax, %cr0`语句后，`0xf0100000`后的内容前后没有改变，且当继续执行时会因为访问了无效的空间范围而出错：
```c
Program received signal SIGTRAP, Trace/breakpoint trap.
The target architecture is assumed to be i386
=> 0xf010002c <relocated>:	(bad)  
relocated () at kern/entry.S:74
74		movl	$0x0,%ebp			# nuke frame pointer
```

#### Exercise 8

>We have omitted a small fragment of code - the code necessary to print octal numbers using patterns of the form "%o". Find and fill in this code fragment.

将`lib/printfmt.c`中的`void vprintfmt(...);`函数中一处`switch`语句的`case 'o'`下更改为：
```
case 'o':
	num = getuint(&ap, lflag);
	base = 8;
	goto number;
```

`make qemu`运行可得到结果：
```
6828 decimal is 15254 octal!
```

---

>Be able to answer the following questions:

- Explain the interface between `printf.c` and `console.c`. Specifically, what function does `console.c` export? How is this function used by printf.c?

 `console.c`实现了一些与向显示器等硬件进行交互的函数，并留有封装好的输入输出接口`getchar()`与`cputchar()`，方便外部调用。其中`printf.c`在其`putch()`函数中使用`cputchar()`来实现输出：
 ```c
 // kern/printf.c
static void
putch(int ch, int *cnt)
{
	  cputchar(ch);
	  *cnt++;
}
 ```

 ```
 // kern/console.c
void
cputchar(int c)
{
  	  cons_putc(c);
}
 ```

- Explain the following from console.c:
```
1      if (crt_pos >= CRT_SIZE) {
2              int i;
3              memcpy(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
4              for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
5                      crt_buf[i] = 0x0700 | ' ';
6              crt_pos -= CRT_COLS;
7      }
```

其中`CRT_COLS`在`kern/console.h`中定义，表示一行有多少列（即多少个字符）。因此这段代码的作用就是当屏幕已经满了的时候，将最上一行舍弃，之后每一行的内容复制到上一行中，然后最后一行清空。

-  For the following questions you might wish to consult the notes for Lecture 2. These notes cover GCC's calling convention on the x86.
Trace the execution of the following code step-by-step:
 ```
int x = 1, y = 3, z = 4;
cprintf("x %d, y %x, z %d\n", x, y, z);
 ```
 - In the call to `cprintf()`, to what does  `fmt` point? To what does `ap` point?
 `fmt`指向"x %d, y %x, z %d\n"的地址，`ap`可能有的第二个参数，这里即为`x`的地址。
 
 - List (in order of execution) each call to `cons_putc`, `va_arg`, and `vcprintf`. For `cons_putc`, list its argument as well. For `va_arg`, list what `ap` points to before and after the call. For `vcprintf` list the values of its two arguments.
 ```
=>cprintf
=>vcprintf("x %d, y %x, z %d\n", 12(%ebp))
=>cons_putc('x')
=>cons_putc(32)
=>va_arg (uint32_t *)ebp+3->(uint32_t *)ebp+4
=>cons_putc('1')
=>cons_putc(',')
=>cons_putc(32)
=>cons_putc('y')
=>cons_putc(' ')
=>va_arg (uint32_t *)ebp+4->(uint32_t *)ebp+5
=>cons_putc('3')
=>cons_putc(',')
=>cons_putc(' ')
=>cons_putc('z')
=>cons_putc(' ')
=>va_arg (uint32_t *)ebp+5->(uint32_t *)ebp+6
=>cons_putc('4')
=>cons_putc('\n')
 ```

- Run the following code.
 ```
    unsigned int i = 0x00646c72;
    cprintf("H%x Wo%s", 57616, &i);
 ```
What is the output? Explain how this output is arrived at in the step-by-step manner of the previous exercise. [Here's an ASCII table](http://web.cs.mun.ca/~michael/c/ascii-table.html) that maps bytes to characters.
The output depends on that fact that the x86 is little-endian. If the x86 were instead big-endian what would you set i to in order to yield the same output? Would you need to change 57616 to a different value?

 [Here's a description of little- and big-endian](http://www.webopedia.com/TERM/b/big_endian.html) and [a more whimsical description](http://www.networksorcery.com/enp/ien/ien137.txt).
 
 输出是`He110 World`，
其中`57616 = 0xe110`，`i`小尾：`0x72`、`0x6c`、`0x64`、`0x00`
执行过程：
 ```
=>cprintf
=>vcprintf("H%x Wo%s", 12(%ebp))
=>cons_putc('H')
=>va_arg (uint32_t *)ebp+3->(uint32_t *)ebp+4
=>cons_putc('e')
=>cons_putc('1')
=>cons_putc('1')
=>cons_putc('0')
=>cons_putc(' ')
=>cons_putc('W')
=>cons_putc('o')
=>va_arg (uint32_t *)ebp+4->(uint32_t *)ebp+5
=>cons_putc(114)   // 0x72 'r'
=>cons_putc(108)   // 0x6c 'l'
=>cons_putc(100)   // 0x64 'd'
 ```

 若使用大尾序，只需将`i`改为'0x726c6400'，`57616`无需更改。

- In the following code, what is going to be printed after '`y=`'? (note: the answer is not a specific value.) Why does this happen?
  ```
    cprintf("x=%d y=%d", 3);
  ```
读取完3后`ap`指向`(uint32_t *)ebp+4`，在处理到`%d`时会再次调用`va_arg`，这是会将`ap`现在所指的位置的内容输出出来，具体是什么不得而知。

- Let's say that GCC changed its calling convention so that it pushed arguments on the stack in declaration order, so that the last argument is pushed last. How would you have to change cprintf or its interface so that it would still be possible to pass it a variable number of arguments?

 要实现这一点必须要能够倒着读传入栈中的参数，这就需要对`va_arg`和`va_start`进行改写。`JOS2014`中的`va_arg`声明为：
 ```
 #define va_arg(ap, type) __builtin_va_arg(ap, type)
 ```
无法直接修改，考虑重写`va_arg`。在[网上](http://research.microsoft.com/en-us/um/redmond/projects/invisible/include/stdarg.h.htm)找到`va_arg`的一种可能的实现：
 ```c
 > #define va_arg(_ap_, _type_) \
  ((_ap_ = (char *) ((__alignof__ (_type_) > 4 \
                       ? __ROUND((int)_ap_,8) : __ROUND((int)_ap_,4)) \
                     + __ROUND(sizeof(_type_),4))), \
   *(_type_ *) (void *) (_ap_ - __ROUND(sizeof(_type_),4)))
 ```
简化之后可以写为：
 ```c
 > #define va_arg(ap,t) \
     (*(t *)((ap += __va_size(t)) - __va_size(t)))
 ```
 若要实现倒序访问，可将其修改为：
  ```c
 > #define va_arg(ap,t) \
     (*(t *)((ap -= __va_size(t)) + __va_size(t)))
 ```
 此外，需要重写`va_start`，使其在初始化时指向最后一个参数：
 ```c
 > #define     va_start(ap, last) \
         ((ap) = (va_list)&(last) - __va_size(last))
 ```

##### Challenge

>Enhance the console to allow text to be printed in different colors. The traditional way to do this is to make it interpret [ANSI escape sequences](http://rrbrandt.dee.ufcg.edu.br/en/docs/ansi/) embedded in the text strings printed to the console, but you may use any mechanism you like. There is plenty of information on [the 6.828 reference page](http://pdosnew.csail.mit.edu/6.828/2014/reference.html) and elsewhere on the web on programming the VGA display hardware. If you're feeling really adventurous, you could try switching the VGA hardware into a graphics mode and making the console draw text onto the graphical frame buffer.

利用问题中给的 [ANSI escape sequences](http://rrbrandt.dee.ufcg.edu.br/en/docs/ansi/)即可完成。

作为演示修改了`monitor.c`中的内容：

```
cprintf("\033[31mWelcome \033[32mto \033[33mthe \033[34mJOS \033[35mkernel \033[36mmonitor!\033[0m\n");
```

![enter image description here](http://ww2.sinaimg.cn/large/6313a6d8jw1eq1ooh800xj20g403kwex.jpg)

#### Exercise 9

>Determine where the kernel initializes its stack, and exactly where in memory its stack is located. How does the kernel reserve space for its stack? And at which "end" of this reserved area is the stack pointer initialized to point to?

`kernel`初始化栈的语句在`entry.S`中：
```
	# Set the stack pointer
	movl	$(bootstacktop),%esp
```	
`bootstacktop`也在`entry.S`中定义了：
```
###################################################################
# boot stack
###################################################################
	.p2align	PGSHIFT		# force page alignment
	.globl		bootstack
bootstack:
	.space		KSTKSIZE
	.globl		bootstacktop   
bootstacktop:
```
空间的预留靠`.space	KSTKSIZE`实现。

栈是由高地址向低地址生长，因此栈顶指针初始化时指向较高的一端，利用`gdb`可以得知其初始值为`0xf0110000`。

```
=> 0xf0100034 <relocated+5>:	mov    $0xf0110000,%esp
relocated () at kern/entry.S:77
77		movl	$(bootstacktop),%esp
(gdb) si
=> 0xf0100039 <relocated+10>:	call   0xf0100094 <i386_init>
80		call	i386_init
(gdb) p $esp
$1 = (void *) 0xf0110000 <entry_pgdir>
(gdb) 
```


#### Exercise 10

>To become familiar with the C calling conventions on the x86, find the address of the `test_backtrace` function in `obj/kern/kernel.asm`, set a breakpoint there, and examine what happens each time it gets called after the kernel starts. How many 32-bit words does each recursive nesting level of `test_backtrace` push on the stack, and what are those words?

>Note that, for this exercise to work properly, you should be using the patched version of QEMU available on the [tools](http://pdosnew.csail.mit.edu/6.828/2014/tools.html) page or on Athena. Otherwise, you'll have to manually translate all breakpoint and memory addresses to linear addresses.

在`obj/kern/kernel.asm`中找到`test_backtrace`，可知其起始地址为`0xf0100040`：
```
   70: f0100040 <test_backtrace>:
   71  #include <kern/console.h>
   72  
   73  // Test the stack backtrace function (lab 1 only)
   74  void
   75: test_backtrace(int x)
   76  {
   77  f0100040:	55            	push   %ebp
   ..
```

在`obj/kern/kernel.asm`中找到调用`test_backtrace`的指令地址`0xf01000cf`，在此处设置断点后查看此时的栈指针`esp =  0xf010ffe0`。

在`0xf0100040`处设置断点，反复执行几次之后用`gdb`打出栈内内容：（其中每**8个32-bit words**为1次`test_backtrace`调用后放入栈中的内容）
```
(gdb) c
Continuing.
=> 0xf0100040 <test_backtrace>:	push   %ebp

Breakpoint 2, test_backtrace (x=0) at kern/init.c:13
13	{
(gdb) si
=> 0xf0100041 <test_backtrace+1>:	mov    %esp,%ebp
0xf0100041	13	{
(gdb) x/48x $esp
0xf010ff58:	0xf010ff78	0xf0100068	0x00000001	0x00000002
0xf010ff68:	0xf010ff98	0x00000000	0xf010089d	0x00000003
0xf010ff78:	0xf010ff98	0xf0100068	0x00000002	0x00000003
0xf010ff88:	0xf010ffb8	0x00000000	0xf010089d	0x00000004
0xf010ff98:	0xf010ffb8	0xf0100068	0x00000003	0x00000004
0xf010ffa8:	0x00000000	0x00000000	0x00000000	0x00000005
0xf010ffb8:	0xf010ffd8	0xf0100068	0x00000004	0x00000005
0xf010ffc8:	0x00000000	0x00010094	0x00010094	0x00010094
0xf010ffd8:	0xf010fff8	0xf01000d4	0x00000005	0x00001aac
0xf010ffe8:	0x00000684	0x00000000	0x00000000	0x00000000
0xf010fff8:	0x00000000	0xf010003e	0x00111021	0x00000000
0xf0110008 <entry_pgdir+8>:	0x00000000	0x00000000	0x00000000	0x00000000
```
每两行为1次调用产生的内容，可以看出每两行的第一个值（`%ebp`）为刚push进栈的（调用链中上一层函数的）帧指针的值；第三个值（`%ebp+8`）为此次调用时传入的参数，其值最开始为5，每次调用减少1，直到0时返回。

继续在`kernel.asm`中确定每个地址对应位置

```
   93  f0100062:	50               push   %eax
   94: f0100063:	e8 d8 ff ff ff   call   f0100040 <test_backtrace>
   95  f0100068:	83 c4 10         add    $0x10,%esp

...

  150  f01000c8:	c7 04 24 05 00 00 00 	movl   $0x5,(%esp)
  151: f01000cf:	e8 6c ff ff ff       	call   f0100040 <test_backtrace>
  152  f01000d4:	83 c4 10             	add    $0x10,%esp	

```
栈中第二列（`%ebp+4`）的`0xf0100068`和`0xf01000d4`皆为这一层`test_backtrace`的返回地址，前者为`test_backtrace`内的迭代调用（语句的下一句，下同），后者为`i386_init`函数中的调用。

其他几个位置的值则是`test_backtrace`在执行过程中压入栈中的内容（如`%ebx`）。

#### Exercise 11

>Implement the backtrace function as specified above. Use the same format as in the example, since otherwise the grading script will be confused. When you think you have it working right, run `make grade` to see if its output conforms to what our grading script expects, and fix it if it doesn't. After you have handed in your Lab 1 code, you are welcome to change the output format of the backtrace function any way you like.

>If you use `read_ebp()`, note that GCC may generate "optimized" code that calls `read_ebp()` before `mon_backtrace()`'s function prologue, which results in an incomplete stack trace (the stack frame of the most recent function call is missing). While we have tried to disable optimizations that cause this reordering, you may want to examine the assembly of `mon_backtrace()` and make sure the call to `read_ebp()` is happening after the function prologue.

```
Stack backtrace:
  ebp f0109e58  eip f0100a62  args 00000001 f0109e80 f0109e98 f0100ed2 00000031
  ebp f0109ed8  eip f01000d6  args 00000000 00000000 f0100058 f0109f28 00000061
  ...
```
其中`ebp`为当前函数调用的帧指针，`eip`为函数返回地址（即调用当前函数的语句之后的一条语句），`args`为传给当前函数的参数。

`guide`中的两个问题：
> The return instruction pointer typically points to the instruction after the call instruction (why?)

返回地址指向调用指令的下一行是因为在体系结构的设计中，每次CPU完成一次取值会立即令`PC`指向下一条语句，此后这个`PC`会被压入栈作为返回地址。（实际上也应该如此，反之若压入栈的为调用语句，则会无限循环调用该函数）

>Why can't the backtrace code detect how many arguments there actually are? How could this limitation be fixed?

`backtrace`无法确定参数的数量是因为这个函数并不明白栈中数值的意义，也不知道调用它的函数实际上传给它了几个参数。要修正这个限制，可以规定在设置好参数后，再向栈中传一个magic number来表示参数列表的结束；或者在传入参数前先传入参数数量也可以解决这个问题。

至于循环输出`backtrace`的边界，可以由**Exercise 10**中输出的栈中内容看出，`kernel`代码的最外层`ebp`为`0x00000000`。实现代码：

```
int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t ebp, eip, args[5];

	cprintf("Stack backtrace:\n");

	for (ebp = read_ebp(); ebp != 0; ebp = *(uintptr_t *)ebp) {
		eip = *((uintptr_t *)ebp + 1);
		args[0] = *((uintptr_t *)ebp + 2);
		args[1] = *((uintptr_t *)ebp + 3);
		args[2] = *((uintptr_t *)ebp + 4);
		args[3] = *((uintptr_t *)ebp + 5);
		args[4] = *((uintptr_t *)ebp + 6);
		cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n", ebp, eip, args[0], args[1], args[2], args[3], args[4]);
	}
	return 0;
}
```

最后将其加入到"the kernel monitor's command list"中，即可在JOS运行时用`backtrace`指令查看stack backtrace：
```
static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display stack backtrace", mon_backtrace },
};
```

#### Exercise 12

>Modify your stack backtrace function to display, for each eip, the function name, source file name, and line number corresponding to that eip.

为了输出`eip`的调试信息，首先去`kern/kdebug.h`中查看定义：

```c
// Debug information about a particular instruction pointer
struct Eipdebuginfo {
	const char *eip_file;		// Source code filename for EIP
	int eip_line;			// Source code linenumber for EIP

	const char *eip_fn_name;	// Name of function containing EIP
					//  - Note: not null terminated!
	int eip_fn_namelen;		// Length of function name
	uintptr_t eip_fn_addr;		// Address of start of function
	int eip_fn_narg;		// Number of function arguments
};

int debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info);
```

先在`debuginfo_eip`中添加`stab_binsearch`以搜索行号：

```
stab_binsearch(stabs, &lline, &rline, N_SLINE, addr);
if (lline > rline)
	return -1;
info->eip_line = stabs[lline].n_desc;
```

之后再`monitor.c`中修改相应部分即可：

```
int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	struct Eipdebuginfo eipinfo;
	uintptr_t ebp, eip, args[5];

	cprintf("Stack backtrace:\n");

	for (ebp = read_ebp(); ebp != 0; ebp = *(uintptr_t *)ebp) {
		eip = *((uintptr_t *)ebp + 1);
		debuginfo_eip(eip, &eipinfo);
		args[0] = *((uintptr_t *)ebp + 2);
		args[1] = *((uintptr_t *)ebp + 3);
		args[2] = *((uintptr_t *)ebp + 4);
		args[3] = *((uintptr_t *)ebp + 5);
		args[4] = *((uintptr_t *)ebp + 6);
		cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n", 
			ebp, eip, args[0], args[1], args[2], args[3], args[4]);
		cprintf("         %s:%d: %.*s+%d\n", 
			eipinfo.eip_file, eipinfo.eip_line, eipinfo.eip_fn_namelen, 
			eipinfo.eip_fn_name, eip - eipinfo.eip_fn_addr);
	}
	return 0;
}
```

## 遇到的困难以及解决方法

最开始是编译遇到了问题，之后参考[这里](https://bugs.launchpad.net/qemu/+bug/1193628)，在`Makefile.target`中201行下面加入了一行`LIBS+=-lrt -lm`得以解决。

刚开始做Lab就在**Exercise 2**卡住了，虽然题目说了明白大概实在做什么就行，但是还是想去看一下具体在做什么。第一次看的时候读到开头转入保护模式的代码，就以为这就进入了Bootloader。但是越往后面执行越不对，而且地址也没有转到[0:0x7c00]。最后以比较大的跨度(`si 10000`)运行了半天，观察了指令的规律，并在网上查了很久的资料才明白这一大段都在做什么。

**Exercise 2**之后的Exercise都比较顺利了，里面很多问题在之前其他课程的学习中也或多或少涉及过，并没有多大的困难。

## 收获及感想

这次lab虽然在实际需要写代码的内容不多，但内容含量还是很大的，花了意外多的时间。不过收货也很大。之前对操作系统启动的过程只有一个大致的概念，这次通过`gdb`的调试以及对相关源码的阅读，对booting的过程有了一个比较清晰的认识了。

## 对课程的意见和建议

这次lab虽然只有一周的时间，但是做起来还是挺费时间的，以后可以建议同学们早点动手，不要像我一样拖到最后一天。·

## 参考文献
[1] [Bug #1193628 “Undefined References” : Bugs : QEMU](https://bugs.launchpad.net/qemu/+bug/1193628)
[2] [Control register - Wikipedia, the free encyclopedia](http://en.wikipedia.org/wiki/Control_register)
[3] [CMOS - OSDev Wiki](http://wiki.osdev.org/CMOS)
[4] [Bochs Developers Guide](http://bochs.sourceforge.net/doc/docbook/development/cmos-map.html)
[5] [A20 - a pain from the past](http://www.win.tue.nl/~aeb/linux/kbd/A20.html)
[6] [Memory - Debugging with GDB](https://sourceware.org/gdb/current/onlinedocs/gdb/Memory.html#Memory)
[7] [stdarg.h Source](http://research.microsoft.com/en-us/um/redmond/projects/invisible/include/stdarg.h.htm) from Microsoft
[8] [Stab Section Basics - STABS](https://sourceware.org/gdb/onlinedocs/stabs/Stab-Section-Basics.html#Stab-Section-Basics)

以及其他Lab1中所提供的参考资料