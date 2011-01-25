/* mmio_test.c - Test MMIO read latency of various network adaptors
 *
 * (C) 2005 by Robert Olsson & Harald Welte
 *
 * This program is licensed under GNU GPL Version 2 as published by
 * the Free Software Foundation.
 */

/*

Part of it is that uncached accesses are plain slow.  An L2 miss is ~370
cycles on my hardware (155ns*2.4 confirms that), but an uncached access
to the same memory location is consistently ~490 cycles.

And part of it seems to be the e1000.  Reading the device control register
(E1000_CTRL, offset 0x00) is ~1700 cycles each, but reading the interrupt
cause register (E1000_ICR, 0xc0) is ~2100 cycles each.  The interrupt mask
register (E1000_IMS, 0x100) is also ~2100 cycles each.

*/

#if 1

#define rdtscll(val) \
     __asm__ __volatile__("rdtsc" : "=A" (val))


  //#define rdtscll(low,high) \
  //   __asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

#else
#define rdtscll(val) do { \
	unsigned int __a,__d; \
	asm volatile("rdtsc" : "=a" (__a), "=d" (__d)); \
	(val) = ((unsigned long)__a) | (((unsigned long)__d)<<32); \
} while(0)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <pci/pci.h>

#define OUTER_LOOP_COUNT	128
#define INNER_LOOP_COUNT	65535

struct reg_desc {
	char *name;
	unsigned int offset;
};

struct network_device {
	unsigned char *name;
	unsigned int vendor;
	unsigned int *devices;

	unsigned int reg_num;
	struct reg_desc *reg_desc;

	int (*testfn)(struct network_device *n, struct pci_dev *p);
};

static int generic_test(struct network_device *n, struct pci_dev *p);

static int dev_mem_fd;


/* e1000 */

static struct reg_desc e1000_reg_desc[] = {
	{
		.name = "device control",
		.offset = 0x00,
	},
	{
		.name = "interrupt cause",
		.offset = 0xc0,
	},
	{
		.name = "RX control",
		.offset = 0x100,
	},
};

static unsigned int e1000_dev_ids[] = {
	0x1000, 0x1001, 0x1004, 0x1008, 0x1009, 0x100c, 0x100d, 0x100e,
	0x100f, 0x1010, 0x1011, 0x1012, 0x1013, 0x1014, 0x1015, 0x1016,
	0x1017, 0x1018, 0x1019, 0x101d, 0x101e, 0x1026, 0x1027, 0x1028,
	0x1075, 0x1076, 0x1077, 0x1078, 0x1079, 0x107a, 0x107b, 0x107c,
	0x108a,
	0 
};

static struct network_device e1000_pci_ids = {
	.name = "e1000",
	.vendor = PCI_VENDOR_ID_INTEL,
	.devices = &e1000_dev_ids,
	.testfn = &generic_test,
	.reg_desc = &e1000_reg_desc,
	.reg_num = 3,
};

/* tg3 */

#ifndef PCI_VENDOR_ID_BROADCOM
#define PCI_VENDOR_ID_BROADCOM	0x14e4
#endif

#define TG3PCI_PCISTATE		0x00000070
#define MAILBOX_INTERRUPT_0	0x00000200
#define TG3_64BIT_REG_LOW	0x04UL

static struct reg_desc tg3_reg_desc[] = {
	{
		.name = "PCI State (only in INTx case)",
		.offset = TG3PCI_PCISTATE,
	},
	{
		.name = "Mailbox Interrupt 0",
		.offset = MAILBOX_INTERRUPT_0 + TG3_64BIT_REG_LOW,
	},
};


static unsigned int tg3_dev_ids[] = {
	0x1600, 0x1601, 0x1644, 0x1645, 0x1646, 0x1647, 0x1648, 0x1649,
	0x165d, 0x1653, 0x1654, 0x1658, 0x1659, 0x165d, 0x165e, 0x166e,
	0x1676, 0x1677, 0x167c, 0x167d, 0x167e, 0x1696, 0x169c, 0x169d,
	0x16a6, 0x16a7, 0x16a8, 0x16c6, 0x16dd, 0x16f7, 0x16fd, 0x170d,
	0x170e,
	0
};

static struct network_device tg3_pci_ids = {
	.name = "tg3",
	.vendor = PCI_VENDOR_ID_BROADCOM,
	.devices = &tg3_dev_ids,
	.testfn = &generic_test,
	.reg_desc = &tg3_reg_desc,
	.reg_num = 2,
};


/* core */

static int test_backend(volatile void *x, unsigned long offset)
{
	int i;

	for (i = 0; i <= INNER_LOOP_COUNT; i++) 
		*((volatile unsigned long *)x + offset);
}

static int generic_test(struct network_device *n, struct pci_dev *p)
{
	unsigned long long start, end;
	unsigned long long regs = p->base_addr[0] &  PCI_ADDR_MEM_MASK;
	volatile void *x;
	int h;
	int loop = 4;

	printf("Using regs = 0x%llx\n", regs);
	x = mmap(NULL, 4096, PROT_READ, MAP_SHARED, dev_mem_fd, regs);
	if (x == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	for (h = 0; h < n->reg_num; h++) {
		int i;
		printf("%30s (%04x): ", n->reg_desc[h].name, 
			n->reg_desc[h].offset);
		fflush(stdout);
		for (i = 0; i < loop; i++) {
			int j;

			rdtscll(start);
			for (j = 0; j <= OUTER_LOOP_COUNT; j++)
				test_backend(x, n->reg_desc[h].offset);
			rdtscll(end);
	 
			printf("%Li ", (end - start) / 
				       (OUTER_LOOP_COUNT * INNER_LOOP_COUNT));
			fflush(stdout);
		}
		printf("\n");
		fflush(stdout);
	}
	munmap((void *)x, 4096);

	return 0;
}

static int
get_device(struct network_device *list, struct pci_dev *dev)
{
	int i;

	if (dev->vendor_id != list->vendor)
		return 0;

	for (i = 0; list->devices[i]; i++) {
		if (list->devices[i] == dev->device_id) {
#ifdef CONFIG_PCI_DOMAINS
			printf("Testing Device ");
			if (dev->domain)
				printf("%04x:", dev->domain);
#endif
			printf("%02x:%02x.%d (%04x:%04x): %s\n", dev->bus,
				dev->dev, dev->func, dev->vendor_id, 
				dev->device_id, list->name);
			if (list->testfn(list, dev) < 0)
				return -1;
		}
	}
	return 0;

}

int main(int argc, char **argv)
{
	struct pci_access *pci_a;
	struct pci_dev *p;

	printf("%s - MMIO latency test program\n", argv[0]);
	printf("\nWARNING: THIS PROGRAM WILL USE LOW-LEVEL ACCESS TO THE "
		"HARDWARE AND THUS LIKELY INTERFERE WITH THE DRIVER\n\n");

	dev_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (dev_mem_fd < 0) {
		perror("open");
		fprintf(stderr, "you must run this as root, sorry\n");
		exit(-1);
	}
   
	pci_a = pci_alloc();
	if (!pci_a) {
		perror("pci_alloc");
		exit(-1);
	}

	pci_init(pci_a);
	pci_scan_bus(pci_a);

	for (p = pci_a->devices; p; p = p->next) {
		get_device(&e1000_pci_ids, p);
		get_device(&tg3_pci_ids, p);
		/* add more here */
	}
	exit(0);
}
