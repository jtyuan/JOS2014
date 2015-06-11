# JOS Lab 5 Report<br/><small><small><small><small>江天源 1100016614</small></small></small></small>

## 总体概述


### 完成情况

|#|E1|E2|E3|E4|E5|E6|E7|E8|E9|E10|
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|Status|√|√|√|√|√|√|√|√|√|√|

|#|Q1|C1|C2|C3|C4|C5|C6|C7|
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|Status|√|×|×|×|√|×|×|√|

<small>* 其中E#代表Exercise #, Q#代表Question #, C# 代表Challenge #</small>

共完成了2个Challenge。

`make grade`结果

![](http://ww3.sinaimg.cn/large/6313a6d8jw1esj2vn3vzyj20wk0myah5.jpg =600x)

### Part A: User Environments and Exception Handling

#### Exercise 1

>`i386_init` identifies the file system environment by passing the type `ENV_TYPE_FS` to your environment creation function, `env_create`. Modify `env_create` in `env.c`, so that it gives the file system environment I/O privilege, but never gives that privilege to any other environment.

在`env_create`最后添加代码，在创建的环境为file server的情况下，给予其I/O权限：

```java
if (type == ENV_TYPE_FS)
   e->env_tf.tf_eflags |= FL_IOPL_3;
```

#### Question

> Do you have to do anything else to ensure that this I/O privilege setting is saved and restored properly when you subsequently switch from one environment to another? Why?

不需要，在environment的switching过程中，I/O privilege由硬件进行保存，并通过`env_pop_tf`的`iret`指令恢复。

#### Exercise 2

>Implement the `bc_pgfault` and `flush_block` functions in `fs/bc.c`. `bc_pgfault` is a page fault handler, just like the one your wrote in the previous lab for copy-on-write fork, except that its job is to load pages in from the disk in response to a page fault. When writing this, keep in mind that (1) `addr` may not be aligned to a block boundary and (2) `ide_read` operates in sectors, not blocks.

`bc_pgfault`是block cache的一个page fault处理函数，它需要做的事情就是在基本的变量检查之后，在内存中分配一个页，并将磁盘中对应的文件读入其中；最后需要清dirty位，这一步用自身到自身的`sys_page_map`即可完成。

```java
static void
bc_pgfault(struct UTrapframe *utf)
{
   void *addr = (void *) utf->utf_fault_va;
   uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
   int r;

   // Check that the fault was within the block cache region
   if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
      panic("page fault in FS: eip %08x, va %08x, err %04x",
            utf->utf_eip, addr, utf->utf_err);

   // Sanity check the block number.
   if (super && blockno >= super->s_nblocks)
      panic("reading non-existent block %08x\n", blockno);

   addr = ROUNDDOWN(addr, PGSIZE);
   if ((r = sys_page_alloc(0, addr, PTE_P | PTE_U | PTE_W)) < 0)
      panic("in bc_pgfault, sys_page_alloc: %e", r);

   if ((r = ide_read(blockno*BLKSECTS, addr, BLKSECTS)) < 0)
      panic("in bc_pgfault, ide_read: %e", r);

   // Clear the dirty bit for the disk block page since we just read the
   // block from disk
   if ((r = sys_page_map(0, addr, 0, addr, 
      uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
      panic("in bc_pgfault, sys_page_map: %e", r);

   // Check that the block we read was allocated. (exercise for
   // the reader: why do we do this *after* reading the block
   // in?)
   if (bitmap && block_is_free(blockno))
      panic("reading free block %08x\n", blockno);
}
```

将内存中`addr`所在的一个标记了dirty的block写回硬盘；进行完基本的检查和dirty位的检查后，调用`ide_write`将内容写回硬盘，最后再利用`sys_page_map`清掉dirty位即可。

```java
void
flush_block(void *addr)
{
   uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
   int r;

   if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
      panic("flush_block of bad va %08x", addr);

   // LAB 5: Your code here.
   if (!(va_is_mapped(addr) && va_is_dirty(addr)))
      return;

   addr = ROUNDDOWN(addr, PGSIZE);

   if ((r = ide_write(blockno*BLKSECTS, addr, BLKSECTS)) < 0)
      panic("in flush_block, ide_write: %e", r);

   if ((r = sys_page_map(0, addr, 0, addr, 
      uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
      panic("in flush_block, sys_page_map: %e", r);
   // panic("flush_block not implemented");
}
```


#### Exercise 3

>Use `free_block` as a model to implement `alloc_block`, which should find a free disk block in the bitmap, mark it used, and return the number of that block. When you allocate a block, you should immediately flush the changed bitmap block to disk with `flush_block`, to help file system consistency.

参考`free_block`中的语句`bitmap[blockno/32] |= 1<<(blockno%32);`，可以知道

>1. bitmap中`0`表示正在使用，`1`表示空闲
2. 只需将对应的bitmap位并一个只有某一位为0的mask即可表示某一位被分配

剩下的实现都水到渠成了：

```java
int
alloc_block(void)
{
   int i;

   for (i = 0; i < super->s_nblocks; i++) {
      if (block_is_free(i)) {
         // mark the blockno == i in-use
         bitmap[i / 32] &= ~(1 << ( i %32));
         flush_block(diskaddr(i / BLKBITSIZE + 2));
         return i;
      }
   }

   // panic("alloc_block not implemented");
   return -E_NO_DISK;
}
```

其中值得注意的是，bitmap前还有两个块作为`boot sector`和`super block`，所以将改变了的bitmap block写回硬盘时，需要`+2`。

#### Exercise 4

>Implement `file_block_walk` and `file_get_block`. `file_block_walk` maps from a block offset within a file to the pointer for that block in the struct File or the indirect block, very much like what `pgdir_walk` did for page tables. `file_get_block` goes one step further and maps to the actual disk block, allocating a new one if necessary.

`file_block_walk`：找到`filebno`对应的文件块编号，并保存在`ppdiskbno`中；分`Direct`和`Indirect`两种情形，前者直接在File结构的直接索引数组中找到对应的结果返回即可。后者在文件尚无间接索引的情况下，根据`alloc`参数的情况决定是否为间接索引新分配空间；在间接索引存在(或者新分配)的情况下，将对应块编号放到`ppdiskbno`中即可。

```java
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
   uint32_t blockno;

   if (filebno < NDIRECT)
      *ppdiskbno = &f->f_direct[filebno];
   else if (filebno < NDIRECT + NINDIRECT) {
      if (!f->f_indirect) {
         if (alloc) {
            if ((blockno = alloc_block()) < 0)
               return -E_NO_DISK;
            memset(diskaddr(blockno), 0, BLKSIZE);
            f->f_indirect = blockno;
         } else
            return -E_NOT_FOUND;
      }
      *ppdiskbno = &((uintptr_t *) 
                  diskaddr(f->f_indirect))[filebno - NDIRECT];
   } else {
      return -E_INVAL;
   }

   return 0;
}
```

`file_get_block`：找到对应的文件块地址。先利用刚才实现的`file_block_walk`能找到对应的块编号，如果那块还没有分配空间，则进行分配；最后利用`diskaddr`找到块的地址，并保存在`blk`中即可。

```java
int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{
       // LAB 5: Your code here.
       // panic("file_get_block not implemented");
   int r;
   uint32_t *pdiskbno;
   uint32_t blkno;

   if ((r = file_block_walk(f, filebno, &pdiskbno, true)) < 0)
      return r;

   if (!*pdiskbno) {
      if ((blkno = alloc_block()) < 0)
         return -E_NO_DISK;
      *pdiskbno = blkno;
   }

   *blk = diskaddr(*pdiskbno);

   return 0;
}
```

#### Exercise 5

>Implement `serve_read` in `fs/serv.c`.

![](http://ww1.sinaimg.cn/large/6313a6d8jw1esj66aimtij20qc0gy0uh.jpg =480x)

上图是以读操作(`read`)为例的一般进程进行文件操作的执行过程。用户程序调用`read`函数，其中会调用对应设备的`devfile_read`函数来进行处理(`(*dev->dev_read)(fd, buf, n)`)。其中一般会调用一个叫`fsipc`的函数，跟系统的`file server`进行通信。`file server`会一直执行`serve`函数对设备的I/O请求进行轮询，当它发现有I/O请求时，会调用`serve_read`函数进行处理，并在其中调用文件系统自己的文件操作函数`file_read`来对磁盘文件进行操作。

`serve_read`的主要工作就是从`ipc`请求中获取到需要读的文件，并调用`file_read`将文件的内容读出并返回给`serve`，`serve`会则将结果返回给用户进程。

实现如下：

```java
int
serve_read(envid_t envid, union Fsipc *ipc)
{
   struct Fsreq_read *req = &ipc->read;
   struct Fsret_read *ret = &ipc->readRet;

   if (debug)
      cprintf("serve_read %08x %08x %08x\n", envid, 
         req->req_fileid, req->req_n);
`
   // Lab 5: Your code here:
   struct OpenFile *o;
   int r;

   if ((r = openfile_lookup(envid, req->req_fileid, &o)) < 0)
      return r;

   if ((r = file_read(o->o_file, ret->ret_buf, req->req_n, 
      o->o_fd->fd_offset)) < 0)
      return r;

   o->o_fd->fd_offset += r;

   return r;
}
```

#### Exercise 6

>Implement `serve_write` in `fs/serv.c` and `devfile_write` in `lib/file.c`.

参照刚才写的`serve_read`和已有的`devfile_read`函数即可实现，没有难度。

```java
int
serve_write(envid_t envid, struct Fsreq_write *req)
{
   if (debug)
      cprintf("serve_write %08x %08x %08x\n", envid, 
         req->req_fileid, req->req_n);

   // LAB 5: Your code here.
   struct OpenFile *o;
   int r;

   if ((r = openfile_lookup(envid, req->req_fileid, &o)) < 0)
      return r;

   if ((r = file_write(o->o_file, req->req_buf, req->req_n, 
      o->o_fd->fd_offset)) < 0)
      return r;

   o->o_fd->fd_offset += r;

   return r;
}
```

```java
static ssize_t
devfile_write(struct Fd *fd, const void *buf, size_t n)
{
   fsipcbuf.write.req_fileid = fd->fd_file.id;
   fsipcbuf.write.req_n = n < PGSIZE ? n : PGSIZE;

   memmove(fsipcbuf.write.req_buf, buf, fsipcbuf.write.req_n);

   return fsipc(FSREQ_WRITE, NULL);
}
```

#### Exercise 7

>`spawn` relies on the new syscall `sys_env_set_trapframe` to initialize the state of the newly created environment. Implement `sys_env_set_trapframe` in `kernel/syscall.c` (don't forget to dispatch the new system call in `syscall()`).

跟其他`env`相关的syscall差不多，设置好trapframe，并开启用户态的中断即可。

```java
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
   struct Env *env;
   if (envid2env(envid, &env, 1) < 0)
      return -E_BAD_ENV;

   env->env_tf = *tf;
   env->env_tf.tf_eflags |= FL_IF;

   return 0;
}
```

#### Challenge

>Implement Unix-style exec.

首先回答前面exercise中的一个问题，`spawn`与`fork`+`exec`的区别在于，后者执行`exec`时，需要子进程在自己的用户空间里完成新程序的加载于执行。

实现思路是在进程空间中用一块区域当做临时空间来加载新程序，然后再由进程自己进行映射和其他一些设置。

在`/lib`目录下添加`exec.c`，并在`inc/lib.c`中加入相应声明，在`lib/Makefrag`中`LIB_SRCFILES`后添加：

```
lib/exec.c \
```

`exec.c`中大部分与`spawn.c`一样。区别主要在`exec`函数中：

不`fork`子进程而是直接将新程序加载到`ETEMP`后，并在相邻的位置（根据加载所占的空间而定，而非固定的`USTACKTOP-PGSIZE`）初始化栈空间

```java
#define ETEMP        (UTEMP3 + PGSIZE)

int
exec(const char *prog, const char **argv)
{
   unsigned char elf_buf[512];
   // struct Trapframe child_tf;
   // envid_t child;
   uintptr_t tf_esp;
   uint32_t tmp_addr;

   int fd, i, r;
   struct Elf *elf;
   struct Proghdr *ph;
   int perm;

   if ((r = open(prog, O_RDONLY)) < 0)
      return r;
   fd = r;

   // Read elf header
   elf = (struct Elf*) elf_buf;
   if (readn(fd, elf_buf, sizeof(elf_buf)) != sizeof(elf_buf)
       || elf->e_magic != ELF_MAGIC) {
      close(fd);
      cprintf("elf magic %08x want %08x\n", elf->e_magic, ELF_MAGIC);
      return -E_NOT_EXEC;
   }
   
   // Set up program segments as defined in ELF header.
   tmp_addr = (uint32_t) ETEMP;
   ph = (struct Proghdr*) (elf_buf + elf->e_phoff);
   for (i = 0; i < elf->e_phnum; i++, ph++) {
      if (ph->p_type != ELF_PROG_LOAD)
         continue;
      perm = PTE_P | PTE_U;
      if (ph->p_flags & ELF_PROG_FLAG_WRITE)
         perm |= PTE_W;
      if ((r = map_segment(0, tmp_addr + PGOFF(ph->p_va), ph->p_memsz,
                 fd, ph->p_filesz, ph->p_offset, perm)) < 0)
         goto error;
      tmp_addr += ROUNDUP(PGOFF(ph->p_va) + ph->p_filesz, PGSIZE);
   }
   close(fd);
   fd = -1;

   if ((r = init_stack(0, argv, &tf_esp, tmp_addr + PGSIZE)) < 0)
      goto error;

   if ((r = sys_exec(elf->e_entry, tf_esp, 
      (void *)(elf_buf + elf->e_phoff), elf->e_phnum)) < 0)
      goto error;

   return 0;

error:
   sys_env_destroy(0);
   close(fd);
   return r;
}
```

最后调用新添加的系统调用`sys_exec`来开始执行：

```java
static int
sys_exec(uint32_t eip, uint32_t esp, void * _ph, uint32_t phnum) {

   int perm, i, r;
   uint32_t etmp;
   uint32_t vs, vt;
   struct Proghdr * ph;
   struct PageInfo * pg;

   curenv->env_tf.tf_eip = eip;
   curenv->env_tf.tf_esp = esp;

   ph = (struct Proghdr *) _ph; 
   etmp = (uint32_t) UTEMP + 3 * PGSIZE; // ETEMP
   
   for (i = 0; i < phnum; i++, ph++) {
      
      if (ph->p_type != ELF_PROG_LOAD)
         continue;

      perm = PTE_P | PTE_U;
      
      if (ph->p_flags & ELF_PROG_FLAG_WRITE)
         perm |= PTE_W;

      vt = ROUNDUP(ph->p_va + ph->p_memsz, PGSIZE);
      for (vs = ROUNDDOWN(ph->p_va, PGSIZE); vs < vt; 
         etmp += PGSIZE, vs += PGSIZE) {
         
         if ((pg = page_lookup(curenv->env_pgdir, 
            (void *) etmp, NULL)) == NULL) 
            return -E_NO_MEM;
         
         if ((r = page_insert(curenv->env_pgdir, pg, 
            (void *) vs, perm)) < 0)
            return r;
         
         page_remove(curenv->env_pgdir, (void *) etmp);
      }
   }

   if ((pg = page_lookup(curenv->env_pgdir, 
      (void *) etmp, NULL)) == NULL) 
      return -E_NO_MEM;
   
   if ((r = page_insert(curenv->env_pgdir, pg, 
      (void *) (USTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W)) < 0) 
      return r;
   
   page_remove(curenv->env_pgdir, (void *) etmp);

   env_run(curenv);
   return 0;
}
```

测试:

添加用户程序`user/exechello.c`：

```java
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
   int r;
   cprintf("i am parent environment %08x\n", thisenv->env_id);
   if ((r = execl("hello", "hello", 0)) < 0)
      panic("exec(hello) failed: %e", r);
}
```

结果：

![](http://ww1.sinaimg.cn/large/6313a6d8jw1esra45n632j20tm0ne7bh.jpg =600x)

可以看出两个environment的id都是`00001001`，即在同一个进程中完成了`exec`的执行。

#### Exercise 8

>Change `duppage` in `lib/fork.c` to follow the new convention. If the page table entry has the `PTE_SHARE` bit set, just copy the mapping directly. (You should use `PTE_SYSCALL`, not `0xfff`, to mask out the relevant bits from the page table entry. `0xfff` picks up the `accessed` and `dirty` bits as well.)

>Likewise, implement `copy_shared_pages` in `lib/spawn.c`. It should loop through all page table entries in the current process (just like `fork` did), copying any page mappings that have the `PTE_SHARE` bit set into the child process.

`duppage`中特殊`PTE_SHARE`位为真的情况，将两个`env`的对应页直接进行映射，并清掉`PTE_SHARE`位：

```java
if (uvpt[pn] & PTE_SHARE) {
   if ((r = sys_page_map(0, addr, envid, addr, 
      uvpt[pn] & PTE_SYSCALL)) < 0) 
      return r;
   return 0;
}
```

`copy_shared_pages`：将进程空间里所有的标记了`PTE_SHARE`的页映射到子进程的地址空间；枚举每一页，对页表进行权限位检查后进行映射即可：

```java
// Copy the mappings for shared pages into the child address space.
static int
copy_shared_pages(envid_t child)
{
   // LAB 5: Your code here.
   int r;
   void *addr;

   for (addr = 0; addr < (void *) USTACKTOP; addr += PGSIZE) {
      if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P)
         && (uvpt[PGNUM(addr)] & PTE_U) 
         && (uvpt[PGNUM(addr)] & PTE_SHARE))
         if ((r = sys_page_map(0, addr, child, addr, 
            uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0) 
            return r;
      
   }

   return 0;
}
```

#### Exercise 9

>In your `kern/trap.c`, call `kbd_intr` to handle trap `IRQ_OFFSET+IRQ_KBD` and `serial_intr` to handle trap `IRQ_OFFSET+IRQ_SERIAL`.

做法如题，毫无难度`_(:з」∠)_`。在`trap_dispatch`中加入以下两句：

```java
// Handle keyboard and serial interrupts.
// LAB 5: Your code here.
if (tf->tf_trapno == IRQ_OFFSET + IRQ_KBD) {
   kbd_intr();
   return;
}

if (tf->tf_trapno == IRQ_OFFSET + IRQ_SERIAL) {
   serial_intr();
   return;
}
```

#### Exercise 10

>The shell doesn't support I/O redirection. It would be nice to run `sh <script` instead of having to type in all the commands in the script by hand, as you did above. Add I/O redirection for `<` to `user/sh.c`.

因为shell原来已经实现了输出重定向`>`，所以照着写就行了`_(:з」∠)_`，唯一的区别是需要将`fd` `dup`给标准输入流`0`。

```java
case '<':   // Input redirection
   // Grab the filename from the argument list
   if (gettoken(0, &t) != 'w') {
      cprintf("syntax error: < not followed by word\n");
      exit();
   }

   if ((fd = open(t, O_RDONLY)) < 0) {
      cprintf("open %s for read: %e", t, fd);
      exit();
   }
   if (fd != 0) {
      dup(fd, 0);
      close(fd);
   }
   break;
```

#### Challenge

>Add more features to the shell. Possibilities include (a few require changes to the file system too):

>
   backgrounding commands (ls &)
   multiple commands per line (ls; echo hi)
   command grouping ((ls; echo hi) | cat > out)
   environment variable expansion (echo $hello)
   quoting (echo "a | b")
   command-line history and/or editing
   tab completion
   directories, cd, and a PATH for command-lookup.
   file creation
   ctl-c to kill the running environment

>
but feel free to do something not on this list.

实现了

- 后台命令 `(ls &)`
- 一行多命令 `(ls; echo hi)`
- 引号 `(echo "a | b")`
- shell的命令历史 (history)
- 文件创建 (touch)
- ctl-c杀死当前进程

----

- **后台命令 `(ls &)`**

`user/sh.c`在分析token已经把`&`和`;`加入到符号列表了：

```
#define SYMBOLS "<|>&;()"
```

只需在`runcmd`中加入对相应情况的处理即可。

在读token时发现读入了`&`，则在后面运行时父进程不等待spawn出来的子进程，而继续自己的执行。

- **一行多命令 `(ls; echo hi)`**

在判断存在`;`token的情况下，在执行完毕后进行处理：

```java
if (r > 0 && more) {
   if ((r = opencons()) < 0)
      panic("opencons: %e", r);
   if (r != 0)
      panic("first opencons used fd %d", r);
   if ((r = dup(0, 1)) < 0)
      panic("dup: %e", r);
   goto again;
}
```

因为之前有`closeall`操作，这里需要恢复到控制台的标准输入输出流。

- **引号 `(echo "a | b")`**

修改`_gettoken`函数。加入静态变量`inquote`标记当前处理的字串是否在一对引号之间，来进行相应的处理。

```java
...
// 一对引号的第二个
if (inquote && *s == '"') {
   inquote = 0;
   *s++ = 0;
}
...
// 一对引号的第一个
if (*s == '"') {
   (*p1)++;
   inquote = 1;
   do {
      s++;
   } while (*s && *s != '"');
}
while (*s && !strchr(WHITESPACE SYMBOLS, *s) && *s != '"')
   s++;
...
```

**前三项的测试**：

![](http://ww2.sinaimg.cn/large/6313a6d8jw1esr8x1igqhj20vk0owtbq.jpg =600x)

其中包含了`;`，`&`，`"`三个部分的测试。`echo "_(:3/_)_"指令由于在后面加了&`，所以其结果在`sh`输出了`$`后才输出。

- **shell的命令历史 (history)**

将shell命令的历史保存进文件`.history`（同时修改了`ls`使其默认不会输出名字以`.`开头的文件）。shell每次执行一个指令都会将其加在`.history`的最后一行。另外还加入了`history`指令，来读取`.history`中的内容并显示。

写历史记录，由于JOS打开文件没有append的选项，因此需要手动读取文件状态，来找到其末尾：

```java
void write_history(char *s) {
   int rfd, r, n;
   struct Stat sta;

   if (debug)
      cprintf("HISOTRY: %s\n", s);

   if ((rfd = open("/.history", O_RDWR | O_CREAT)) < 0)
      panic("open /.history: %e", rfd);

   if ((r = stat("/.history", &sta)) < 0)
      panic("stat /.history: %e", r);

   seek(rfd, sta.st_size);

   for (n = 0; s[n]; n++);
   s[n++] = '\n';
   s[n] = '\0';
   if ((r = write(rfd, s, n)) != n)
      panic("write /.history: %e", r);

   close(rfd);
}
```

添加`history`指令，首先在`/user`目录下编写`history.c`代码：

```
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
   int rfd;
   char buf[512];
   int n;

   if ((rfd = open("/.history", O_RDONLY | O_CREAT)) < 0)
      panic("open /.history: %e", rfd);

   while ((n = read(rfd, buf, sizeof buf-1)) > 0)
      sys_cputs(buf, n);

   close(rfd);
}
```

然后在`fs/Makefrag`文件中的`USERAPP`后添加：

```
$(OBJDIR)/user/history \
```

**测试**：

![](http://ww4.sinaimg.cn/large/6313a6d8jw1esr96revyaj20ss0mmwh8.jpg =600x)

- **文件创建 (touch)**

比较简单，添加指令`touch`。过程跟`history`类似，先编写`user/touch.c`，再向`fs/Makefrag`中添加相应条目。

`touch.c`：

```
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
   int rfd;
   char buf[512];

   if ((rfd = open(argv[1], O_WRONLY | O_CREAT)) < 0)
      panic("open %s: %e", argv[1], rfd);

   close(rfd);
}
```

- **ctl-c杀死当前进程**

反复翻看JOS的代码之后发现接收键盘输入到得到相应的字符这一步由`kern/console.c`完成。

在其中的`kbd_proc_data`函数中加入：

```java
if ((shift & CTL) && c == C('C'))
   return SIGINT;
```

其中`(shift & CTL)`为真代表`ctl`键被按下，`c`为输入的字符，`C('C')`为`ctl`按下时键盘上`c`键对应的字符。由此可以得到`ctl+c`按下时的状态。

`SIGINT`是添加在`inc/stdio.h`中的一个常量，是我自定义的`ctl+c`的ascii字符编号(0x11)。

由于shell读取输入是在`readline`中进行的，需要对其进行修改：如果读到`SIGINT`则结束读取，忽略后面的内容。

在shell中判断，如果读入了`SIGINT`，则结束当前进程。

**测试**:

![](http://ww4.sinaimg.cn/large/6313a6d8jw1esr9fmkw8gj20tu0nc7bh.jpg =600x)

其中

```
$ sh
$ $ cat lorem
```

间按了一次`ctl+c`，结束了刚启动的`sh`回到了外层的`sh`。

最后的

```
$ init: starting sh
```

是外层`sh`也接收到`ctl+c`结束后，返回到`user/init.c`时输出的。（`user/init.c`里会循环spawn出一个`sh`并等待其结束）

----

## 遇到的困难以及解决方法

这次lab比较简单，没有遇到什么困难。

## 收获及感想

这次lab把之前只有理论知识的文件系统实现了一遍。虽然很多部分都已经有完整的代码，实际需要写的地方不是很多。不过在阅读代码的过程中也是获益匪浅，特别是用户进程与fileserver进行ipc通信的过程，让我对其他进程执行文件操作的整个过程有了具体的了解，同时也再次熟悉了一下ipc。