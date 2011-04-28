/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MAC driver for the ARCTangent EMAC 10100 (Rev 5)
 * File : drivers/net/arctangent_emac.c
 * Author(s) : amit bhor (Codito Tech.), Sameer Dhavale
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>


#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/init.h>
#include <linux/inetdevice.h>
#include <linux/inet.h>
#include <linux/proc_fs.h>

#include <asm/cacheflush.h>
#include <asm/irq.h>


/* clock speed is set while parsing parameters in setup.c */
extern unsigned long clk_speed;

/*
 * This points to the last detected device. All aa3 vmac devices are linked
 * using the prev_dev pointer in aa3_emac_priv structure
 */
static struct net_device *aa3_root_dev;

/* Timeout time for tranmission */
#define TX_TIMEOUT (400*HZ/1000)

#define AUTO_NEG_TIMEOUT 2000

/* Buffer descriptors */
#define TX_BDT_LEN	128	/* Number of recieve BD's */
#define RX_BDT_LEN	128	/* Number of transmit BD's */
#define SKB_PREALLOC    256	/* Number of cached SKB's */
#define NAPI_WEIGHT 40      /* workload for NAPI */


void            arc_thread(void);
static int aa3_poll (struct napi_struct *napi, int budget);
static int  aa3_clean(struct net_device *netdev, unsigned int *work_done, unsigned int work_to_do);
void spinlock_test(void );
static int aa3_tx_clean(struct net_device *netdev);
void thread2 (void);
static irqreturn_t aa3_emac_intr (int irq, void *dev_instance);

//Stats proc file system

int             read_proc(char *sysbuf, char **p_mybuf, off_t offset, int l_sysbuf, int zero);
struct proc_dir_entry *myproc;
//debug interface.

// Pre - allocated(cached) SKB 's.  Improve driver performance by allocating SKB' s
// in advance of when they are needed.

	volatile struct sk_buff *skb_prealloc[SKB_PREALLOC];
//pre - allocated SKB 's
	volatile unsigned int skb_count = 0;

//EMAC stats.Turn these on to get more information from the EMAC
// Turn them off to get better performance.
// note:If you see UCLO it 's bad.

#define EMAC_STATS 1
	unsigned int    emac_drop = 0;
	unsigned int    emac_defr = 0;
	unsigned int    emac_ltcl = 0;
	unsigned int    emac_uflo = 0;
	unsigned int    emac_rtry = 0;
	unsigned int    skb_preallocated_used = 0;
	unsigned int    skb_not_preallocated = 0;


/*
 * Size of RX buffers, min = 0 (pointless) max = 2048 (MAX_RX_BUFFER_LEN) MAC
 * reference manual recommends a value slightly greater than the maximum size
 * of the packet expected other wise it will chain a zero size buffer desc if
 * a packet of exactly RX_BUFFER_LEN comes. VMAC will chain buffers if a
 * packet bigger than this arrives.
 */
#define RX_BUFFER_LEN	(ETH_FRAME_LEN + 4)

#define MAX_RX_BUFFER_LEN	0x800	/* 2^11 = 2048 = 0x800 */
#define MAX_TX_BUFFER_LEN	0x800	/* 2^11 = 2048 = 0x800 */

/*
 * 14 bytes of ethernet header, 8 bytes pad to prevent buffer chaining of
 * maximum sized ethernet packets (1514 bytes)
 */
#define	VMAC_BUFFER_PAD ETH_HLEN + 8

/* VMAC register definitions, offsets in the ref manual are in bytes */
#define ID_OFFSET 0x0/0x4
#define MDIO_DATA_OFFSET 0x34/0x4
#define ADDRL_OFFSET 0x24/0x4
#define ADDRH_OFFSET 0x28/0x4
#define RXRINGPTR_OFFSET 0x20/0x4
#define TXRINGPTR_OFFSET 0x1c/0x4
#define POLLRATE_OFFSET	0x10/0x4
#define ENABLE_OFFSET	0x08/0x4
#define STAT_OFFSET	0x04/0x4
#define CONTROL_OFFSET	0x0c/0x4
#define LAFL_OFFSET	0x2c/0x4
#define LAFH_OFFSET	0x30/0x4
#define RXERR_OFFSET	0x14/0x4
#define MISS_OFFSET	0x18/0x4

/* STATUS and ENABLE Register bit masks */
#define TXINT_MASK	(1<<0)	/* Transmit interrupt */
#define RXINT_MASK	(1<<1)	/* Recieve interrupt */
#define ERR_MASK	(1<<2)	/* Error interrupt */
#define TXCH_MASK	(1<<3)	/* Transmit chaining error interrupt */
#define MSER_MASK	(1<<4)	/* Missed packet counter error */
#define RXCR_MASK	(1<<8)	/* RXCRCERR counter rolled over	 */
#define RXFR_MASK	(1<<9)	/* RXFRAMEERR counter rolled over */
#define RXFL_MASK	(1<<10)	/* RXOFLOWERR counter rolled over */
#define MDIO_MASK	(1<<12)	/* MDIO complete */
#define TXPL_MASK	(1<<31)	/* TXPOLL */

/* CONTROL Register bit masks */
#define EN_MASK		(1<<0)	/* VMAC enable */
#define TXRN_MASK	(1<<3)	/* TX enable */
#define RXRN_MASK	(1<<4)	/* RX enable */
#define DSBC_MASK	(1<<8)	/* Disable recieve broadcast */
#define ENFL_MASK	(1<<10)	/* Enable Full Duplex */
#define PROM_MASK	(1<<11)	/* Promiscuous mode */

/* Buffer descriptor INFO bit masks */
#define OWN_MASK	(1<<31)	/* 0-CPU owns buffer, 1-VMAC owns buffer */
#define FRST_MASK	(1<<16)	/* First buffer in chain */
#define LAST_MASK	(1<<17)	/* Last buffer in chain */
#define LEN_MASK	0x000007FF	/* last 11 bits */
#define CRLS            (1<<21)
#define DEFR            (1<<22)
#define DROP            (1<<23)
#define RTRY            (1<<24)
#define LTCL            (1<<28)
#define UFLO            (1<<29)

/* ARCangel board PHY Identifier */
#define PHY_ID	0x3

/* LXT971 register definitions */
#define LXT971A_CTRL_REG               0x00
#define LXT971A_STATUS_REG             0x01
#define LXT971A_AUTONEG_ADV_REG        0x04
#define LXT971A_AUTONEG_LINK_REG       0x05
#define LXT971A_MIRROR_REG             0x10
#define LXT971A_STATUS2_REG            0x11

/* LXT971A control register bit definitions */
#define LXT971A_CTRL_RESET         0x8000
#define LXT971A_CTRL_LOOPBACK      0x4000
#define LXT971A_CTRL_SPEED         0x2000
#define LXT971A_CTRL_AUTONEG       0x1000
#define LXT971A_CTRL_DUPLEX        0x0100
#define LXT971A_CTRL_RESTART_AUTO  0x0200
#define LXT971A_CTRL_COLL          0x0080
#define LXT971A_CTRL_DUPLEX_MODE   0x0100

/* Auto-negatiation advertisement register bit definitions */
#define LXT971A_AUTONEG_ADV_100BTX_FULL     0x100
#define LXT971A_AUTONEG_ADV_100BTX          0x80
#define LXT971A_AUTONEG_ADV_10BTX_FULL      0x40
#define LXT971A_AUTONEG_ADV_10BT            0x20
#define AUTONEG_ADV_IEEE_8023               0x1

/* Auto-negatiation Link register bit definitions */
#define LXT971A_AUTONEG_LINK_100BTX_FULL     0x100
#define LXT971A_AUTONEG_LINK_100BTX          0x80
#define LXT971A_AUTONEG_LINK_10BTX_FULL      0x40

/* Status register bit definitions */
#define LXT971A_STATUS_COMPLETE     0x20
#define LXT971A_STATUS_AUTONEG_ABLE 0x04

/* Status2 register bit definitions */
#define LXT971A_STATUS2_COMPLETE    0x80
#define LXT971A_STATUS2_POLARITY    0x20
#define LXT971A_STATUS2_NEGOTIATED  0x100
#define LXT971A_STATUS2_FULL        0x200
#define LXT971A_STATUS2_LINK_UP     0x400
#define LXT971A_STATUS2_100         0x4000


// #define ARCTANGENT_EMAC_SETUP
// #define ARCTANGENT_EMAC_DEBUG

#ifdef ARCTANGENT_EMAC_DEBUG
#define dbg_printk(fmt, args...)	\
		printk ("ARCTangent emac: "); \
		printk (fmt, ## args);
#else
#define dbg_printk(fmt, args...)
#endif

#define __mdio_write(priv, mdio_data_reg, phy_id, phy_reg, val)	\
{                                                   \
priv->mdio_complete = 0;				\
		*(mdio_data_reg) = (0x50020000 | phy_id << 23 | \
					phy_reg << 18 | (val & 0xffff)) ; \
		while (!priv->mdio_complete);			\
}

#define __mdio_read(priv, mdio_data_reg, phy_id, phy_reg, val)	\
{                                                   \
		priv->mdio_complete = 0;				\
		*(mdio_data_reg) = (0x60020000 | phy_id << 23 | phy_reg << 18 );	\
		while (!priv->mdio_complete);			\
		val = *(mdio_data_reg); \
		val &= 0xffff;              \
}

	struct aa3_buffer_desc
	{
		unsigned int    info;
		void           *data;
	};

	struct aa3_emac_priv
	{
		struct net_device_stats stats;
		/* base address of the register set for this device */
		int            *reg_base_addr;
		struct net_device *prev_dev;
		spinlock_t      lock;
		/*
		 * rx buffer descriptor. We align descriptors to 32 so info
		 * and data lie on the same cache line. This also satisfies
		 * the 8 byte alignment required by the VMAC
		 */
		struct aa3_buffer_desc rxbd[RX_BDT_LEN] __attribute__((aligned(32)));
		/* tx buffer descriptor */
		struct aa3_buffer_desc txbd[TX_BDT_LEN] __attribute__((aligned(32)));
		/* The actual socket buffers */
		struct sk_buff *rx_skbuff[RX_BDT_LEN];
		struct sk_buff *tx_skbuff[TX_BDT_LEN];
		unsigned int    txbd_curr;
		unsigned int    txbd_dirty;
		/*
		 * Set by interrupt handler to indicate completion of mdio
		 * operation. reset by reader (see __mdio_read())
		 */
		volatile unsigned int mdio_complete;
        struct napi_struct napi;
	};

/*
 * This MAC addr is given to the first opened EMAC, the last byte will be
 * incremented by 1 each time so succesive emacs will get a different
 * hardware address
 */
	extern struct sockaddr mac_addr;	/* Intialised while
						 * processing parameters in
						 * setup.c */
/*
 * static struct sockaddr mac_addr = { 0,
 *
 * { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05 } };
 */

	int             aa3_emac_open(struct net_device * dev);
	int             aa3_emac_stop(struct net_device * dev);
	int             aa3_emac_ioctl(struct net_device * dev, struct ifreq * rq, int cmd);
	struct net_device_stats *aa3_emac_stats(struct net_device * dev);
	void            aa3_emac_update_stats(struct aa3_emac_priv * ap);
	int             aa3_emac_tx(struct sk_buff * skb, struct net_device * dev);
	void            aa3_emac_tx_timeout(struct net_device * dev);
	void            aa3_emac_set_multicast_list(struct net_device * dev);
	int             aa3_emac_set_address(struct net_device * dev, void *p);
	int             aa3_emac_init(struct net_device * dev);

static void     dump_phy_status(unsigned int status)
{

/* Intel LXT971A STATUS2, bit 5 says "Polarity" is reversed if set. */
/* Not exactly sure what this means, but if I invert the bits they seem right */

    if (status & LXT971A_STATUS2_POLARITY)
        status=~status;

	printk("LXT971A: 0x%08x ", status);

	if (status & LXT971A_STATUS2_100)
		printk("100mbps, ");
	else
		printk("10mbps, ");
	if (status & LXT971A_STATUS2_FULL)
		printk("full duplex, ");
	else
		printk("half duplex, ");
	if (status & LXT971A_STATUS2_NEGOTIATED)
	{
		printk("auto-negotiation ");
		if (status & LXT971A_STATUS2_COMPLETE)
			printk("complete, ");
		else
			printk("in progress, ");
	} else
		printk("manual mode, ");
	if (status & LXT971A_STATUS2_LINK_UP)
		printk("link is up\n");
	else
		printk("link is down\n");
}


#ifdef CONFIG_EMAC_NAPI

static int aa3_poll (struct napi_struct *napi, int budget)
{


   struct net_device *netdev = aa3_root_dev;
   struct aa3_emac_priv *ap = (struct aa3_emac_priv *) netdev->priv;
   volatile unsigned int *reg;
   unsigned int work_done = 0;



     aa3_clean(netdev,&work_done, budget);
//     aa3_tx_clean(netdev);
//    printk("%u\n", work_done);
        if(work_done < budget)
        {
            netif_rx_complete(netdev, napi);
            reg = ap->reg_base_addr + ENABLE_OFFSET;
            *reg |= RXINT_MASK;
        }

//    printk("work done %u budget %u\n", work_done, budget);
    return(work_done);
}


static int aa3_clean(struct net_device *netdev, unsigned int *work_done, unsigned int work_to_do)

{
    unsigned int    flags;
	struct net_device *dev = (struct net_device *) aa3_root_dev;
	struct aa3_emac_priv *ap = (struct aa3_emac_priv *) dev->priv;
	volatile unsigned int *reg;
	unsigned int    status, len, i, info;
	struct sk_buff *skb, *skbnew;


//printk("work done %p, *work_done %u\n", work_done, *work_done);

		for (i = 0; i < RX_BDT_LEN; i++)
		{
			/* Why not to go in a round-robin order */
			info = arc_read_uncached_32(&ap->rxbd[i].info);
			if ((info & OWN_MASK) == 0)
			{
				if ((info & FRST_MASK) && (info & LAST_MASK))
				{
					len = info & LEN_MASK;
					ap->stats.rx_packets++;
					ap->stats.rx_bytes += len;

					if (skb_count)
					{
						skb_preallocated_used++;
						skbnew = skb_prealloc[skb_count];
						skb_count--;
					} else
					{
						if (!(skbnew = dev_alloc_skb(dev->mtu + VMAC_BUFFER_PAD)))
						{
							printk(KERN_INFO "Out of Memory, dropping packet\n");
							/*
							 * return buffer to
							 * VMAC
							 */
							arc_write_uncached_32(&ap->rxbd[i].info,
									      (OWN_MASK | (dev->mtu + VMAC_BUFFER_PAD)));
							ap->stats.rx_dropped++;
						} else
						{

							skb_not_preallocated++;
						}
					}

					if (skbnew)
					{
						skb = ap->rx_skbuff[i];
//						dbg_printk("RXINT: invalidating cache for data: 0x%lx-0x%lx\n",
//							   skb->data, skb->data + skb->len);
						inv_dcache_range(skb->data, skb->data + len);
						//flush_dcache_all();
						/*
						 * IP header Alignment (14
						 * byte Ethernet header)
						 */
						skb_reserve(skbnew, 2);
						/*
						 * replace by newly allocated
						 * sk buff
						 */
						arc_write_uncached_32(&ap->rxbd[i].data, skbnew->data);
						ap->rx_skbuff[i] = skbnew;
						/*
						 * Set length and give buffer
						 * to VMAC
						 */
						arc_write_uncached_32(&ap->rxbd[i].info,
								      (OWN_MASK | (dev->mtu + VMAC_BUFFER_PAD)));
						/*
						 * Give arrived sk buff to
						 * system
						 */
						skb->dev = dev;
						skb_put(skb, len - 4);	/* Make room for data */
						skb->protocol = eth_type_trans(skb, dev);

                        if(*work_done >= work_to_do)
				            dev_kfree_skb_irq(skb);  // drop packet
                        else
                        {
                            *work_done = *work_done +1;
						    netif_receive_skb(skb);
                        }

//                        printk("wd after %u\n", *work_done);
#if 0
                        if(work_done && *work_done >= work_to_do)
                        {
//                           printk("too much work wd %u wtd %u\n",*work_done,work_to_do);
                            return -EAGAIN;
                        }
#endif
					}
            }
    }
    }

}

static irqreturn_t aa3_emac_intr (int irq, void *dev_instance)
{

	struct net_device *dev = (struct net_device *) dev_instance;
	struct aa3_emac_priv *ap = (struct aa3_emac_priv *) dev->priv;
	volatile unsigned int *reg;
    volatile unsigned int *mask;
    volatile unsigned int *rxring;

	unsigned int    status, len, i, info,enable, rx_ring,mdio_reg;
	struct sk_buff *skb, *skbnew;

	reg = ap->reg_base_addr + STAT_OFFSET;
	status = (*reg);
	(*reg) = status;
    mask = ap->reg_base_addr + ENABLE_OFFSET;
    enable = (*mask);

    rxring = ap->reg_base_addr + RXRINGPTR_OFFSET;
   rx_ring = *rxring;

    reg = ap->reg_base_addr + MDIO_DATA_OFFSET;

//    __mdio_read(ap,reg,PHY_ID,LXT971A_STATUS_REG, mdio_reg);

	/*
	 * We dont allow chaining of recieve packets. We want to do "zero
	 * copy" and sk_buff structure is not chaining friendly when we dont
	 * want to copy data. We preallocate buffers of MTU size so incoming
	 * packets wont be chained
	 */
	if (status & RXINT_MASK)
	{
        if(likely(netif_rx_schedule_prep(dev, &ap->napi)))
        {
//            printk("disable ints\n");
            reg = ap->reg_base_addr + ENABLE_OFFSET;;
            *reg &= ~RXINT_MASK; // no more interrupts.
            __netif_rx_schedule(dev,&ap->napi);
        }
    }

    if (status & MDIO_MASK)
	{
//        printk("MDIO INT\n");
		/*
		 * Mark the mdio operation as complete. This is reset by the
		 * MDIO reader
		 */
		ap->mdio_complete = 1;
	}

    if (status & TXINT_MASK)
    {
        aa3_tx_clean(dev);
    }

	if (status & ERR_MASK)
	{
//		printk(KERN_DEBUG "ARCTangent Vmac: Error...status = 0x%x\n", status);
		if (status & TXCH_MASK)
		{
			ap->stats.tx_errors++;
			ap->stats.tx_aborted_errors++;
			printk(KERN_ERR "Transmit_chaining error! txbd_dirty = %u\n", ap->txbd_dirty);
		} else if (status & MSER_MASK)
		{
			ap->stats.rx_missed_errors += 255;
			ap->stats.rx_errors += 255;
		} else if (status & RXCR_MASK)
		{
			ap->stats.rx_crc_errors += 255;
			ap->stats.rx_errors += 255;
		} else if (status & RXFR_MASK)
		{
			ap->stats.rx_frame_errors += 255;
			ap->stats.rx_errors += 255;
		} else if (status & RXFL_MASK)
		{
			ap->stats.rx_over_errors += 255;
			ap->stats.rx_errors += 255;
		} else
		{
			printk(KERN_ERR "ARCTangent Vmac: Unkown Error status = 0x%x\n", status);
		}
	}
return IRQ_HANDLED;
}


static int aa3_tx_clean(struct net_device *netdev)
{
    unsigned int    flags;
	struct net_device *dev = (struct net_device *) aa3_root_dev;
	struct aa3_emac_priv *ap = (struct aa3_emac_priv *) dev->priv;
	volatile unsigned int *reg;
	unsigned int    status, len, i, info;
	struct sk_buff *skb, *skbnew;

#if 0
    reg = ap->reg_base_addr + STAT_OFFSET;
    status = (*reg);
    (*reg) = status;

	if (status & TXINT_MASK)
	{

#endif

		/*
		 * Kind of short circuiting code taking advantage of the fact
		 * that the ARC emac will release multiple "sent" packets in
		 * order. So if this interrupt was not for the current
		 * (txbd_dirty) packet then no other "next" packets were
		 * sent. The manual says that TX interrupt can occur even if
		 * no packets were sent and there may not be 1 to 1
		 * correspondence of interrupts and number of packets queued
		 * to send
		 */
		for (i = 0; i < TX_BDT_LEN; i++)
		{
			info = arc_read_uncached_32(&ap->txbd[ap->txbd_dirty].info);

#ifdef EMAC_STATS
			//if (info & RTRY)
				//printk(KERN_CRIT "had to retry\n");

			if (info & DROP)
				emac_drop++;
			if (info & DEFR)
				emac_defr++;
			if (info & LTCL)
				emac_ltcl++;
			if (info & UFLO)
				emac_uflo++;
#endif
			if (info & OWN_MASK)
			{
				dbg_printk("TXINT, OWN_MASK set\n");
				goto err_int;
			}
			if (!(arc_read_uncached_32(&ap->txbd[ap->txbd_dirty].data)))
			{
				dbg_printk("TXINT, no data !!!\n");
				goto err_int;
			}
			if (info & FRST_MASK)
			{
				skb = ap->tx_skbuff[ap->txbd_dirty];
				ap->stats.tx_packets++;
				ap->stats.tx_bytes += skb->len;
				/* return the sk_buff to system */
				dev_kfree_skb_irq(skb);
			}
			arc_write_uncached_32(&ap->txbd[ap->txbd_dirty].data, 0x0);
			arc_write_uncached_32(&ap->txbd[ap->txbd_dirty].info, 0x0);
			ap->txbd_dirty = (ap->txbd_dirty + 1) % TX_BDT_LEN;
		}
/*	} TXINT */
err_int:
	return IRQ_HANDLED;


}


#else


/* aa3 emac interrupt handler */
static irqreturn_t
aa3_emac_intr(int irq, void *dev_instance)
{
	struct net_device *dev = (struct net_device *) dev_instance;
	struct aa3_emac_priv *ap = netdev_priv(dev);
	volatile unsigned int *reg;
	unsigned int    status, len, i, info;
	struct sk_buff *skb, *skbnew;

	/* Check what kind of interrupt */
	reg = ap->reg_base_addr + STAT_OFFSET;
	status = (*reg);
	(*reg) = status;
	/*
	 * We dont allow chaining of recieve packets. We want to do "zero
	 * copy" and sk_buff structure is not chaining friendly when we dont
	 * want to copy data. We preallocate buffers of MTU size so incoming
	 * packets wont be chained
	 */
	if (status & RXINT_MASK)
	{
		for (i = 0; i < RX_BDT_LEN; i++)
		{
			/* Why not to go in a round-robin order */
			info = arc_read_uncached_32(&ap->rxbd[i].info);
			if ((info & OWN_MASK) == 0)
			{
				if ((info & FRST_MASK) && (info & LAST_MASK))
				{
					len = info & LEN_MASK;
					ap->stats.rx_packets++;
					ap->stats.rx_bytes += len;

					if (skb_count)
					{
						skb_preallocated_used++;
						skbnew = skb_prealloc[skb_count];
						skb_count--;
					} else
					{
						if (!(skbnew = dev_alloc_skb(dev->mtu + VMAC_BUFFER_PAD)))
						{
							printk(KERN_INFO "Out of Memory, dropping packet\n");
							/*
							 * return buffer to
							 * VMAC
							 */
							arc_write_uncached_32(&ap->rxbd[i].info,
									      (OWN_MASK | (dev->mtu + VMAC_BUFFER_PAD)));
							ap->stats.rx_dropped++;
						} else
						{

							skb_not_preallocated++;
						}
					}

					if (skbnew)
					{
						skb = ap->rx_skbuff[i];
//						dbg_printk("RXINT: invalidating cache for data: 0x%lx-0x%lx\n",
//							   skb->data, skb->data + skb->len);
						inv_dcache_range(skb->data, skb->data + len);
						//flush_dcache_all();
						/*
						 * IP header Alignment (14
						 * byte Ethernet header)
						 */
						skb_reserve(skbnew, 2);
						/*
						 * replace by newly allocated
						 * sk buff
						 */
						arc_write_uncached_32(&ap->rxbd[i].data, skbnew->data);
						ap->rx_skbuff[i] = skbnew;
						/*
						 * Set length and give buffer
						 * to VMAC
						 */
						arc_write_uncached_32(&ap->rxbd[i].info,
								      (OWN_MASK | (dev->mtu + VMAC_BUFFER_PAD)));
						/*
						 * Give arrived sk buff to
						 * system
						 */
						skb->dev = dev;
						skb_put(skb, len - 4);	/* Make room for data */
						skb->protocol = eth_type_trans(skb, dev);
						/* Raise rx soft irq */
						netif_rx(skb);
					}
				} else
				{
					printk(KERN_INFO "Rx chained, Packet bigger than device MTU\n");
					/* Return buffer to VMAC */
					arc_write_uncached_32(&ap->rxbd[i].info,
							      (OWN_MASK | (dev->mtu + VMAC_BUFFER_PAD)));
					ap->stats.rx_length_errors++;
				}
			}
		}
	}
	if (status & TXINT_MASK)
	{
		/*
		 * Kind of short circuiting code taking advantage of the fact
		 * that the ARC emac will release multiple "sent" packets in
		 * order. So if this interrupt was not for the current
		 * (txbd_dirty) packet then no other "next" packets were
		 * sent. The manual says that TX interrupt can occur even if
		 * no packets were sent and there may not be 1 to 1
		 * correspondence of interrupts and number of packets queued
		 * to send
		 */
		for (i = 0; i < TX_BDT_LEN; i++)
		{
			info = arc_read_uncached_32(&ap->txbd[ap->txbd_dirty].info);

#ifdef EMAC_STATS
			//if (info & RTRY)
				//printk(KERN_CRIT "had to retry\n");

			if (info & DROP)
				emac_drop++;
			if (info & DEFR)
				emac_defr++;
			if (info & LTCL)
				emac_ltcl++;
			if (info & UFLO)
				emac_uflo++;
#endif
			if (info & OWN_MASK)
			{
				dbg_printk("TXINT, OWN_MASK set\n");
				goto err_int;
			}
			if (!(arc_read_uncached_32(&ap->txbd[ap->txbd_dirty].data)))
			{
				dbg_printk("TXINT, no data !!!\n");
				goto err_int;
			}
			if (info & FRST_MASK)
			{
				skb = ap->tx_skbuff[ap->txbd_dirty];
				ap->stats.tx_packets++;
				ap->stats.tx_bytes += skb->len;
				/* return the sk_buff to system */
				dev_kfree_skb_irq(skb);
			}
			arc_write_uncached_32(&ap->txbd[ap->txbd_dirty].data, 0x0);
			arc_write_uncached_32(&ap->txbd[ap->txbd_dirty].info, 0x0);
			ap->txbd_dirty = (ap->txbd_dirty + 1) % TX_BDT_LEN;
		}
	}
	if (status & MDIO_MASK)
	{
		/*
		 * Mark the mdio operation as complete. This is reset by the
		 * MDIO reader
		 */
		ap->mdio_complete = 1;
	}
err_int:
	if (status & ERR_MASK)
	{
//		printk(KERN_DEBUG "ARCTangent Vmac: Error...status = 0x%x\n", status);
		if (status & TXCH_MASK)
		{
			ap->stats.tx_errors++;
			ap->stats.tx_aborted_errors++;
			printk(KERN_ERR "Transmit_chaining error! txbd_dirty = %u\n", ap->txbd_dirty);
		} else if (status & MSER_MASK)
		{
			ap->stats.rx_missed_errors += 255;
			ap->stats.rx_errors += 255;
		} else if (status & RXCR_MASK)
		{
			ap->stats.rx_crc_errors += 255;
			ap->stats.rx_errors += 255;
		} else if (status & RXFR_MASK)
		{
			ap->stats.rx_frame_errors += 255;
			ap->stats.rx_errors += 255;
		} else if (status & RXFL_MASK)
		{
			ap->stats.rx_over_errors += 255;
			ap->stats.rx_errors += 255;
		} else
		{
			printk(KERN_ERR "ARCTangent Vmac: Unkown Error status = 0x%x\n", status);
		}
	}
	return IRQ_HANDLED;
}

#endif // NAPI test.

/* aa3 emac open routine */
int
aa3_emac_open(struct net_device * dev)
{
	struct aa3_emac_priv *ap;
	struct aa3_buffer_desc *bd;
	struct sk_buff *skb;
	volatile unsigned int *reg;
	volatile unsigned int *stat_reg;
	int             i;
	unsigned int    temp, duplex;
	int             noauto;

	ap = netdev_priv(dev);
	if (ap == NULL)
		return -ENODEV;

	/* Register interrupt handler for device */
	request_irq(dev->irq, aa3_emac_intr, 0, dev->name, dev);

	reg = ap->reg_base_addr + ENABLE_OFFSET;
	(*reg) |= MDIO_MASK;	/* MDIO Complete Interrupt Mask */

	reg = ap->reg_base_addr + MDIO_DATA_OFFSET;
	stat_reg = ap->reg_base_addr + STAT_OFFSET;

	/* Reset the PHY */
	__mdio_write(ap, reg, PHY_ID, LXT971A_CTRL_REG, LXT971A_CTRL_RESET);

	/* Wait till the PHY has finished resetting */
	i = 0;
	do
	{
		__mdio_read(ap, reg, PHY_ID, LXT971A_CTRL_REG, temp);
		i++;
	} while (i < AUTO_NEG_TIMEOUT && (temp & LXT971A_CTRL_RESET));

	noauto = (i >= AUTO_NEG_TIMEOUT);	/* A bit hacky this, but we
						 * want to be able to enable
						 * networking without the
						 * cable being plugged in */
	if (noauto)
		__mdio_write(ap, reg, PHY_ID, LXT971A_CTRL_REG, 0);

	/* Advertize capabilities */
	temp = LXT971A_AUTONEG_ADV_10BT | AUTONEG_ADV_IEEE_8023;

	/* If the system clock is greater than 25Mhz then advertize 100 */
	if (clk_speed > 25000000)
		temp |= LXT971A_AUTONEG_ADV_100BTX;

	__mdio_write(ap, reg, PHY_ID, LXT971A_AUTONEG_ADV_REG, temp);

	if (!noauto)
	{
		/* Start Auto-Negotiation */
		__mdio_write(ap, reg, PHY_ID, LXT971A_CTRL_REG,
			(LXT971A_CTRL_AUTONEG | LXT971A_CTRL_RESTART_AUTO));

		/* Wait for Auto Negotiation to complete */
		i = 0;
		do
		{
			__mdio_read(ap, reg, PHY_ID, LXT971A_STATUS2_REG, temp);
			i++;
		} while ((i < AUTO_NEG_TIMEOUT) && !(temp & LXT971A_STATUS2_COMPLETE));
		if (i < AUTO_NEG_TIMEOUT)
		{
			/*
			 * Check if full duplex is supported and set the vmac
			 * accordingly
			 */
			if (temp & LXT971A_STATUS2_FULL)
				duplex = ENFL_MASK;
			else
				duplex = 0;
		} else
		{
			noauto = 1;
		}
	}
	if (noauto)
	{
		/*
		 * Auto-negotiation timed out - this happens if the network
		 * cable is unplugged Force 10mbps, half-duplex.
		 */
		printk("Emac - forcing manual PHY config.\n");
		__mdio_write(ap, reg, PHY_ID, LXT971A_CTRL_REG, 0);
		duplex = 0;
	}
	__mdio_read(ap, reg, PHY_ID, LXT971A_STATUS2_REG, temp);
	dump_phy_status(temp);

	/*
	 * Allocate the rx BD's. kmalloc guarantees only 4 byte alignment. We
	 * need a 8 byte aligned memory so we allocate 1 extra (8 byte) BD
	 * and then adjust it to the next 8 byte boundary
	 */
	/*
	 * ap->rxbd = kmalloc (sizeof(struct aa3_buffer_desc)*(RX_BDT_LEN +
	 * 1), GFP_KERNEL);
	 *
	 * if (ap->rxbd == NULL) return -ENOMEM; ap->rxbd = (struct
	 * aa3_buffer_desc *) ((unsigned int)(ap->rxbd + 1) & 0xfffffff8);
	 *
	 */
	/* Allocate and set buffers for rx BD's */
	bd = ap->rxbd;
	for (i = 0; i < RX_BDT_LEN; i++)
	{
		skb = dev_alloc_skb(dev->mtu + VMAC_BUFFER_PAD);
		/* IP header Alignment (14 byte Ethernet header) */
		skb_reserve(skb, 2);
		ap->rx_skbuff[i] = skb;
		arc_write_uncached_32(&bd->data, skb->data);
		/* VMAC owns rx descriptors */
		arc_write_uncached_32(&bd->info, OWN_MASK | (dev->mtu + VMAC_BUFFER_PAD));
		bd++;
	}
	/* Allocate tx BD's similar to rx BD's */
	/*
	ap->txbd = kmalloc (sizeof(struct aa3_buffer_desc)*(TX_BDT_LEN + 1),
								 GFP_KERNEL);
	if (ap->txbd == NULL)
		return -ENOMEM;
	ap->txbd = (struct aa3_buffer_desc *)
			((unsigned int)(ap->txbd + 1) & 0xfffffff8);
	*/
	/* All TX BD's owned by CPU */
	//memset(ap->txbd, 0, sizeof(struct aa3_buffer_desc) * TX_BDT_LEN);
	bd = ap->txbd;
	for (i = 0; i < TX_BDT_LEN; i++)
	{
		arc_write_uncached_32(&bd->data, 0);
		arc_write_uncached_32(&bd->info, 0);
		bd++;
	}
	/* Initialize logical address filter */
	reg = ap->reg_base_addr + LAFL_OFFSET;
	(*reg) = 0x0;
	reg = ap->reg_base_addr + LAFH_OFFSET;
	(*reg) = 0x0;

	/* Set rx BD ring pointer */
	reg = ap->reg_base_addr + RXRINGPTR_OFFSET;
	(*reg) = (unsigned int) ap->rxbd;

	/* Set tx BD ring pointer */
	reg = ap->reg_base_addr + TXRINGPTR_OFFSET;
	(*reg) = (unsigned int) ap->txbd;

	/* Set Poll rate so that it polls every 1 ms */
	reg = ap->reg_base_addr + POLLRATE_OFFSET;
	(*reg) = (clk_speed / 1000000);

	/*
	 * Enable interrupts. Note: interrupts wont actually come till we set
	 * CONTROL below.
	 */
	reg = ap->reg_base_addr + ENABLE_OFFSET;
	/* FIXME :: enable all intrs later */
	(*reg) = TXINT_MASK |	/* Transmit interrupt */
		RXINT_MASK |	/* Recieve interrupt */
		ERR_MASK |	/* Error interrupt */
		TXCH_MASK |	/* Transmit chaining error interrupt */
        MSER_MASK |
		MDIO_MASK;	/* MDIO Complete Intereupt Mask */

	/* Set CONTROL */
	reg = ap->reg_base_addr + CONTROL_OFFSET;
	(*reg) = (RX_BDT_LEN << 24) |	/* RX buffer desc table len */
		(TX_BDT_LEN << 16) |	/* TX buffer des tabel len */
		TXRN_MASK |	/* TX enable */
		RXRN_MASK |	/* RX enable */
		duplex;		/* Full Duplex enable */

	(*reg) |= EN_MASK;	/* VMAC enable */
#ifdef CONFIG_EMAC_NAPI
    netif_wake_queue(dev);
    napi_enable(&ap->napi);
#else
	netif_start_queue(dev);
#endif

	printk(KERN_INFO "%s up\n", dev->name);
//	kernel_thread(arc_thread, NULL, CLONE_KERNEL);
    kthread_run(arc_thread, 0, "EMAC helper");
#if 0
        printk(KERN_CRIT "Starting spinlock_test thread\n");
        kernel_thread(spinlock_test, NULL, CLONE_KERNEL);
#endif
	//ARC Emac helper thread.
		myproc = create_proc_entry("emac", 0644, NULL);
	if (myproc)
	{
		myproc->read_proc = read_proc;
	}
	return 0;
}

/* aa3 close routine */
int
aa3_emac_stop(struct net_device * dev)
{
	volatile unsigned int *reg;
	struct aa3_emac_priv *ap = netdev_priv(dev);

	dbg_printk("stop called\n");
#ifdef CONFIG_EMAC_NAPI
    napi_disable(&ap->napi);
#endif

	/* Disable VMAC */
	reg = ap->reg_base_addr + CONTROL_OFFSET;
	(*reg) &= (~EN_MASK);

	netif_stop_queue(dev);	/* stop the queue for this device */

	free_irq(dev->irq, dev);

	/* close code here */
	printk(KERN_INFO "%s down\n", dev->name);

	return 0;
}

/* aa3 emac ioctl commands */
int
aa3_emac_ioctl(struct net_device * dev, struct ifreq * rq, int cmd)
{
	dbg_printk("ioctl called\n");
	/* FIXME :: not ioctls yet :( */
	return (-EOPNOTSUPP);
}

/* aa3 emac get statistics */
struct net_device_stats *
aa3_emac_stats(struct net_device * dev)
{
	int             flags;
	struct aa3_emac_priv *ap = netdev_priv(dev);

	dbg_printk("get stats called\n");
	spin_lock_irqsave(&ap->lock, flags);
	aa3_emac_update_stats(ap);
	spin_unlock_irqrestore(&ap->lock, flags);

	return (&ap->stats);
}

void
aa3_emac_update_stats(struct aa3_emac_priv * ap)
{
	unsigned long   miss, rxerr, rxfram, rxcrc, rxoflow;
	volatile unsigned int *reg;

	reg = ap->reg_base_addr + RXERR_OFFSET;
	rxerr = *reg;
	reg = ap->reg_base_addr + MISS_OFFSET;
	miss = *reg;

	rxcrc = (rxerr & 0xff);
	rxfram = (rxerr >> 8 & 0xff);
	rxoflow = (rxerr >> 16 & 0xff);

	ap->stats.rx_errors += miss;
	ap->stats.rx_errors += rxcrc + rxfram + rxoflow;

	ap->stats.rx_over_errors += rxoflow;
	ap->stats.rx_frame_errors += rxfram;
	ap->stats.rx_crc_errors += rxcrc;
	ap->stats.rx_missed_errors += miss;

	/* update the stats from the VMAC */
	return;
}

/* aa3 emac transmit routine */
int
aa3_emac_tx(struct sk_buff * skb, struct net_device * dev)
{
	int             len, data_len, bitmask;
	unsigned int    info;
	char           *data;
	volatile unsigned int *reg;
	volatile unsigned int *status;

	struct aa3_emac_priv *ap = netdev_priv(dev);

	int             inpacket;

	len = skb->len < ETH_ZLEN ? ETH_ZLEN : skb->len;
	data = skb->data;
	dev->trans_start = jiffies;

	//dbg_printk("transmit called - skb=0x%lx, len=%lu\n", skb, len);
	//dbg_printk("tx: flushing and invalidating cache for data: 0x%lx-0x%lx\n", data, data + len);
	flush_dcache_range(data, data + len);
	//was in.
	//	flush_dcache_all();

	/* transmission code */
	reg = ap->reg_base_addr + CONTROL_OFFSET;
	status = ap->reg_base_addr + STAT_OFFSET;

	/* Set First bit */
	bitmask = FRST_MASK;
	inpacket = 1;

	while (len)
	{
		data_len = (len > MAX_TX_BUFFER_LEN) ? MAX_TX_BUFFER_LEN : len;
		info = arc_read_uncached_32(&ap->txbd[ap->txbd_curr].info);
		if ((info & OWN_MASK) == 0)
		{
			arc_write_uncached_32(&ap->txbd[ap->txbd_curr].data, data);
			/* Set Last bit */
			len -= data_len;
			if (len <= 0)
			{
				bitmask |= LAST_MASK;
				inpacket = 0;
			}
			/* Set tx_skbuff here */
			ap->tx_skbuff[ap->txbd_curr] = skb;

			/*
			 * Set data length, bit mask and give ownership to
			 * VMAC This should be the last thing we do. Vmac
			 * might immediately start sending
			 */
			arc_write_uncached_32(&ap->txbd[ap->txbd_curr].info,
					      OWN_MASK | bitmask | data_len);


			//while ((arc_read_uncached_32(&ap->txbd[ap->txbd_curr].info)) & (OWN_MASK));
			ap->txbd_curr = (ap->txbd_curr + 1) % TX_BDT_LEN;

			data += data_len;
			bitmask = 0;

		} else
		{
           if (!netif_queue_stopped(dev))
            {
			    printk(KERN_INFO "Out of TX buffers\n");

            }
            return NETDEV_TX_BUSY;
		}
     }
	if (inpacket)
		printk("Incomplete packet\n");

	/* Set TXPOLL bit to force a poll */
	reg = ap->reg_base_addr + STAT_OFFSET;
	(*reg) |= TXPL_MASK;

	/* Success */
	return 0;
}

/* the transmission timeout function */
void
aa3_emac_tx_timeout(struct net_device * dev)
{
	dbg_printk("transmission timed out\n");
	printk(KERN_CRIT "transmission timeout\n");
	return;
}

/* the set multicast list method */
void
aa3_emac_set_multicast_list(struct net_device * dev)
{
	dbg_printk("set multicast list called\n");
	return;
}

/*
 * Set MAC address of the interface. Called from te core after a SIOCSIFADDR
 * ioctl and from open above
 */
int
aa3_emac_set_address(struct net_device * dev, void *p)
{
	int             i;
	struct sockaddr *addr = p;
	volatile unsigned int *reg;
	struct aa3_emac_priv *ap = netdev_priv(dev);

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	reg = ap->reg_base_addr + ADDRL_OFFSET;
	(*reg) = *(unsigned int *) dev->dev_addr;
	reg = ap->reg_base_addr + ADDRH_OFFSET;
	(*reg) = (*(unsigned int *) &dev->dev_addr[4]) & 0x0000ffff;

	printk(KERN_INFO "MAC address set to ");
	for (i = 0; i < 5; i++)
		printk("%02x:", dev->dev_addr[i]);
	printk("%02x\n", dev->dev_addr[i]);

	return 0;
}


static int
__init aa3_emac_probe(int num)
{
	volatile unsigned int *reg_base_addr;
	unsigned int   *id_reg;

	if (num == 8)
		return -ENODEV;	/* Only upto 8 instances of the VMAC are
				 * possible */

	/* Calculate the register base address of this instance (num) */
	reg_base_addr = (unsigned int *) (VMAC_REG_BASEADDR | (num << 8));

	/* The address of the ID register */
	id_reg = (unsigned int *) reg_base_addr + ID_OFFSET;

	/* Check for VMAC revision 5 or 7, magic number */
	if (*id_reg == 0x0005fd02 || *id_reg == 0x0007fd02)
	{
		return 0;
	}
	return -ENODEV;
}

		// dev->hard_start_xmit = aa3_emac_tx;

static const struct net_device_ops aa3_emac_netdev_ops = {
	.ndo_open 			= aa3_emac_open,
	.ndo_stop 			= aa3_emac_stop,
	.ndo_start_xmit 	= aa3_emac_tx,
	.ndo_set_multicast_list = aa3_emac_set_multicast_list,
	.ndo_tx_timeout 	= aa3_emac_tx_timeout,
	.ndo_set_mac_address= aa3_emac_set_address,
    .ndo_get_stats      = aa3_emac_stats,
    // .ndo_do_ioctl    = aa3_emac_ioctl;
	// .ndo_change_mtu 	= aa3_emac_change_mtu, FIXME:  not allowed
};

int __init
aa3_module_init(void)
{
	int             vmacs = 0;	/* Number of vmac's detected */
	struct net_device *dev;
	struct aa3_emac_priv *priv;
	int             err = -ENODEV;
#ifdef ARCTANGENT_EMAC_SETUP
	struct ifreq    ifr;
	struct sockaddr_in *addr;
#endif
	/* Probe for all the vmac's */
	while (aa3_emac_probe(vmacs) != -ENODEV)
	{
		vmacs++;

		/*
		 * Allocate a net device structure and the priv structure and
		 * intitialize to generic ethernet values
		 */
		/* dev = init_etherdev(NULL, sizeof(struct aa3_emac_priv)); */

		dev = alloc_etherdev(sizeof(struct aa3_emac_priv));

		if (dev == NULL)
			return -ENOMEM;
		priv = netdev_priv(dev);
		priv->reg_base_addr = (unsigned int *) (VMAC_REG_BASEADDR | ((vmacs - 1) << 8));
		/* link this device into the list of all detected devices */
		priv->prev_dev = aa3_root_dev;
		aa3_root_dev = dev;

		/* populate our net_device structure */
        dev->netdev_ops = &aa3_emac_netdev_ops;

		dev->watchdog_timeo = TX_TIMEOUT;
		/* FIXME :: no multicast support yet */
		dev->flags &= ~IFF_MULTICAST;
		dev->irq = VMAC_IRQ + (vmacs - 1);	/* set irq number here */
#ifdef CONFIG_NET_POLL_CONTROLLER
        dev->poll_controller = aa3_netpoll;
#endif


		/* Set EMAC hardware address */
		aa3_emac_set_address(dev, &mac_addr);

		spin_lock_init(&((struct aa3_emac_priv *)netdev_priv(dev))->lock);
		//Amit 's hack
			break;
	}

	printk_init("ARCTangent emac: Probed and found %u vmac's\n", vmacs);
	if (vmacs == 0 )  {
		printk_init("***ARC EMAC [NOT] detected, skipping EMAC init\n");
		return -ENODEV;
	}

#ifdef ARCTANGENT_EMAC_SETUP
	strcpy(ifr.ifr_name, "eth0");
	addr = (struct sockaddr_in *) & ifr.ifr_addr;
	addr->sin_family = AF_INET;
	addr->sin_port = 0;
	addr->sin_addr.s_addr = in_aton("192.168.100.222");
	devinet_ioctl(SIOCSIFADDR, &ifr);
	addr = (struct sockaddr_in *) & ifr.ifr_broadaddr;
	addr->sin_family = AF_INET;
	addr->sin_port = 0;
	addr->sin_addr.s_addr = in_aton("192.168.100.255");
	devinet_ioctl(SIOCSIFBRDADDR, &ifr);
	addr = (struct sockaddr_in *) & ifr.ifr_netmask;
	addr->sin_family = AF_INET;
	addr->sin_port = 0;
	addr->sin_addr.s_addr = in_aton("255.255.255.0");
	devinet_ioctl(SIOCSIFNETMASK, &ifr);
	ifr.ifr_flags = IFF_UP;
	devinet_ioctl(SIOCSIFFLAGS, &ifr);
	dbg_printk("ARCTangent emac: Interface is up\n");
#endif				/* ARCTANGENT_EMAC_SETUP */
	/* return (vmacs ? 0 : -ENODEV); */
#ifdef CONFIG_EMAC_NAPI
    netif_napi_add(dev,&priv->napi,aa3_poll, NAPI_WEIGHT);
//    SET_NETDEV_DEV(dev,&priv->napi);
#endif

	err = register_netdev(dev);

	if (err)
		goto out;
	return err;
out:
	free_netdev(dev);
	return ERR_PTR(err);

}

void __exit
aa3_module_cleanup(void)
{
	struct net_device *dev, *prev_dev;
	dev = aa3_root_dev;
	while (dev)
	{
		/* unregister the network device */
		unregister_netdev(dev);
		/* free up memory for private data */
		prev_dev = ((struct aa3_emac_priv *)netdev_priv(dev))->prev_dev;
		kfree(netdev_priv(dev));
		dev = prev_dev;
	}

	return;

}


void
arc_thread(void) //helps with interrupt mitigation.
{
	unsigned int    flags;
	unsigned int    i;
	while (1)
	{
		msleep(100);

		if (!skb_count)
		{

			for (i = 1; i != SKB_PREALLOC; i++)
			{
				skb_prealloc[i] = dev_alloc_skb(1518);
				//MTU
					if (!skb_prealloc[i])
					printk(KERN_CRIT "Failed to get an SKB\n");

			}
			skb_count = SKB_PREALLOC - 1;
		}
	}
}


int
read_proc(char *sysbuf, char **p_mybuf, off_t offset, int l_sysbuf, int zero)
{
	int             len;

	if (offset > 0)
	{
		return 0;
	}

	len = sprintf(sysbuf, "\nARC EMAC STATISTICS\n");
	len += sprintf(sysbuf + len, "-------------------\n");
	len += sprintf(sysbuf + len, "SKB Pre-allocated available buffers : %u\n", skb_count);
	len += sprintf(sysbuf + len, "SKB Pre-allocated maximum           : %u\n", SKB_PREALLOC);
	len += sprintf(sysbuf + len, "Number of pre-allocated SKB's used  : %u\n", skb_preallocated_used);
	len += sprintf(sysbuf + len, "Number of intr allocated SKB's used : %u\n", skb_not_preallocated);
	len += sprintf(sysbuf + len, "EMAC DEFR count : %u\n", emac_defr);
	len += sprintf(sysbuf + len, "EMAC DROP count : %u\n", emac_drop);
	len += sprintf(sysbuf + len, "EMAC LTCL count : %u\n", emac_ltcl);
	len += sprintf(sysbuf + len, "EMAC UFLO count : %u\n", emac_uflo);
	return (len);

}

module_init(aa3_module_init);
module_exit(aa3_module_cleanup);
