# Lab 7 README <br/><small><small><small><small>江天源 1100016614<br/> 吕　鑫 1100016639</small></small></small></small>

## 使用方法

定义：

```
    usage: snapshot [-(n|c)rdv] [file...]
        n: Split-Mirror Strategy
        c: Copy-on-Write Strategy
        r: Make snapshots for directories recursively
        d: Debug info
        v: Verbose
     file: file(s)/directory(ies) to make snapshot; default: "/"
```


**Split-Mirror**

```
    snapshot -nrv [files...]
```

`[files...]`留空默认为根目录

**Copy-on-Write**

```
    snapshot -crv [files...]
```

## 测试&验证方法

(1) 在`kern/init.c`里将入口程序设置为`user/icode`，再执行

```
    make qemu
```

或者直接

```
    make run-icode
```

(2) 为了测试递归功能先建一些子文件夹：

```sh
    $ mkdir a
    $ mkdir a/b
    $ mkdir a/b/c
    $ cp lorem a/b
    $ cp -r a/b a/d
```

(3) 对整个文件系统进行snapshot：

**Split-Mirror**

```
    $ snapshot -nrv
```

**Copy-on-Write**

a. 执行两次以验证多份链接的重定向：

	```
    $ snapshot -crv
    $ snapshot -crv
	```

b. 测试Copy-on-Write机制的运作

	```
	$ echo test > a/b/lorem
	```

(4) 结果验证：

a. 列出所有snapshot：

	```
	$ ssfilter -l
	```

	得到结果类似于（第一项为Split-Mirror的结果，后两项为COW的结果）：

	```
	$ ssfilter -l
       8192 d @499666147(GMT+0 2015/06/17 04:09:07)
      12288 d @499670414(GMT+0 2015/06/17 05:20:14)
      12288 d @499670418(GMT+0 2015/06/17 05:20:18)

	```

b. 验证Split-Mirror（具体文件名参考上一步中实际列出来的文件名）：

	```
	$ ls -l @499666147
	```

	可以得到看到里面的文件与之前根目录下文件是一致的。

	验证：

	```
	$ cat @499666147/a/b/lorem
	```

	预期输出为`lorem`中原本的内容，即

	>Lorem ipsum dolor sit amet, consectetur
	adipisicing elit, sed do eiusmod tempor
	incididunt ut labore et dolore magna
	aliqua. Ut enim ad minim veniam, quis
	nostrud exercitation ullamco laboris
	nisi ut aliquip ex ea commodo consequat.
	Duis aute irure dolor in reprehenderit
	in voluptate velit esse cillum dolore eu
	fugiat nulla pariatur. Excepteur sint
	occaecat cupidatat non proident, sunt in
	culpa qui officia deserunt mollit anim
	id est laborum.

c. 验证Copy-on-Write
	
	i.首先验证链接文件的建立：

	```
	$ ls -l @499670414
	$ ls -l @499670418
	```
	
	可以发现snapshot里面的一般文件类型为`l`，即链接文件，其大小也非常的小。

	ii.之后验证Copy-on-Write机制的运作：

	```
	$ cat @499670414/a/b/lorem
	$ cat @499670418/a/b/lorem
	```

	两次输出都应该为`lorem`原本的内容，而非测试时echo重定向进去的`test`。

	iii.最后检查重定向的处理：

	```
	$ ls -l @499670414/a/b
          0 d @499670414/a/b/c
         22 l @499670414/a/b/lorem
	$ ls -l @499670418/a/b
          0 d @499670418/a/b/c
        447 - @499670418/a/b/lorem

	```
	发现老的一份snapshot中的`a/b/lorem`文件仍然是链接文件`l`（现在指向的是`@499670418/a/b/lorem`，否则`cat`不会输出正确的结果）；新的一份snapshot中的`a/b/lorem`文件为普通文件类型，这是COW机制运作的结果，在`write`操作时即将原始的`a/b/lorem`文件中的内容拷贝到了`@499670418/a/b/lorem`中了。

d. （辅助功能）检查snapshot筛选功能：

	考虑到实际使用时的需求，筛选函数只精确到小时。所以为了方便测试，需要先创建几个有一定时间差距的假的`snapshot`文件（使用`ssfilter`可以查看这几个假`snapshot`的时间）：

	```
	$ mkdir @400000000
	$ mkdir @500000000
	```

	验证精确筛选（恰好等于给定时间）：

	```
	$ ssfilter -l / 2015 06
       8192 d /@499666147(GMT+0 2015/06/17 04:09:07)
      12288 d /@499670414(GMT+0 2015/06/17 05:20:14)
      12288 d /@499670418(GMT+0 2015/06/17 05:20:18)
          0 d /@500000000(GMT+0 2015/06/21 00:53:20)
	```

	```
	$ ssfilter -l / 2015 06 17 05
      12288 d /@499670414(GMT+0 2015/06/17 05:20:14)
      12288 d /@499670418(GMT+0 2015/06/17 05:20:18)
	```


    验证筛选给定时间点后的所有时间：


	```
	$ ssfilter -la / 2015 06 18
          0 d /@500000000(GMT+0 2015/06/21 00:53:20)
	```

	验证筛选给定时间点前的所有时间：


	```
	$ ssfilter -lb / 2015 06 17
       8192 d /@499666147(GMT+0 2015/06/17 04:09:07)
      12288 d /@499670414(GMT+0 2015/06/17 05:20:14)
      12288 d /@499670418(GMT+0 2015/06/17 05:20:18)
          0 d /@400000000(GMT+0 2012/05/10 15:06:40)
	```

	**至此所有主要功能验证完毕**

## 代码说明

本次lab主要涉及以下文件：（略去了只有函数声明的头文件；已在相关部分在注释中标记`Lab 7`）

```c
/fs/fs.c
/fs/serv.c

/inc/fs.h

/lib/fd.c
/lib/file.c

// 以上代码负责处理与文件系统的ipc以及文件系统上的操作

/lib/string.c       // 添加了一些辅助的字符串处理/查找函数

/user/cp.c          // 文件/目录拷贝
/user/link.c        // 建立链接文件
/user/ls.c          // 添加了对于系统隐藏文件和snapshot文件的处理
/user/mkdir.c       // 创建文件夹
/user/snapshot.c    // 创建snapshot文件
/user/ssfilter.c    // 对snapshot文件进行筛选显示
/user/systime.c     // 获取系统当前时间的时间戳（从2000/01/01 00:00算起）
```

## 成员分工

|姓名|负责工作|
|:-:|:-----:|
|江天源|各种辅助函数的实现；COW策略下执行`snapshot`时的处理|
|吕鑫|Split-Mirror策略；COW策略下执行`write`（或发生`trunc`）时的处理|

## Github Repo

[Github -- JOS2014 at Lab7](https://github.com/jtyuan/JOS2014/tree/lab7) [https://github.com/jtyuan/JOS2014/tree/lab7]