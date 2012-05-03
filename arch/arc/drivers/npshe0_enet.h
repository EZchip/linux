
#ifndef _NPSHE0_ENET
#define _NPSHE0_ENET


/* driver status definitions  */
#define EZ_NET_DEV_OFF	0
#define EZ_NET_DEV_ON	1
#define	EZ_NET_BRODCAST_MODE	0
#define	EZ_NET_PROMISC_MODE	1


/* driver global definitions*/
#define EZ_NET_ETH_PKT_SZ	(ETH_FRAME_LEN+20)
#define EZ_NET_LAN_BUFFER_SIZE 0x600 /* 1.5K */
#define EZ_NET_REG_SIZE	4

/* IRQ  definitions */
#define IRQ_LAN_RX_LINE	7
#define IRQ_LAN_TX_LINE	8
#define IRQ_LAN_RX_MASK	0x00000080
#define IRQ_LAN_TX_MASK	0x00000100


/* GEMAC config register definitions */
#define GEMAC_CFG_INIT_VALUE	0x70C8501F
#define GEMAC_CFG_REG_BASE		0xC0003000


/* TX CTL register definitions */
#define TX_CTL_BUSY     	0x00008000
#define TX_CTL_ERROR    	0x00004000
#define TX_CTL_RESET    	0x00000800
#define TX_CTL_LENGTH_MASK	0x000007FF


/* RX CTL register definitions */
#define RX_CTL_BUSY     	0x00008000
#define RX_CTL_ERROR    	0x00004000
#define RX_CTL_CRC      	0x00002000
#define RX_CTL_RESET   	 	0x00000800
#define RX_CTL_LENGTH_MASK	0x000007FF


/* LAN register definitions  */
#define REG_GEMAC_TX_CTL     (* ( (volatile int*)(GEMAC_CFG_REG_BASE+0x00000000 ) ) )
#define REG_GEMAC_TXBUF_STS  (* ( (volatile int*)(GEMAC_CFG_REG_BASE+0x00000004 ) ) )
#define REG_GEMAC_TXBUF_DATA (* ( (volatile int*)(GEMAC_CFG_REG_BASE+0x00000008 ) ) )
#define REG_GEMAC_RX_CTL     (* ( (volatile int*)(GEMAC_CFG_REG_BASE+0x00000010 ) ) )
#define REG_GEMAC_RXBUF_STS  (* ( (volatile int*)(GEMAC_CFG_REG_BASE+0x00000014 ) ) )
#define REG_GEMAC_RXBUF_DATA (* ( (volatile int*)(GEMAC_CFG_REG_BASE+0x00000018 ) ) )
#define REG_GEMAC_MAC_CFG    (* ( (volatile int*)(GEMAC_CFG_REG_BASE+0x00000040 ) ) )


/* driver private data structure */
typedef struct  {
   int				  msg_enable;
   int                status;       /* EZ_NET_DEV_OFF or EZ_NET_DEV_ON */
   unsigned char      address[ETH_ALEN]; /* mac address */
   int                down_event;
   int				  packet_len;
   int 				  rx_mode;

}EZT_NET_PRIV;

#endif /* _NPSHE0_ENET */
