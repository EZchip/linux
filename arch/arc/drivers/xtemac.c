
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
#include <linux/platform_device.h>
#include "xtemac.h"

#define DRIVER_NAME         "xilinx_temac"
#define TX_RING_LEN         2
#define RX_RING_LEN         32


static int xtemac_poll (struct napi_struct *napi, int budget);
static int xtemac_clean(struct net_device *netdev, unsigned int *work_done, unsigned int work_to_do);
int xtemac_open(struct net_device *);
int xtemac_tx(struct sk_buff *, struct net_device *);
void xtemac_tx_timeout(struct net_device *);
void xtemac_set_multicast_list (struct net_device *);
void xtemac_tx_timeout(struct net_device *);
struct net_device_stats * xtemac_stats(struct net_device *);
int xtemac_stop(struct net_device *);
int xtemac_set_address(struct net_device *, void *);
void print_phy_status2(unsigned int );
void print_packet(unsigned int, unsigned char *);

static int xtemac_tx_clean(struct net_device *netdev);
static irqreturn_t xtemac_intr (int irq, void *dev_instance);

static const struct net_device_ops xtemac_netdev_ops = {
	.ndo_open 			= xtemac_open,
	.ndo_stop 			= xtemac_stop,
	.ndo_start_xmit 	= xtemac_tx,
	.ndo_set_multicast_list = xtemac_set_multicast_list,
	.ndo_tx_timeout 	= xtemac_tx_timeout,
	.ndo_set_mac_address= xtemac_set_address,
    .ndo_get_stats      = xtemac_stats,
    // .ndo_do_ioctl    = aa3_emac_ioctl;
	// .ndo_change_mtu 	= aa3_emac_change_mtu, FIXME:  not allowed
};


struct xtemac_stats
{
    unsigned int rx_packets;
    unsigned int rx_bytes;
};



struct xtemac_ptr
{
        struct xtemac_bridge bridge;
        struct xtemac_configuration config;
        struct xtemac_address_filter filter;
        struct xtemac_bridge_extn bridgeExtn;

};

volatile struct xtemac_ptr *xtemac = XTEMAC_BASE;

struct xtemac_priv
	{
		struct net_device_stats stats;
        spinlock_t      lock;
        unsigned char   address[6];
        unsigned int    RxEntries;
        unsigned int    RxCurrent;
        struct sk_buff  *tx_skb;
        struct net_device      *ndev;   // This device.
        void *          tx_buff;        // Area packets TX from
        void *          rx_buff;        // Area packets DMA into
        unsigned int    rx_len;
        struct  xtemac_stats phy_stats;     // PHY stats.

	};


void            xtemac_update_stats(struct xtemac_priv * ap);
unsigned int XEMAC_mdio_read ( volatile struct xtemac_ptr * ,
    unsigned int,
    unsigned int,
    volatile unsigned int *);



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



/* XTEMAC interrupt handler */
static irqreturn_t
xtemac_emac_intr(int irq, void *dev_instance)
{

    unsigned int    status;
    unsigned int    dmaStatus;
//    printk("TEMAC interrupt\n");
    struct xtemac_priv  *priv = netdev_priv(dev_instance);
    unsigned int len;
    struct sk_buff  *skb;
    unsigned int n;

    status = *&xtemac->bridge.intrStsReg;
    if(status & INTR_STATUS_RX_ERROR_BIT_POS)
    {
        printk("INTR_STATUS_RX_ERROR\n");
        xtemac->bridge.intrClrReg = INTR_STATUS_RX_ERROR_BIT_POS;
        xtemac->bridge.dmaRxClrReg = 1;         // Attempt to clear the error
        xtemac->bridge.txFifoRxFifoRstReg = 1;         // Reset receiver.
        __asm__ __volatile__ ("nop");
        __asm__ __volatile__ ("nop");
        __asm__ __volatile__ ("nop");
        xtemac->bridge.txFifoRxFifoRstReg = 0;         // Fifo out of reset.
    }

    if(status & INTR_STATUS_TX_ERROR_BIT_POS)
    {
        printk("INTR_STATUS_TX_ERROR\n");
        xtemac->bridge.intrClrReg = INTR_STATUS_RX_ERROR_BIT_POS;
    }
    if(status & INTR_STATUS_DMA_TX_ERROR_BIT_POS)
    {
        printk("INTR_STATUS_DMA_TX_ERROR\n");
    }
    if(status & INTR_STATUS_DMA_RX_ERROR_BIT_POS)
    {
        printk("INTR_STATUS_DMA_TX_ERROR\n");
    }
    if(status & INTR_STATUS_DMA_RX_COMPLETE_BIT_POS)
    {
//        printk("INTR_DMA_RX_COMPLETE\n");
        xtemac->bridge.dmaRxClrReg = 1;
        priv->stats.rx_packets++;
        inv_dcache_range(priv->rx_buff, priv->rx_buff +priv->rx_len);
        flush_dcache_all();
        skb=alloc_skb(priv->rx_len + 32, GFP_ATOMIC);
        skb_reserve(skb,NET_IP_ALIGN);      // Align header
        memcpy(skb->data, priv->rx_buff, priv->rx_len);
        skb_put(skb,priv->rx_len);
        skb->dev = dev_instance;
        skb->protocol = eth_type_trans(skb,dev_instance);
        skb->ip_summed = CHECKSUM_NONE;
        netif_rx(skb);

    xtemac->bridgeExtn.intrEnableReg = ENABLE_ALL_INTR ;

    }
    if(status & INTR_STATUS_DMA_TX_COMPLETE_BIT_POS)
    {
//        printk("INTR_DMA_TX_COMPLETE\n");
//        dev_kfree_skb(priv->tx_skb);
        priv->tx_skb = 0;
        xtemac->bridge.dmaTxClrReg = 1;
    }


// Receive a packet.
// Start to DMA into a physically contiguous buffer.
// Switch off RX interrupts until DMA complete.

    if(status & INTR_STATUS_RCV_INTR_BIT_POS )
    {

//        printk("INTR_STATUS_RCV_INTR\n");

        xtemac->bridge.intrClrReg = INTR_STATUS_RCV_INTR_BIT_POS;
        priv->rx_len = xtemac->bridgeExtn.sizeReg;
        xtemac->bridge.dmaRxAddrReg = virt_to_phys(priv->rx_buff);
        xtemac->bridge.dmaRxCmdReg = (priv->rx_len << 8 | DMA_READ_COMMAND);
        xtemac->bridgeExtn.intrEnableReg = ENABLE_ALL_INTR & (~ENABLE_PKT_RECV_INTR);

    }


    if(status & INTR_STATUS_RX_OVRN_INTR_BIT_POS)
    {
        xtemac->bridge.intrClrReg = INTR_STATUS_RX_OVRN_INTR_BIT_POS;
        printk("INTR_STATUS_RX_OVRN_INTR\n");
        xtemac->bridge.dmaRxClrReg = 1;         // Attempt to clear the error
        xtemac->bridge.txFifoRxFifoRstReg = 1;         // Reset receiver.
        __asm__ __volatile__ ("nop");
        __asm__ __volatile__ ("nop");
        __asm__ __volatile__ ("nop");
        xtemac->bridge.txFifoRxFifoRstReg = 0;         // Fifo out of reset.

    }
out:
//printk("Interrupt done\n");
	return IRQ_HANDLED;
}


/* XTEAMC open */
int
xtemac_open(struct net_device * dev)
{
return 0;
}

/* XTEMAC close routine */
int
xtemac_stop(struct net_device * dev)
{
	return 0;
}

/* XTEMAC ioctl commands */
int
xtemac_ioctl(struct net_device * dev, struct ifreq * rq, int cmd)
{
	printk("ioctl called\n");
	/* FIXME :: not ioctls yet :( */
	return (-EOPNOTSUPP);
}

/* XTEMAC transmit routine */
int
xtemac_tx(struct sk_buff * skb, struct net_device * dev)
{
//printk("Transmit...\n");

    unsigned int status;
    struct xtemac_priv *priv = netdev_priv(dev);

    if(priv->tx_skb)
        return 0;

    memcpy(priv->tx_buff, skb->data, skb->len);
    flush_dcache_all();
    //print_packet(skb->len, priv->tx_buff);

    xtemac->bridge.dmaTxAddrReg = virt_to_phys(priv->tx_buff);
    xtemac->bridge.dmaTxCmdReg = (skb->len <<8 | DMA_WRITE_COMMAND | DMA_START_OF_PACKET | DMA_END_OF_PACKET);
    priv->stats.tx_packets++;

    priv->tx_skb = skb;
    dev_kfree_skb(priv->tx_skb);
    return(0);

}



void tx_dma_complete(void)
{

}


/* the transmission timeout function */
void
xtemac_tx_timeout(struct net_device * dev)
{
	printk("transmission timed out\n");
	printk(KERN_CRIT "transmission timeout\n");
	return;
}

/* the set multicast list method */
void
xtemac_set_multicast_list(struct net_device * dev)
{
	printk("set multicast list called\n");
	return;
}

// XTEMAC MAC address set.

int
xtemac_set_address(struct net_device * dev, void *p)
{

    int             i;
    struct sockaddr *addr = p;
    struct xtemac_priv *ap = netdev_priv(dev);

    memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

    printk(KERN_INFO "MAC address set to ");
    for (i = 0; i < 5; i++)
        printk("%02x:", dev->dev_addr[i]);
    printk("%02x\n", dev->dev_addr[i]);
// Program filter.

//    xtemac->addrFilter.addrFilterModeReg = ENABLE_ADDR_FILTER;

	return 0;
}

void xtemac_update_stats( struct xtemac_priv *ap)
{
//
// Code to read the current stats from the XTEMAC and add to ap->stats
//


}

struct net_device_stats *
xtemac_stats(struct net_device * dev)
{

    int             flags;
    struct xtemac_priv *ap = netdev_priv(dev);

    printk("get stats called\n");
    spin_lock_irqsave(&ap->lock, flags);
    xtemac_update_stats(ap);
    spin_unlock_irqrestore(&ap->lock, flags);

    return (&ap->stats);



}


static int __devinit xtemac_probe(struct platform_device *dev)
{

    struct net_device *ndev;
    struct xtemac_priv *priv;
    unsigned int n;
    unsigned int rc;
    unsigned int speed;

printk("Probing...\n");

// Setup a new netdevice

    ndev = alloc_etherdev(sizeof(struct xtemac_priv));
    if (!ndev)
    {
        printk("Failed to allocated net device\n");
        return -ENOMEM;
    }

    dev_set_drvdata(dev,ndev);

// Fixme - use real mac address;

    ndev->dev_addr[0] = 0x0;
    ndev->dev_addr[1] = 0x1;
    ndev->dev_addr[2] = 0x2;
    ndev->dev_addr[3] = 0x3;
    ndev->dev_addr[4] = 0x4;
    ndev->dev_addr[5] = 0x80;
//    ndev->features |= NETIF_F_IP_CSUM;
    ndev->irq = 6;

    priv = netdev_priv(ndev);
    priv->ndev = ndev;
    priv->tx_skb = 0;

    for(n=0;n<=5;n++)
    {
        printk("setup MAC address %x\n",n);
        priv->address[n] = ndev->dev_addr[n];
    }

    ndev->netdev_ops = &xtemac_netdev_ops;
    ndev->watchdog_timeo = (400*HZ/1000);
    ndev->flags &= ~IFF_MULTICAST;

// Setup RX buffer

    priv->rx_buff = kmalloc(RX_RING_LEN*1500,GFP_KERNEL);
    priv->tx_buff = kmalloc(TX_RING_LEN*1500,GFP_KERNEL);

    if(!priv->rx_buff || !priv->tx_buff)
    {
        printk("Bummer, failed to alloc ring buffers\n");
        return -ENOMEM;
    }

// Install interrupt handler

    spin_lock_init(&((struct xtemac_priv *) netdev_priv(ndev))->lock);

    request_irq(ndev->irq, xtemac_emac_intr, 0, ndev->name, ndev);

// Register net device with kernel, now ifconfig should see the device

    rc = register_netdev(ndev);
    if (rc)
    {
        printk("Didn't register the netdev, bummer.\n");
        return(rc);
    }

// OK, now switch on the hardware.

    xtemac->bridge.intrClrReg = CLEAR_INTR_REG;
    xtemac->bridge.txFifoRxFifoRstReg= RESET_TX_RX_FIFO;
    xtemac->bridge.txFifoRxFifoRstReg=0;
    xtemac->bridgeExtn.intrEnableReg = (ENABLE_ALL_INTR & (~ENABLE_RX_OVERRUN_INTR) & (~ENABLE_MDIO_INTR));
    xtemac->config.rxCtrl1Reg, ENABLE_RX;
    xtemac->config.txCtrlReg, ENABLE_TX;

// Probe the Phy to see what speed is negotiated.

     XEMAC_mdio_read(xtemac, BSP_XEMAC1_PHY_ID,MV_88E1111_STATUS2_REG, &speed);

    print_phy_status2(speed);

    return(0);

}

static void xtemac_remove(struct net_device *dev)
{
    struct net_device *ndev = dev_get_drvdata(dev);
    unregister_netdev(ndev);
    return(0);

}



void print_packet(unsigned int len, unsigned char * packet)
{
    unsigned int    n;
    printk("Printing packet\nLen = %u\n", len);

    for(n=0;n<len;n++)
    {
        if(! (n %20))
            printk("\n");
        printk("%02x-",*packet++);
    }
    printk("\n");
}



void print_phy_status2(unsigned int status)
{

    printk("Status is %x\n", status);

    if(status & MV_88E1111_STATUS2_FULL)
        printk("Full duplex\n");
    if(status & MV_88E1111_STATUS2_LINK_UP)
        printk("Link up\n");
    if(status & MV_88E1111_STATUS2_100)
        printk("100Mbps\n");
    if(status & MV_88E1111_STATUS2_1000)
        printk("1000Mbps\n");
}



/* [IN] The device physical address */
/* [IN] the register index (0-31)*/
/* [IN]/[OUT] The data pointer */
unsigned int XEMAC_mdio_read ( volatile struct xtemac_ptr * dev_ptr,
     unsigned int                phy_addr,
     unsigned int                reg_index,
     volatile unsigned int                * data_ptr
  )
{
    unsigned int value = 0;
    unsigned int count=0;

    dev_ptr->config.mgmtConfigReg = MDIO_ENABLE;
    dev_ptr->bridgeExtn.phyAddrReg = phy_addr;
   *data_ptr = *(&dev_ptr->bridgeExtn.macMDIOReg + reg_index);

   value = dev_ptr->bridgeExtn.intrRawStsReg;

   while (!(value & INTR_STATUS_MDIO_BIT_POS))
   {
       count++;
       value = dev_ptr->bridgeExtn.intrRawStsReg;
   }

   dev_ptr->bridge.intrClrReg =  INTR_STATUS_MDIO_BIT_POS;
    printk("read MDIO status in %x counts\n", count);
}



static struct platform_driver xtemac_driver = {
     .driver = {
         .name = DRIVER_NAME,
     },
     .probe = xtemac_probe,
     .remove = xtemac_remove

 };



int __init
xtemac_module_init(void)
{

printk("*** Init XTEMAC ***\n");
    return platform_driver_register(&xtemac_driver);


}

void __exit
xtemac_module_cleanup(void)
{
	return;

}


module_init(xtemac_module_init);
module_exit(xtemac_module_cleanup);
