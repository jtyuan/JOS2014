#include <kern/e1000.h>

// LAB 6: Your driver code here
uint32_t MAC[2] = {0x12005452, 0x5634}; // 52:54:00:12:34:56 

struct tx_desc tx_desc_arr[E1000_TXDESC] __attribute__ ((aligned (16)));
struct tx_pkt tx_pkt_buf[E1000_TXDESC];

struct rx_desc rx_desc_arr[E1000_RXDESC] __attribute__ ((aligned (16)));
struct rx_pkt rx_pkt_buf[E1000_RXDESC];

static void init_desc_array();

int
e1000_attach(struct pci_func *pcif)
{
	pci_func_enable(pcif);
	init_desc_array();

	e1000 = (uint32_t *) mmio_map_region(ROUNDDOWN(pcif->reg_base[0], PGSIZE), 
		ROUNDUP(pcif->reg_size[0], PGSIZE));

	cprintf("e1000 status: 0x%08x\n", e1000[E1000_STATUS]);
	assert(e1000[E1000_STATUS] == 0x80080783);

	// -------------------Transmit initialization--------------------
	// Program the Transmit Descriptor Base Address Register
	e1000[E1000_TDBAL] = PADDR(tx_desc_arr);
	e1000[E1000_TDBAH] = 0;

	// Set the Transmit Descriptor Length Register
	e1000[E1000_TDLEN] = sizeof(struct tx_desc) * E1000_TXDESC;
	// cprintf("TDLEN: %d\n", e1000[E1000_TDLEN]);

	// Initialize the Transmit Descriptor Head and Tail Registers
	e1000[E1000_TDH] = 0;
	e1000[E1000_TDT] = 0;

	// Initialize the Transmit Control Register
	e1000[E1000_TCTL] = E1000_TCTL_EN|E1000_TCTL_PSP|
		(E1000_TCTL_CT & (0x10 << 4))|(E1000_TCTL_COLD & (0x40 <<12));

	// Program the Transmit IPG Register
	e1000[E1000_TIPG] = 10|(4<<10)|(6<<20);


	// -------------------Receive initialization--------------------
	// Program the Receive Address Registers	
	for (e1000[E1000_EERD] = 0x0, e1000[E1000_EERD] |= E1000_EERD_START;
		!(e1000[E1000_EERD] & E1000_EERD_DONE); e1000[E1000_EERD] >>= 16);
	e1000[E1000_RAL] = e1000[E1000_EERD] >> 16;

	for (e1000[E1000_EERD] = 0x1 << 8, e1000[E1000_EERD] |= E1000_EERD_START;
		!(e1000[E1000_EERD] & E1000_EERD_DONE); e1000[E1000_EERD] >>= 16);
	e1000[E1000_RAL] |= e1000[E1000_EERD] & 0xffff0000;

	for (e1000[E1000_EERD] = 0x2 << 8, e1000[E1000_EERD] |= E1000_EERD_START;
		!(e1000[E1000_EERD] & E1000_EERD_DONE); e1000[E1000_EERD] >>= 16);
	e1000[E1000_RAH] = e1000[E1000_EERD] >> 16;

	// cprintf("%x %x\n", e1000[E1000_RAH], e1000[E1000_RAL]);

	e1000[E1000_RAH] |= 0x80000000;

	// e1000[E1000_RAL] = MAC[0];
	// e1000[E1000_RAH] = MAC[1];
	// e1000[E1000_RAH] |= 0x80000000;

	// Program the Receive Descriptor Base Address Registers
	e1000[E1000_RDBAL] = PADDR(rx_desc_arr);
	e1000[E1000_RDBAH] = 0;

	// // Set the Receive Descriptor Length Register
	e1000[E1000_RDLEN] = sizeof(struct rx_desc) * E1000_RXDESC;

	// // Set the Receive Descriptor Head and Tail Registers
	e1000[E1000_RDH] = 0;
	e1000[E1000_RDT] = 0;

	// Initialize the Receive Control Register
	// e1000[E1000_RCTL] = 0|E1000_RCTL_EN|E1000_RCTL_BSIZE|E1000_RCTL_SECRC;
	e1000[E1000_RCTL] = E1000_RCTL_EN;
	e1000[E1000_RCTL] &= ~E1000_RCTL_LPE;
	e1000[E1000_RCTL] &= ~E1000_RCTL_LBM;
	e1000[E1000_RCTL] &= ~E1000_RCTL_RDMTS;
	e1000[E1000_RCTL] &= ~E1000_RCTL_MO;
	e1000[E1000_RCTL] |= E1000_RCTL_BAM;
	e1000[E1000_RCTL] |= E1000_RCTL_BSIZE;
	e1000[E1000_RCTL] |= E1000_RCTL_SECRC;
	return 0;
}

static void 
init_desc_array()
{
	int i;
	memset(tx_desc_arr, 0, sizeof(struct tx_desc)*E1000_TXDESC);
	memset(tx_pkt_buf, 0, sizeof(struct tx_pkt)*E1000_TXDESC);
	for (i = 0; i < E1000_TXDESC; i++) {
		tx_desc_arr[i].addr = PADDR(tx_pkt_buf[i].buf);
		tx_desc_arr[i].status = E1000_TXD_STAT_DD;
		tx_desc_arr[i].cmd = E1000_TXD_CMD_RS|E1000_TXD_CMD_EOP;
	}
	memset(rx_desc_arr, 0, sizeof(struct rx_desc) * E1000_RXDESC);
	memset(rx_pkt_buf, 0, sizeof(struct rx_pkt) * E1000_RXDESC);
	for (i = 0; i < E1000_RXDESC; i++) {
		rx_desc_arr[i].addr = PADDR(rx_pkt_buf[i].buf);
		rx_desc_arr[i].status = 0;
	}
}

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