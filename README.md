# JOS Lab 6 Report<br/><small><small><small><small>江天源 1100016614</small></small></small></small>

## 总体概述


### 完成情况

|#|E1|E2|E3|E4|E5|E6|E7|E8|E9|E10|E11|E12|E13|
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|Status|√|√|√|√|√|√|√|√|√|√|√|√|√|

|#|Q1|Q2|Q3|Q4|C1|C2|C3|C4|C5|
|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
|Status|√|√|√|√|√|×|×|×|×|

<small>* 其中E#代表Exercise #, Q#代表Question #, C# 代表Challenge #</small>

共完成了1个Challenge。

`make grade`结果

![](http://ww4.sinaimg.cn/large/6313a6d8jw1et2acl3pg0j20x00mm10k.jpg =600x)

### Part A: Initialization and transmitting packets

#### Exercise 1

>Add a call to time_tick for every clock interrupt in kern/trap.c. Implement sys_time_msec and add it to syscall in kern/syscall.c so that user space has access to the time.

在`kern/trap.c` `trap_dispatch`函数中，处理时钟中断处加上对`time_tick`的调用即可（根据注释，在多处理器的情况下，每个CPU都会触发时钟中断，因此需要加上判断，只在第一个CPU上进行计数）：

```java
if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
   if (thiscpu->cpu_id == 0)
      time_tick();
   lapic_eoi();
   sched_yield();
   return;
}
```

`sys_time_msec`：获取`time_msec`的函数在`kern/time.c`中已经定义了，直接调用`time_msec`函数即可。（同样要在syscall添加对SYS_time_msec的处理）

```java
static int
sys_time_msec(void)
{
   return time_msec();
}
```

#### Exercise 2

>Browse Intel's Software Developer's Manual for the E1000. This manual covers several closely related Ethernet controllers. QEMU emulates the 82540EM.

>You should skim over chapter 2 now to get a feel for the device. To write your driver, you'll need to be familiar with chapters 3 and 14, as well as 4.1 (though not 4.1's subsections). You'll also need to use chapter 13 as reference. The other chapters mostly cover components of the E1000 that your driver won't have to interact with. Don't worry about the details right now; just get a feel for how the document is structured so you can find things later.

>While reading the manual, keep in mind that the E1000 is a sophisticated device with many advanced features. A working E1000 driver only needs a fraction of the features and interfaces that the NIC provides. Think carefully about the easiest way to interface with the card. We strongly recommend that you get a basic driver working before taking advantage of the advanced features.

浏览了一遍，对内容的位置有大概印象，具体还是得实现的时候查阅。


#### Exercise 3

>Implement an attach function to initialize the E1000. Add an entry to the pci_attach_vendor array in kern/pci.c to trigger your function if a matching PCI device is found (be sure to put it before the {0, 0, 0} entry that mark the end of the table). You can find the vendor ID and device ID of the 82540EM that QEMU emulates in section 5.2. You should also see these listed when JOS scans the PCI bus while booting.

>For now, just enable the E1000 device via pci_func_enable. We'll add more initialization throughout the lab.

>We have provided the kern/e1000.c and kern/e1000.h files for you so that you do not need to mess with the build system. They are currently blank; you need to fill them in for this exercise. You may also need to include the e1000.h file in other places in the kernel.

>When you boot your kernel, you should see it print that the PCI function of the E1000 card was enabled. Your code should now pass the pci attach test of make grade.

在`kern/e1000.h`中添加：

```
#include <kern/pci.h>

#define E1000_VENDORID 0x8086
#define E1000_DEVICEID 0x100e

int e1000_attach(struct pci_func *pcif);
```

`kern/e1000.c`实现`e1000_attach`，现在只需调用`pci_func_enable`函数：

```java
int
e1000_attach(struct pci_func *pcif)
{
   pci_func_enable(pcif);
}
```


在`kern/pci.c`中的`pci_attach_vendor`中`{0, 0, 0}`前添加相关内容：

```java
// pci_attach_vendor matches the vendor ID and device ID of a PCI device
struct pci_driver pci_attach_vendor[] = {
   {E1000_VENDORID, E1000_DEVICEID, &e1000_attach},
   { 0, 0, 0 },
};
```


#### Exercise 4

> In your attach function, create a virtual memory mapping for the E1000's BAR 0 by calling mmio_map_region (which you wrote in lab 4 to support memory-mapping the LAPIC).

>You'll want to record the location of this mapping in a variable so you can later access the registers you just mapped. Take a look at the lapic variable in kern/lapic.c for an example of one way to do this. If you do use a pointer to the device register mapping, be sure to declare it volatile; otherwise, the compiler is allowed to cache values and reorder accesses to this memory.

>To test your mapping, try printing out the device status register (section 13.4.2). This is a 4 byte register that starts at byte 8 of the register space. You should get 0x80080783, which indicates a full duplex link is up at 1000 MB/s, among other things.

`kern/e1000.h`添加

```
#include <kern/pmap.h>

volatile uint32_t *e1000; 

#define E1000_STATUS   (0x00008>>2)  // Device Status - RO
```

根据lab6的介绍，manual里设计的这些位置数值代表的是Byte偏移，因此在进行数组或者指针运算时需要除以类型的大小（这里是4）。

`kern/e1000.c`中添加

```
e1000 = (uint32_t *) mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
cprintf("E1000 status: 0x%08x\n", e1000[E1000_STATUS]);
```

测试：

`make INIT_CFLAGS=-DTEST_NO_NS qemu`输出结果

```
E1000 status: 0x80080783
```

正确。

#### Exercise 5

>Perform the initialization steps described in section 14.5 (but not its subsections). Use section 13 as a reference for the registers the initialization process refers to and sections 3.3.3 and 3.4 for reference to the transmit descriptors and transmit descriptor array.

>Be mindful of the alignment requirements on the transmit descriptor array and the restrictions on length of this array. Since TDLEN must be 128-byte aligned and each transmit descriptor is 16 bytes, your transmit descriptor array will need some multiple of 8 transmit descriptors. However, don't use more than 64 descriptors or our tests won't be able to test transmit ring overflow.

>For the TCTL.COLD, you can assume full-duplex operation. For TIPG, refer to the default values described in table 13-77 of section 13.4.34 for the IEEE 802.3 standard IPG (don't use the values in the table in section 14.5).

先看看Developer's Manual的14.5节怎么说：

>Allocate a region of memory for the transmit descriptor list. Software should insure this memory is aligned on a paragraph (16-byte) boundary. Program the Transmit Descriptor Base Address (TDBAL/TDBAH) register(s) with the address of the region. TDBAL is used for 32-bit addresses and both TDBAL and TDBAH are used for 64-bit addresses.

>Set the Transmit Descriptor Length (TDLEN) register to the size (in bytes) of the descriptor ring.
This register must be 128-byte aligned.

>The Transmit Descriptor Head and Tail (TDH/TDT) registers are initialized (by hardware) to 0b after a power-on or a software initiated Ethernet controller reset. Software should write 0b to both these registers to ensure this.

>Initialize the Transmit Control Register (TCTL) for desired operation to include the following:

>
- Set the Enable (TCTL.EN) bit to 1b for normal operation.
- Set the Pad Short Packets (TCTL.PSP) bit to 1b.
- Configure the Collision Threshold (TCTL.CT) to the desired value. Ethernet standard is 10h.
This setting only has meaning in half duplex mode.
- Configure the Collision Distance (TCTL.COLD) to its expected value. For full duplex
operation, this value should be set to 40h. For gigabit half duplex, this value should be set to
200h. For 10/100 half duplex, this value should be set to 40h.

>Program the Transmit IPG (TIPG) register with the following decimal values to get the minimum
legal Inter Packet Gap:

>![](http://ww2.sinaimg.cn/large/6313a6d8jw1et0gkfhl1xj20my090abb.jpg =600x)

然而Exercise的描述里说到`TIPG`的值需要用在`13.4.34`节中提到的IEEE 802.3标准IPG的默认值：

![](http://ww2.sinaimg.cn/large/6313a6d8jw1et2auq1anfj20zy14inca.jpg =600x)

![](http://ww3.sinaimg.cn/large/6313a6d8jw1et2av8jtbfj20xs0li10d.jpg =600x)

`IPGT1`默认值`8`，`IPGT2`的默认值为`6`，而且标准要求两者之间满足`3*IPGT1=2*IPGT2`，默认值显然是不满足的。然而后两者只在半双工时有显著影响，因此随便取一者的默认值，并令另一方满足等式关系即可。

最终取值`IPGT=10` `IPG1=4` `IPG2=6`

参考manual 3.3.3节：

![](http://ww2.sinaimg.cn/large/6313a6d8jw1et0gddvqo3j20sc07sdhq.jpg =600x)

设计tx descriptor的结构如下（lab的介绍里也直接提供了）：

```
struct tx_desc
{
   uint64_t addr;
   uint16_t length;
   uint8_t cso;
   uint8_t cmd;
   uint8_t status;
   uint8_t css;
   uint16_t special;
} __attribute__((packed));
```

在`kern/e1000.c` `e1000_attach`中添加上述初始化代码：

```
memset(tx_desc_arr, 0, sizeof(struct tx_desc)*E1000_TXDESC);
memset(tx_pkt_buf, 0, sizeof(struct tx_pkt)*E1000_TXDESC);
for (i = 0; i < E1000_TXDESC; i++) {
   tx_desc_arr[i].addr = PADDR(tx_pkt_buf[i].buf);
   tx_desc_arr[i].status = E1000_TXD_STAT_DD;
   tx_desc_arr[i].cmd = E1000_TXD_CMD_RS|E1000_TXD_CMD_EOP;
}

// Program the Transmit Descriptor Base Address Register
e1000[E1000_TDBAL] = PADDR(tx_desc_arr);
e1000[E1000_TDBAH] = 0;

// Set the Transmit Descriptor Length Register
e1000[E1000_TDLEN] = sizeof(struct tx_desc) * E1000_TXDESC;

// Initialize the Transmit Descriptor Head and Tail Registers
e1000[E1000_TDH] = 0;
e1000[E1000_TDT] = 0;

// Initialize the Transmit Control Register
e1000[E1000_TCTL] = E1000_TCTL_EN|E1000_TCTL_PSP|
   (E1000_TCTL_CT & (0x10 << 4))|(E1000_TCTL_COLD & (0x40 <<12));

// Program the Transmit IPG Register
e1000[E1000_TIPG] = 10|(4<<10)|(6<<20);
```

以下是参考`e1000_hw.h`向`kern/e1000.h`中添加的常量：

```c
#define E1000_TXDESC       64
#define TX_PKT_SIZE     1518

#define E1000_TDBAL        (0x03800>>2)  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH        (0x03804>>2)  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN        (0x03808>>2)  /* TX Descriptor Length - RW */
#define E1000_TDH          (0x03810>>2)  /* TX Descriptor Head - RW */
#define E1000_TDT          (0x03818>>2)  /* TX Descripotr Tail - RW */

#define E1000_TCTL         (0x00400>>2)  /* TX Control - RW */
#define E1000_TCTL_EN      0x00000002 /* enable tx */
#define E1000_TCTL_PSP     0x00000008 /* pad short packets */
#define E1000_TCTL_CT      0x00000ff0 /* collision threshold */
#define E1000_TCTL_COLD    0x003ff000 /* collision distance */

#define E1000_TIPG         (0x00410>>2)  /* TX Inter-packet gap -RW */

#define E1000_TXD_CMD_RS   0x00000008 /* Report Status */
#define E1000_TXD_CMD_EOP  0x00000001 /* End of Packet */
#define E1000_TXD_STAT_DD  0x00000001 /* Descriptor Done */

```

#### Exercise 6

>Write a function to transmit a packet by checking that the next descriptor is free, copying the packet data into the next descriptor, and updating TDT. Make sure you handle the transmit queue being full.

在`kern/e1000.c`中实现transmit函数，检查下一个transmit descriptor是否为free：

```
int
e1000_transmit(char *data, size_t len)
{
   uint32_t tdt = e1000[E1000_TDT];

   // Check that the next tx desc is free
   if (tx_desc_arr[tdt].status & E1000_TXD_STAT_DD) {
      
      if (len > TX_PKT_SIZE)
         len = TX_PKT_SIZE;
      
      memmove(tx_pkt_buf[tdt].buf, data, len);
      
      tx_desc_arr[tdt].length = len;

      tx_desc_arr[tdt].status &= ~E1000_TXD_STAT_DD;
      tx_desc_arr[tdt].cmd |= E1000_TXD_CMD_RS;
      tx_desc_arr[tdt].cmd |= E1000_TXD_CMD_EOP;

      e1000[E1000_TDT] = (tdt + 1) % E1000_TXDESC;

      return 0;
   }
   
   return -E_TX_FULL;
}
```

这里需要注意的是`TDT`的值不是byte偏移而是数组的下标，因此上面的代码在使用`tdt`时无需除`4`。

其中涉及的错误`E_TX_FULL`定义在`inc/error.h`中。

#### Exercise 7

>Add a system call that lets you transmit packets from user space. The exact interface is up to you. Don't forget to check any pointers passed to the kernel from user space.

添加一个系统调用为用户程序提供调用`e1000_transmit`的接口，检查一下传入虚拟地址，再直接调用就行了：

```
static int
sys_net_try_send(char *data, size_t len)
{
   user_mem_assert(curenv, data, len, PTE_U);
   return e1000_transmit(data, len);
}
```

具体的系统调用添加过程跟以往一致，不再赘述。

#### Exercise 8

>Implement net/output.c.

具体要实现的功能在`output.c`中写得很清楚：

>- read a packet from the network server
- send the packet to the device driver

第一步由`lab4`实现的ipc完成，其中传输的缓存放在`union Nsipc`中，其中包含一个结构体

```
struct jif_pkt {
    int jp_len;
    char jp_data[0];
};
```

收到ipc请求之后需要检查其来源并且确认其请求类型为`NSREQ_OUTPUT`；

之后调用之前实现的系统调用即可：

```
#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
   binaryname = "ns_output";

   // LAB 6: Your code here:
   //    - read a packet from the network server
   // - send the packet to the device driver
   int r;

   while (1) {
      r = sys_ipc_recv(&nsipcbuf);

      if ((thisenv->env_ipc_from != ns_envid) ||
          (thisenv->env_ipc_value != NSREQ_OUTPUT))
         continue;

      while ((r = sys_net_try_send(nsipcbuf.pkt.jp_data,
         nsipcbuf.pkt.jp_len)));
   }
}

```

测试运行`make E1000_DEBUG=TXERR,TX run-net_testoutput`，得到结果：

![](http://ww4.sinaimg.cn/large/6313a6d8jw1et0i1bi2jaj20xc0muguf.jpg =600x)

测试`tcpdump -XXnr qemu.pcap`：

![](http://ww1.sinaimg.cn/large/6313a6d8jw1et0i3f9f51j20uu0l4aje.jpg =600x)

一次性测试更多的包`make E1000_DEBUG=TXERR,TX NET_CFLAGS=-DTESTOUTPUT_COUNT=100 run-net_testoutput`：

![](http://ww4.sinaimg.cn/large/6313a6d8jw1et0i4ilanfj20rq13kdv3.jpg =600x)

so far so good.

#### Question 1

>How did you structure your transmit implementation? In particular, what do you do if the transmit ring is full?

当transmit ring满时，会将新传来的包drop掉，然后返回给用户失败信息。`output`中发现发送失败（返回`<0`）后，会反复重试直到成功。

### Part B: Receiving packets and the web server

#### Exercise 9

>Read section 3.2. You can ignore anything about interrupts and checksum offloading (you can return to these sections if you decide to use these features later), and you don't have to be concerned with the details of thresholds and how the card's internal caches work.

接收包的描述符结构：

![](http://ww3.sinaimg.cn/large/6313a6d8jw1et2djv9ruwj20xq09egnm.jpg =600x)

得到：

```
struct rx_desc
{
   uint64_t addr;
   uint16_t length;
   uint16_t chksum;
   uint8_t status;
   uint8_t errors;
   uint16_t special;
} __attribute__((packed));
```

接收时的流程：

![](http://ww4.sinaimg.cn/large/6313a6d8jw1et2dkpkeemj20y20ugtca.jpg =600x)

#### Exercise 10

>Set up the receive queue and configure the E1000 by following the process in section 14.4. You don't have to support "long packets" or multicast. For now, don't configure the card to use interrupts; you can change that later if you decide to use receive interrupts. Also, configure the E1000 to strip the Ethernet CRC, since the grade script expects it to be stripped.

>By default, the card will filter out all packets. You have to configure the Receive Address Registers (RAL and RAH) with the card's own MAC address in order to accept packets addressed to that card. You can simply hard-code QEMU's default MAC address of 52:54:00:12:34:56 (we already hard-code this in lwIP, so doing it here too doesn't make things any worse). Be very careful with the byte order; MAC addresses are written from lowest-order byte to highest-order byte, so 52:54:00:12 are the low-order 32 bits of the MAC address and 34:56 are the high-order 16 bits.

>The E1000 only supports a specific set of receive buffer sizes (given in the description of RCTL.BSIZE in 13.4.22). If you make your receive packet buffers large enough and disable long packets, you won't have to worry about packets spanning multiple receive buffers. Also, remember that, just like for transmit, the receive queue and the packet buffers must be contiguous in physical memory.

这次轮到写receive的初始化，同样先看看manual的14.4节怎么说：

>*Program the Receive Address Register**(s) (RAL/RAH) with the desired Ethernet addresses. RAL[0]/RAH[0] should always be used to store the Individual Ethernet MAC address of the Ethernet controller. This can come from the EEPROM or from any other means (for example, on some machines, this comes from the system PROM not the EEPROM on the adapter port).

>...

>**Allocate a region of memory for the receive descriptor list**. Software should insure this memory is aligned on a paragraph (16-byte) boundary. Program the Receive Descriptor Base Address (RDBAL/RDBAH) register(s) with the address of the region. RDBAL is used for 32-bit addresses and both RDBAL and RDBAH are used for 64-bit addresses.

>**Set the Receive Descriptor Length (RDLEN) register** to the size (in bytes) of the descriptor ring. This register must be 128-byte aligned.

>**The Receive Descriptor Head and Tail registers are initialized** (by hardware) to 0b after a power-on or a software-initiated Ethernet controller reset. Receive buffers of appropriate size should be allocated and pointers to these buffers should be stored in the receive descriptor ring. Software initializes the Receive Descriptor Head (RDH) register and Receive Descriptor Tail (RDT) with the appropriate head and tail addresses. Head should point to the first valid receive descriptor in the descriptor ring and tail should point to one descriptor beyond the last valid descriptor in the descriptor ring.

>**Program the Receive Control (RCTL) register** with appropriate values for desired operation to include the following:

>- Set the receiver Enable (RCTL.EN) bit to 1b for normal operation. However,it is best to leave the Ethernet controller receive logic disabled (RCTL.EN = 0b) until after the receive descriptor ring has been initialized and software is ready to process received packets.
- Set the Long Packet Enable (RCTL.LPE) bit to 1b when processing packets greater than the standard Ethernet packet size. For example, this bit would be set to 1b when processing Jumbo Frames.
- Loopback Mode (RCTL.LBM) should be set to 00b for normal operation.
- Configure the Receive Descriptor Minimum Threshold Size (RCTL.RDMTS) bitstothe desired value.
- Configure the Multicast Offset (RCTL.MO) bits to the desired value.
- Set the Broadcast AcceptMode (RCTL.BAM) bit to 1b allowing the hardware to accept broadcast packets.
- Configure the Receive Buffer Size (RCTL.BSIZE) bits to reflect the size of the receive buffers software provides to hardware. Also configure the Buffer Extension Size (RCTL.BSEX) bits if receive buffer needs to be larger than 2048 bytes.
- Set the Strip Ethernet CRC (RCTL.SECRC) bit if the desire is for hardware to strip the CRC prior to DMA-ing the receive packet to host memory.
- ...

按照上述顺序进行初始化：


设置MAC地址（被hard-coded为`52:54:00:12:34:56`）

```
uint32_t MAC[2] = {0x12005452, 0x5634};

e1000[E1000_RAL] = MAC[0];
e1000[E1000_RAH] = MAC[1];
e1000[E1000_RAH] |= 0x80000000;
```

初始化描述符数组和缓存，并设置描述符表的地址：

```
memset(rx_desc_arr, 0, sizeof(struct rx_desc) * E1000_RXDESC);
memset(rx_pkt_buf, 0, sizeof(struct rx_pkt) * E1000_RXDESC);
for (i = 0; i < E1000_RXDESC; i++) {
   rx_desc_arr[i].addr = PADDR(rx_pkt_buf[i].buf);
   rx_desc_arr[i].status = 0;
}

e1000[E1000_RDBAL] = PADDR(rx_desc_arr);
e1000[E1000_RDBAH] = 0;
```

设置接收描述符表大小：

```
e1000[E1000_RDLEN] = sizeof(struct rx_desc) * E1000_RXDESC;
```

初始化描述符队列的首尾指针：

```
e1000[E1000_RDH] = 0;
e1000[E1000_RDT] = 0;
```

设置控制寄存器：

```
e1000[E1000_RCTL] = E1000_RCTL_EN;
e1000[E1000_RCTL] &= ~E1000_RCTL_LPE;
e1000[E1000_RCTL] &= ~E1000_RCTL_LBM;
e1000[E1000_RCTL] &= ~E1000_RCTL_RDMTS;
e1000[E1000_RCTL] &= ~E1000_RCTL_MO;
e1000[E1000_RCTL] |= E1000_RCTL_BAM;
e1000[E1000_RCTL] |= E1000_RCTL_BSIZE;
e1000[E1000_RCTL] |= E1000_RCTL_SECRC;
```

测试`make E1000_DEBUG=TX,TXERR,RX,RXERR,RXFILTER run-net_testinput`：

```
e1000: unicast match[0]: 52:54:00:12:34:56
```

#### Exercise 11

>Write a function to receive a packet from the E1000 and expose it to user space by adding a system call. Make sure you handle the receive queue being empty.

>If you decide to use interrupts to detect when packets are received, you'll need to write code to handle these interrupts. If you do use interrupts, note that, once an interrupt is asserted, it will remain asserted until the driver clears the interrupt. In your interrupt handler make sure to clear the interrupt handled as soon as you handle it. If you don't, after returning from your interrupt handler, the CPU will jump back into it again. In addition to clearing the interrupts on the E1000 card, interrupts also need to be cleared on the LAPIC. Use lapic_eoi to do so.

与`e1000_transmit`类似：

```
size_t
e1000_receive(char *data, size_t len)
{
   uint32_t rdt;
   rdt = e1000[E1000_RDT];

   if (rx_desc_arr[rdt].status & E1000_RXD_STAT_DD) {

      if (len > rx_desc_arr[rdt].length)
         len = rx_desc_arr[rdt].length;
      
      memmove(data, rx_pkt_buf[rdt].buf, len);
      
      rx_desc_arr[rdt].status &= ~E1000_RXD_STAT_DD;
      rx_desc_arr[rdt].status &= ~E1000_RXD_STAT_EOP;

      e1000[E1000_RDT] = (rdt + 1) % E1000_RXDESC;

      return len;
   }
   
   return -E_RX_EMPTY;
}
```

接收的系统调用也类似：

```
static size_t
sys_net_recv(char *data, size_t len)
{
   user_mem_assert(curenv, data, len, PTE_U | PTE_W);
   return e1000_receive(data, len);
}
```

#### Exercise 12

>Implement `net/input.c`.

在从设备驱动接收到数据前反复尝试，接收到后将其发送到network server：


```
#include "ns.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
   binaryname = "ns_input";
   char buf[2048];

   int len, r, i;
   int perm = PTE_U | PTE_P | PTE_W;
   len = RECV_BUF_SIZE -1;

   while (1) {
      while ((len = sys_net_recv(buf, len)) < 0)
         sys_yield();

      while ((r = sys_page_alloc(0, &nsipcbuf, perm)) < 0);

      nsipcbuf.pkt.jp_len = len;
      memmove(nsipcbuf.pkt.jp_data, buf, nsipcbuf.pkt.jp_len);

      while ((r = sys_ipc_try_send(ns_envid, NSREQ_INPUT, &nsipcbuf, perm)) < 0);
   }
}
```

测试`make E1000_DEBUG=TX,TXERR,RX,RXERR,RXFILTER run-net_testinput`：

![](http://ww3.sinaimg.cn/large/6313a6d8jw1et2e2i7l3mj20xs0n0aiz.jpg =600x)

结果正确。

#### Question 2

>How did you structure your receive implementation? In particular, what do you do if the receive queue is empty and a user environment requests the next incoming packet?

与transmit时类似，如果接收队列为空则返回错误；用户发现错误则会反复尝试，直到成功接收为止。

#### Challenge 1

>Read about the EEPROM in the developer's manual and write the code to load the E1000's MAC address out of the EEPROM. Currently, QEMU's default MAC address is hard-coded into both your receive initialization and lwIP. Fix your initialization to use the MAC address you read from the EEPROM, add a system call to pass the MAC address to lwIP, and modify lwIP to the MAC address read from the card. Test your change by configuring QEMU to use a different MAC address.

Manual的5.3.1节如是说：

>Software can use the EEPROM Read register (EERD) to cause the Ethernet controller to read a word from the EEPROM that the software can then use. To do this, software writes the address to read the Read Address (EERD.ADDR) field and then simultaneously writes a 1b to the Start Read bit (EERD.START). The Ethernet controller then reads the word from the EEPROM, sets the Read Done bit (EERD.DONE), and puts the data in the Read Data field (EERD.DATA). Software can poll the EEPROM Read register until it sees the EERD.DONE bit set, then use the data from the EERD.DATA field. Any words read this way are not written to hardware’s internal registers.
Software can also directly access the EEPROM’s 4-wire interface through the EEPROM/FLASH Control Register (EEC). It can use this for reads, writes, or other EEPROM operations.

再根据5.6.1节：

>The Ethernet Individual Address (IA) is a six-byte field that must be unique for each Ethernet port (and unique for each copy of the EEPROM image). The first three bytes are vendor specific. The value from this field is loaded into the Receive Address Register 0 (RAL0/RAH0). For a MAC address of 12-34-56-78-90-AB, words 2:0 load as follows (note that these words are byte- swapped):
>
   Word 0 = 3412 
   Word 1 = 7856 
   Word 2 = AB90

从`EERD`的`EERD_START`开始逐字读入直到`EERD_DONE`为真，读出这三个字然后存入`RAL/RAH`

```
for (e1000[E1000_EERD] = 0x0, e1000[E1000_EERD] |= E1000_EERD_START;
   !(e1000[E1000_EERD] & E1000_EERD_DONE); e1000[E1000_EERD] >>= 16);
e1000[E1000_RAL] = e1000[E1000_EERD] >> 16;

for (e1000[E1000_EERD] = 0x1 << 8, e1000[E1000_EERD] |= E1000_EERD_START;
   !(e1000[E1000_EERD] & E1000_EERD_DONE); e1000[E1000_EERD] >>= 16);
e1000[E1000_RAL] |= e1000[E1000_EERD] & 0xffff0000;

for (e1000[E1000_EERD] = 0x2 << 8, e1000[E1000_EERD] |= E1000_EERD_START;
   !(e1000[E1000_EERD] & E1000_EERD_DONE); e1000[E1000_EERD] >>= 16);
e1000[E1000_RAH] = e1000[E1000_EERD] >> 16;

e1000[E1000_RAH] |= 0x80000000;
```

在最后一句前加了一句`cprintf("%x %x\n", e1000[E1000_RAH], e1000[E1000_RAL]);`输出：

![](http://ww2.sinaimg.cn/large/6313a6d8jw1et2g4y9ezij207o02sdg3.jpg =120x)

再测试`make grade`：

![](http://ww3.sinaimg.cn/large/6313a6d8jw1et2g848mttj20xk0mwdmz.jpg =600x)

结果正确~

#### Exercise 13

>The web server is missing the code that deals with sending the contents of a file back to the client. Finish the web server by implementing send_file and send_data.

`send_data`：读出文件大小，然后读出文件内容并写入`sock`

```
static int
send_data(struct http_request *req, int fd)
{
   // LAB 6: Your code here.
   
   char buf[MAX_PACKET_SIZE];
   int r;
   struct Stat stat;

   if ((r = fstat(fd, &stat)) < 0)
      die("send_data: fstat failed.");

   if ((r = readn(fd, buf, stat.st_size)) != stat.st_size)
      die("send_data: readn failed.");

   if ((r = write(req->sock, buf, stat.st_size)) != stat.st_size)
      die("send_data: write failed.");

   return 0;
}
```

`send_file`补充代码，在文件不存在或者为目录的情况下返回`404`错误，文件存在的情况下设置`file_size`即可：

```
if((fd = open(req->url, O_RDONLY)) < 0) {
   send_error(req, 404);
   goto end;
}
if(st.st_isdir) {
   send_error(req, 404);
   goto end;
}
if(fstat(fd, &st) < 0) {
   send_error(req, 501);
   goto end;
}
file_size = st.st_size;
```

测试`make grade`：

![](http://ww4.sinaimg.cn/large/6313a6d8jw1et2acl3pg0j20x00mm10k.jpg =600x)

#### Question 3

>What does the web page served by JOS's web server say?

如下

>This file came from JOS.
>Cheesy web page!

#### Question 4

>How long approximately did it take you to do this lab?

3 whole days

## 遇到的困难以及解决方法

这次主要的难度在于对于文档的阅读，并将其实现这件事。实现的过程中遇到的问题大多数都是来自于阅读文档时漏掉了细节（主要是在初始化阶段的设置时出了问题）。反复对照文档修改总算把问题解决了。

还有一个问题是，做到Exercise 12的时候，死活不出结果。。最后发现好像是新版的QEMU出了问题，最后装了教学网上放出的QEMU后终于能通过了。

## 收获及感想

这次lab与之前最大的不同在于，之前的lab需要阅读大量的代码，而且注释都非常详细。而这次的lab主要任务是对文档的阅读与理解，感觉在反反复复调试修改的过程中，对阅读这种技术性文档的能力有了飞跃\_(′ཀ`」 ∠)\_。。

## 参考文献

[1] [PCI/PCI-X Family of Gigabit Ethernet Controllers Software Developer’s Manual](http://pdosnew.csail.mit.edu/6.828/2014/readings/hardware/8254x_GBe_SDM.pdf)

[2] [QEMU's e1000_hw.c](http://pdosnew.csail.mit.edu/6.828/2014/labs/lab6/e1000_hw.h)