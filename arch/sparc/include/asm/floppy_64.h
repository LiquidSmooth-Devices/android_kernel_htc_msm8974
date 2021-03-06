/* floppy.h: Sparc specific parts of the Floppy driver.
 *
 * Copyright (C) 1996, 2007, 2008 David S. Miller (davem@davemloft.net)
 * Copyright (C) 1997 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 *
 * Ultra/PCI support added: Sep 1997  Eddie C. Dost  (ecd@skynet.be)
 */

#ifndef __ASM_SPARC64_FLOPPY_H
#define __ASM_SPARC64_FLOPPY_H

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>

#include <asm/auxio.h>

#undef PCI_FDC_SWAP_DRIVES


struct sun_flpy_controller {
	volatile unsigned char status1_82077; 
	volatile unsigned char status2_82077; 
	volatile unsigned char dor_82077;     
	volatile unsigned char tapectl_82077; 
	volatile unsigned char status_82077;  
#define drs_82077              status_82077   
	volatile unsigned char data_82077;    
	volatile unsigned char ___unused;
	volatile unsigned char dir_82077;     
#define dcr_82077              dir_82077      
};

static struct sun_flpy_controller *sun_fdc = (struct sun_flpy_controller *)-1;
unsigned long fdc_status;
static struct platform_device *floppy_op = NULL;

struct sun_floppy_ops {
	unsigned char	(*fd_inb) (unsigned long port);
	void		(*fd_outb) (unsigned char value, unsigned long port);
	void		(*fd_enable_dma) (void);
	void		(*fd_disable_dma) (void);
	void		(*fd_set_dma_mode) (int);
	void		(*fd_set_dma_addr) (char *);
	void		(*fd_set_dma_count) (int);
	unsigned int	(*get_dma_residue) (void);
	int		(*fd_request_irq) (void);
	void		(*fd_free_irq) (void);
	int		(*fd_eject) (int);
};

static struct sun_floppy_ops sun_fdops;

#define fd_inb(port)              sun_fdops.fd_inb(port)
#define fd_outb(value,port)       sun_fdops.fd_outb(value,port)
#define fd_enable_dma()           sun_fdops.fd_enable_dma()
#define fd_disable_dma()          sun_fdops.fd_disable_dma()
#define fd_request_dma()          (0) 
#define fd_free_dma()             
#define fd_clear_dma_ff()         
#define fd_set_dma_mode(mode)     sun_fdops.fd_set_dma_mode(mode)
#define fd_set_dma_addr(addr)     sun_fdops.fd_set_dma_addr(addr)
#define fd_set_dma_count(count)   sun_fdops.fd_set_dma_count(count)
#define get_dma_residue(x)        sun_fdops.get_dma_residue()
#define fd_cacheflush(addr, size) 
#define fd_request_irq()          sun_fdops.fd_request_irq()
#define fd_free_irq()             sun_fdops.fd_free_irq()
#define fd_eject(drive)           sun_fdops.fd_eject(drive)

#undef HAVE_DISABLE_HLT

static int sun_floppy_types[2] = { 0, 0 };

#define FLOPPY0_TYPE		sun_floppy_init()
#define FLOPPY1_TYPE		sun_floppy_types[1]

#define FDC1			((unsigned long)sun_fdc)

#define N_FDC    1
#define N_DRIVE  8

#define CROSS_64KB(a,s) (0)

static unsigned char sun_82077_fd_inb(unsigned long port)
{
	udelay(5);
	switch(port & 7) {
	default:
		printk("floppy: Asked to read unknown port %lx\n", port);
		panic("floppy: Port bolixed.");
	case 4: 
		return sbus_readb(&sun_fdc->status_82077) & ~STATUS_DMA;
	case 5: 
		return sbus_readb(&sun_fdc->data_82077);
	case 7: 
		
		return sbus_readb(&sun_fdc->dir_82077);
	}
	panic("sun_82072_fd_inb: How did I get here?");
}

static void sun_82077_fd_outb(unsigned char value, unsigned long port)
{
	udelay(5);
	switch(port & 7) {
	default:
		printk("floppy: Asked to write to unknown port %lx\n", port);
		panic("floppy: Port bolixed.");
	case 2: 
		
		sbus_writeb(value, &sun_fdc->dor_82077);
		break;
	case 5: 
		sbus_writeb(value, &sun_fdc->data_82077);
		break;
	case 7: 
		sbus_writeb(value, &sun_fdc->dcr_82077);
		break;
	case 4: 
		sbus_writeb(value, &sun_fdc->status_82077);
		break;
	}
	return;
}

unsigned char *pdma_vaddr;
unsigned long pdma_size;
volatile int doing_pdma = 0;

char *pdma_base = NULL;
unsigned long pdma_areasize;

static void sun_fd_disable_dma(void)
{
	doing_pdma = 0;
	if (pdma_base) {
		mmu_unlockarea(pdma_base, pdma_areasize);
		pdma_base = NULL;
	}
}

static void sun_fd_set_dma_mode(int mode)
{
	switch(mode) {
	case DMA_MODE_READ:
		doing_pdma = 1;
		break;
	case DMA_MODE_WRITE:
		doing_pdma = 2;
		break;
	default:
		printk("Unknown dma mode %d\n", mode);
		panic("floppy: Giving up...");
	}
}

static void sun_fd_set_dma_addr(char *buffer)
{
	pdma_vaddr = buffer;
}

static void sun_fd_set_dma_count(int length)
{
	pdma_size = length;
}

static void sun_fd_enable_dma(void)
{
	pdma_vaddr = mmu_lockarea(pdma_vaddr, pdma_size);
	pdma_base = pdma_vaddr;
	pdma_areasize = pdma_size;
}

irqreturn_t sparc_floppy_irq(int irq, void *dev_cookie)
{
	if (likely(doing_pdma)) {
		void __iomem *stat = (void __iomem *) fdc_status;
		unsigned char *vaddr = pdma_vaddr;
		unsigned long size = pdma_size;
		u8 val;

		while (size) {
			val = readb(stat);
			if (unlikely(!(val & 0x80))) {
				pdma_vaddr = vaddr;
				pdma_size = size;
				return IRQ_HANDLED;
			}
			if (unlikely(!(val & 0x20))) {
				pdma_vaddr = vaddr;
				pdma_size = size;
				doing_pdma = 0;
				goto main_interrupt;
			}
			if (val & 0x40) {
				
				*vaddr++ = readb(stat + 1);
			} else {
				unsigned char data = *vaddr++;

				
				writeb(data, stat + 1);
			}
			size--;
		}

		pdma_vaddr = vaddr;
		pdma_size = size;

		
		val = readb(auxio_register);
		val |= AUXIO_AUX1_FTCNT;
		writeb(val, auxio_register);
		val &= ~AUXIO_AUX1_FTCNT;
		writeb(val, auxio_register);

		doing_pdma = 0;
	}

main_interrupt:
	return floppy_interrupt(irq, dev_cookie);
}

static int sun_fd_request_irq(void)
{
	static int once = 0;
	int error;

	if(!once) {
		once = 1;

		error = request_irq(FLOPPY_IRQ, sparc_floppy_irq,
				    IRQF_DISABLED, "floppy", NULL);

		return ((error == 0) ? 0 : -1);
	}
	return 0;
}

static void sun_fd_free_irq(void)
{
}

static unsigned int sun_get_dma_residue(void)
{
	
	return 0;
}

static int sun_fd_eject(int drive)
{
	set_dor(0x00, 0xff, 0x90);
	udelay(500);
	set_dor(0x00, 0x6f, 0x00);
	udelay(500);
	return 0;
}

#include <asm/ebus_dma.h>
#include <asm/ns87303.h>

static struct ebus_dma_info sun_pci_fd_ebus_dma;
static struct device *sun_floppy_dev;
static int sun_pci_broken_drive = -1;

struct sun_pci_dma_op {
	unsigned int 	addr;
	int		len;
	int		direction;
	char		*buf;
};
static struct sun_pci_dma_op sun_pci_dma_current = { -1U, 0, 0, NULL};
static struct sun_pci_dma_op sun_pci_dma_pending = { -1U, 0, 0, NULL};

extern irqreturn_t floppy_interrupt(int irq, void *dev_id);

static unsigned char sun_pci_fd_inb(unsigned long port)
{
	udelay(5);
	return inb(port);
}

static void sun_pci_fd_outb(unsigned char val, unsigned long port)
{
	udelay(5);
	outb(val, port);
}

static void sun_pci_fd_broken_outb(unsigned char val, unsigned long port)
{
	udelay(5);
	if (port == ((unsigned long)sun_fdc) + 2) {
		if (((val & 0x03) == sun_pci_broken_drive) && (val & 0x20)) {
			val |= 0x10;
		}
	}
	outb(val, port);
}

#ifdef PCI_FDC_SWAP_DRIVES
static void sun_pci_fd_lde_broken_outb(unsigned char val, unsigned long port)
{
	udelay(5);
	if (port == ((unsigned long)sun_fdc) + 2) {
		if (((val & 0x03) == sun_pci_broken_drive) && (val & 0x10)) {
			val &= ~(0x03);
			val |= 0x21;
		}
	}
	outb(val, port);
}
#endif 

static void sun_pci_fd_enable_dma(void)
{
	BUG_ON((NULL == sun_pci_dma_pending.buf) 	||
	    (0	  == sun_pci_dma_pending.len) 	||
	    (0	  == sun_pci_dma_pending.direction));

	sun_pci_dma_current.buf = sun_pci_dma_pending.buf;
	sun_pci_dma_current.len = sun_pci_dma_pending.len;
	sun_pci_dma_current.direction = sun_pci_dma_pending.direction;

	sun_pci_dma_pending.buf  = NULL;
	sun_pci_dma_pending.len  = 0;
	sun_pci_dma_pending.direction = 0;
	sun_pci_dma_pending.addr = -1U;

	sun_pci_dma_current.addr =
		dma_map_single(sun_floppy_dev,
			       sun_pci_dma_current.buf,
			       sun_pci_dma_current.len,
			       sun_pci_dma_current.direction);

	ebus_dma_enable(&sun_pci_fd_ebus_dma, 1);

	if (ebus_dma_request(&sun_pci_fd_ebus_dma,
			     sun_pci_dma_current.addr,
			     sun_pci_dma_current.len))
		BUG();
}

static void sun_pci_fd_disable_dma(void)
{
	ebus_dma_enable(&sun_pci_fd_ebus_dma, 0);
	if (sun_pci_dma_current.addr != -1U)
		dma_unmap_single(sun_floppy_dev,
				 sun_pci_dma_current.addr,
				 sun_pci_dma_current.len,
				 sun_pci_dma_current.direction);
	sun_pci_dma_current.addr = -1U;
}

static void sun_pci_fd_set_dma_mode(int mode)
{
	if (mode == DMA_MODE_WRITE)
		sun_pci_dma_pending.direction = DMA_TO_DEVICE;
	else
		sun_pci_dma_pending.direction = DMA_FROM_DEVICE;

	ebus_dma_prepare(&sun_pci_fd_ebus_dma, mode != DMA_MODE_WRITE);
}

static void sun_pci_fd_set_dma_count(int length)
{
	sun_pci_dma_pending.len = length;
}

static void sun_pci_fd_set_dma_addr(char *buffer)
{
	sun_pci_dma_pending.buf = buffer;
}

static unsigned int sun_pci_get_dma_residue(void)
{
	return ebus_dma_residue(&sun_pci_fd_ebus_dma);
}

static int sun_pci_fd_request_irq(void)
{
	return ebus_dma_irq_enable(&sun_pci_fd_ebus_dma, 1);
}

static void sun_pci_fd_free_irq(void)
{
	ebus_dma_irq_enable(&sun_pci_fd_ebus_dma, 0);
}

static int sun_pci_fd_eject(int drive)
{
	return -EINVAL;
}

void sun_pci_fd_dma_callback(struct ebus_dma_info *p, int event, void *cookie)
{
	floppy_interrupt(0, NULL);
}

#define DOR	(port + 2)
#define MSR	(port + 4)
#define FIFO	(port + 5)

static void sun_pci_fd_out_byte(unsigned long port, unsigned char val,
			        unsigned long reg)
{
	unsigned char status;
	int timeout = 1000;

	while (!((status = inb(MSR)) & 0x80) && --timeout)
		udelay(100);
	outb(val, reg);
}

static unsigned char sun_pci_fd_sensei(unsigned long port)
{
	unsigned char result[2] = { 0x70, 0x00 };
	unsigned char status;
	int i = 0;

	sun_pci_fd_out_byte(port, 0x08, FIFO);
	do {
		int timeout = 1000;

		while (!((status = inb(MSR)) & 0x80) && --timeout)
			udelay(100);

		if (!timeout)
			break;

		if ((status & 0xf0) == 0xd0)
			result[i++] = inb(FIFO);
		else
			break;
	} while (i < 2);

	return result[0];
}

static void sun_pci_fd_reset(unsigned long port)
{
	unsigned char mask = 0x00;
	unsigned char status;
	int timeout = 10000;

	outb(0x80, MSR);
	do {
		status = sun_pci_fd_sensei(port);
		if ((status & 0xc0) == 0xc0)
			mask |= 1 << (status & 0x03);
		else
			udelay(100);
	} while ((mask != 0x0f) && --timeout);
}

static int sun_pci_fd_test_drive(unsigned long port, int drive)
{
	unsigned char status, data;
	int timeout = 1000;
	int ready;

	sun_pci_fd_reset(port);

	data = (0x10 << drive) | 0x0c | drive;
	sun_pci_fd_out_byte(port, data, DOR);

	sun_pci_fd_out_byte(port, 0x07, FIFO);
	sun_pci_fd_out_byte(port, drive & 0x03, FIFO);

	do {
		udelay(100);
		status = sun_pci_fd_sensei(port);
	} while (((status & 0xc0) == 0x80) && --timeout);

	if (!timeout)
		ready = 0;
	else
		ready = (status & 0x10) ? 0 : 1;

	sun_pci_fd_reset(port);
	return ready;
}
#undef FIFO
#undef MSR
#undef DOR

static int __init ebus_fdthree_p(struct device_node *dp)
{
	if (!strcmp(dp->name, "fdthree"))
		return 1;
	if (!strcmp(dp->name, "floppy")) {
		const char *compat;

		compat = of_get_property(dp, "compatible", NULL);
		if (compat && !strcmp(compat, "fdthree"))
			return 1;
	}
	return 0;
}

static unsigned long __init sun_floppy_init(void)
{
	static int initialized = 0;
	struct device_node *dp;
	struct platform_device *op;
	const char *prop;
	char state[128];

	if (initialized)
		return sun_floppy_types[0];
	initialized = 1;

	op = NULL;

	for_each_node_by_name(dp, "SUNW,fdtwo") {
		if (strcmp(dp->parent->name, "sbus"))
			continue;
		op = of_find_device_by_node(dp);
		if (op)
			break;
	}
	if (op) {
		floppy_op = op;
		FLOPPY_IRQ = op->archdata.irqs[0];
	} else {
		struct device_node *ebus_dp;
		void __iomem *auxio_reg;
		const char *state_prop;
		unsigned long config;

		dp = NULL;
		for_each_node_by_name(ebus_dp, "ebus") {
			for (dp = ebus_dp->child; dp; dp = dp->sibling) {
				if (ebus_fdthree_p(dp))
					goto found_fdthree;
			}
		}
	found_fdthree:
		if (!dp)
			return 0;

		op = of_find_device_by_node(dp);
		if (!op)
			return 0;

		state_prop = of_get_property(op->dev.of_node, "status", NULL);
		if (state_prop && !strncmp(state_prop, "disabled", 8))
			return 0;

		FLOPPY_IRQ = op->archdata.irqs[0];

		auxio_reg = (void __iomem *) op->resource[2].start;
		writel(readl(auxio_reg)|0x2, auxio_reg);

		sun_floppy_dev = &op->dev;

		spin_lock_init(&sun_pci_fd_ebus_dma.lock);

		
		sun_pci_fd_ebus_dma.regs = (void __iomem *)
			op->resource[1].start;
		if (!sun_pci_fd_ebus_dma.regs)
			return 0;

		sun_pci_fd_ebus_dma.flags = (EBUS_DMA_FLAG_USE_EBDMA_HANDLER |
					     EBUS_DMA_FLAG_TCI_DISABLE);
		sun_pci_fd_ebus_dma.callback = sun_pci_fd_dma_callback;
		sun_pci_fd_ebus_dma.client_cookie = NULL;
		sun_pci_fd_ebus_dma.irq = FLOPPY_IRQ;
		strcpy(sun_pci_fd_ebus_dma.name, "floppy");
		if (ebus_dma_register(&sun_pci_fd_ebus_dma))
			return 0;

		
		sun_fdc = (struct sun_flpy_controller *) op->resource[0].start;

		sun_fdops.fd_inb = sun_pci_fd_inb;
		sun_fdops.fd_outb = sun_pci_fd_outb;

		can_use_virtual_dma = use_virtual_dma = 0;
		sun_fdops.fd_enable_dma = sun_pci_fd_enable_dma;
		sun_fdops.fd_disable_dma = sun_pci_fd_disable_dma;
		sun_fdops.fd_set_dma_mode = sun_pci_fd_set_dma_mode;
		sun_fdops.fd_set_dma_addr = sun_pci_fd_set_dma_addr;
		sun_fdops.fd_set_dma_count = sun_pci_fd_set_dma_count;
		sun_fdops.get_dma_residue = sun_pci_get_dma_residue;

		sun_fdops.fd_request_irq = sun_pci_fd_request_irq;
		sun_fdops.fd_free_irq = sun_pci_fd_free_irq;

		sun_fdops.fd_eject = sun_pci_fd_eject;

		fdc_status = (unsigned long) &sun_fdc->status_82077;

		if (1) {
			sun_pci_broken_drive = 1;
			sun_fdops.fd_outb = sun_pci_fd_broken_outb;
		}

		allowed_drive_mask = 0;
		if (sun_pci_fd_test_drive((unsigned long)sun_fdc, 0))
			sun_floppy_types[0] = 4;
		if (sun_pci_fd_test_drive((unsigned long)sun_fdc, 1))
			sun_floppy_types[1] = 4;

		config = 0;
		for (dp = ebus_dp->child; dp; dp = dp->sibling) {
			if (!strcmp(dp->name, "ecpp")) {
				struct platform_device *ecpp_op;

				ecpp_op = of_find_device_by_node(dp);
				if (ecpp_op)
					config = ecpp_op->resource[1].start;
				goto config_done;
			}
		}
	config_done:

		switch (config & 0x3ff) {
		case 0x02e:
		case 0x15c:
		case 0x26e:
		case 0x398:
			break;
		default:
			config = 0;
		}

		if (!config)
			return sun_floppy_types[0];

		
		ns87303_modify(config, ASC, 0, 0xc0);

#ifdef PCI_FDC_SWAP_DRIVES
		if (!sun_floppy_types[0] && sun_floppy_types[1]) {
			ns87303_modify(config, FER, FER_EDM, 0);
			ns87303_modify(config, ASC, ASC_DRV2_SEL, 0);
			ns87303_modify(config, FCR, 0, FCR_LDE);

			config = sun_floppy_types[0];
			sun_floppy_types[0] = sun_floppy_types[1];
			sun_floppy_types[1] = config;

			if (sun_pci_broken_drive != -1) {
				sun_pci_broken_drive = 1 - sun_pci_broken_drive;
				sun_fdops.fd_outb = sun_pci_fd_lde_broken_outb;
			}
		}
#endif 

		return sun_floppy_types[0];
	}
	prop = of_get_property(op->dev.of_node, "status", NULL);
	if (prop && !strncmp(state, "disabled", 8))
		return 0;

	sun_fdc = (struct sun_flpy_controller *)
		(op->resource[0].start +
		 ((op->resource[0].flags & 0x1ffUL) << 32UL));

	
	if (sbus_readb(&sun_fdc->status1_82077) == 0xff) {
		sun_fdc = (struct sun_flpy_controller *)-1;
		return 0;
	}

        sun_fdops.fd_inb = sun_82077_fd_inb;
        sun_fdops.fd_outb = sun_82077_fd_outb;

	can_use_virtual_dma = use_virtual_dma = 1;
	sun_fdops.fd_enable_dma = sun_fd_enable_dma;
	sun_fdops.fd_disable_dma = sun_fd_disable_dma;
	sun_fdops.fd_set_dma_mode = sun_fd_set_dma_mode;
	sun_fdops.fd_set_dma_addr = sun_fd_set_dma_addr;
	sun_fdops.fd_set_dma_count = sun_fd_set_dma_count;
	sun_fdops.get_dma_residue = sun_get_dma_residue;

	sun_fdops.fd_request_irq = sun_fd_request_irq;
	sun_fdops.fd_free_irq = sun_fd_free_irq;

	sun_fdops.fd_eject = sun_fd_eject;

        fdc_status = (unsigned long) &sun_fdc->status_82077;

	
	allowed_drive_mask = 0x01;
	sun_floppy_types[0] = 4;
	sun_floppy_types[1] = 0;

	return sun_floppy_types[0];
}

#define EXTRA_FLOPPY_PARAMS

static DEFINE_SPINLOCK(dma_spin_lock);

#define claim_dma_lock() \
({	unsigned long flags; \
	spin_lock_irqsave(&dma_spin_lock, flags); \
	flags; \
})

#define release_dma_lock(__flags) \
	spin_unlock_irqrestore(&dma_spin_lock, __flags);

#endif 
