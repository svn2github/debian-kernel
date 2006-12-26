/*
  This driver supports the Unicorn ADSL chipset from STMicroelectronics.
  The chipset consists of the ADSL DMT transceiver ST70137 and either the
  ST70134A or ST70136 Analog Front End (AFE).
  This file contains the ethernet interface and SAR routines.
*/
#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,18)
#	include <linux/config.h>
#endif
#if defined(CONFIG_MODVERSIONS) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
#include <linux/modversions.h>
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/smp_lock.h>
#include <linux/random.h>
#include <linux/skbuff.h>
#include <linux/atm.h>
#include <linux/atmioc.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/if_pppox.h>
#include <linux/ip.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include "types.h"
#include "amu/amas.h"
#include "crc.h"
#include "unicorn.h"
#include "debug.h"

#ifdef CONFIG_ATM
extern struct proc_dir_entry *atm_proc_root;
#else
static struct proc_dir_entry *atm_proc_root=NULL;
#endif

#define DRIVER_NAME "UNICORN-ETH"
#ifdef ETH_DRIVER
MODULE_AUTHOR ("fisaksen@bewan.com");
MODULE_DESCRIPTION ("Ethernet driver for the ST UNICORN ADSL modem.");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif

const unsigned char PAD[2] =
	{0x00,0x00};
const unsigned char BRIDGED_LLC_OUI_PID[10] = 
	{0xAA,0xAA,0x03,0x00,0x80,0xC2,0x00,0x07,0x00,0x00}; // including PAD
const unsigned char ROUTED_LLC_OUI_PID[8] = 
	{0xAA,0xAA,0x03,0x00,0x00,0x00,0x08,0x00};
const unsigned char PPPOA_LLC_ENCAPS[4] = 
	{0xfe,0xfe,0x03,0xcf};


// poll for data every POLL_TIME msec 
// multiple of 10ms in most cases
#define POLL_TIME 10

/*
 *	This structure defines an ethernet arp header.
 */
struct arphdr_eth
{
	unsigned short	ar_hrd;		/* format of hardware address	*/
	unsigned short	ar_pro;		/* format of protocol address	*/
	unsigned char	ar_hln;		/* length of hardware address	*/
	unsigned char	ar_pln;		/* length of protocol address	*/
	unsigned short	ar_op;		/* ARP opcode (command)		*/
	/*
	 *	 Ethernet looks like this : This bit is variable sized however...
	 */
	unsigned char		ar_sha[ETH_ALEN];	/* sender hardware address	*/
	__u32                   ar_sip;		/* sender IP address		*/
	unsigned char		ar_tha[ETH_ALEN];	/* target hardware address	*/
	__u32		        ar_tip;		/* target IP address		*/
} __attribute__((packed));

// Private data
typedef struct unicorn_ethdrv {
	/* eth driver */
	struct net_device *dev;
	struct sk_buff_head tx_q;     // "Normal" transmit queue - UBR
	struct sk_buff *reas;         // Reassembly buffer
	ADSL_STATUS adsl_status;
	unsigned long downstream_rate;
	unsigned long upstream_rate;
	unsigned long curr_us_rate;
	unsigned long us_cell_counter;
	unsigned long curr_ds_rate;
	unsigned long ds_cell_counter;
	enum {
		RFC2364=0,
		RFC2684_BRIDGED,
		RFC2684_ROUTED,
	} protocol;
	enum {
		VCMUX=0,
		LLC
	} encaps;
	/* VCC */
	int vpi;
	int vci;
	/* traffic shaping */
	int link_rate;
	struct timer_list timer;    
	int ms_counter;
	int max_cell_counter;  // counts max number of cells/sec for traffic shaping
	struct proc_dir_entry *proc_dir_entry;
	/* Statistics */
	struct net_device_stats eth_stats;
	T_oam_stats oam_stats;
	/* pci/usb driver entrypoints */
	struct unicorn_entrypoints *ep;
} unicorn_ethdrv_t;

// extended skb_data structure used when encoding cells (max 48 bytes)
struct atm_ext_skb_data {
	unsigned char aal;
	union {
		struct {
			unsigned int pdu_length;
		} aal0;
		struct {
			unsigned int pdu_length;
			unsigned long crc;
		} aal5;
	} tx;
 };

#define ATM_EXT_SKB(skb) (((struct atm_ext_skb_data *) (skb)->cb))


struct unicorn_ethdrv *unicorn_ethdrv = NULL;

// driver parameters
static char if_name[IFNAMSIZ] = { 0x0 };
static char mac_address[ETH_ALEN*2 + 1] = { 0x0 };
static int VPI= ATM_VPI_UNSPEC;
static int VCI= ATM_VCI_UNSPEC;
static char PROTOCOL[8] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
static char ENCAPS[11] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
#if DEBUG
#ifdef ETH_DRIVER
unsigned long DebugLevel=0; // ATM_D,DATA_D
#endif
#endif

const unsigned char PPPOE_SERVER_ADDR[ETH_ALEN] = {0x00,'P','P','P','O','A'};
static unsigned short pppoe_unique_session_id = 0;

static int unicorn_eth_open(struct net_device *eth_dev);
static int unicorn_eth_close(struct net_device *eth_dev);
static int unicorn_eth_send(struct sk_buff *skb, struct net_device *eth_dev);
static struct net_device_stats *unicorn_eth_stats(struct net_device *eth_dev);
static int unicorn_eth_ioctl(struct net_device *eth_dev, struct ifreq *rq, int cmd);
static int unicorn_eth_change_mtu(struct net_device *eth_dev, int new_mtu);
static void unicorn_eth_set_multicast(struct net_device *eth_dev);
static void unicorn_eth_tx_timeout(struct net_device *eth_dev);
static int unicorn_eth_proc_read (char *page, char **start, off_t off, int count, int *eof, void *data);

const char *get_adsl_status_string(ADSL_STATUS status)
{
	switch (status) {
	case ADSL_STATUS_NOHARDWARE: return "no hardware"; 
	case ADSL_STATUS_NOLINK: return "no link"; 
	case ADSL_STATUS_ATMREADY: return "ATM ready"; 
	default: return "unknown";
	}
}
const char *get_modemstate_string(AMSW_ModemState state)
{
	switch(state) {
	case C_AMSW_IDLE: return "IDLE";
	case C_AMSW_L3: return "L3";
	case C_AMSW_LISTENING: return "LISTENING";
	case C_AMSW_ACTIVATING: return "ACTIVATING";
	case C_AMSW_Ghs_HANDSHAKING: return "Ghs HANDSHAKING";
	case C_AMSW_ANSI_HANDSHAKING: return "ANSI HANDSHAKING";
	case C_AMSW_INITIALIZING: return "INITIALIZING";
	case C_AMSW_RESTARTING: return "RESTARTING";
	case C_AMSW_FAST_RETRAIN: return "FAST RETRAIN";
	case C_AMSW_SHOWTIME_L0: return "SHOWTIME L0";
	case C_AMSW_SHOWTIME_LQ: return "SHOWTIME LQ";
	case C_AMSW_SHOWTIME_L1: return "SHOWTIME L1";
	case C_AMSW_EXCHANGE: return "EXCHANGE";
	case C_AMSW_TRUNCATE: return "TRUNCATE";
	case C_AMSW_ESCAPE: return "ESCAPE";
	case C_AMSW_DISORDERLY: return "DISORDERLY";
	case C_AMSW_RETRY: return "RETRY";
	default: return "unknown";
	}
}

inline unsigned char get_hex_digit(const char digit[2])
{
	int i;
	unsigned char h=0;

	for (i = 0; i < 2; i++) {
		char c = digit[i];
		h <<=4;
		if (c >= '0' && c <= '9') {
			h += c - '0';
		} else if (c >= 'A' && c <= 'F') {
			h += c - 'A' + 10;
		} else if (c >= 'a' && c <= 'f') {
			h += c - 'a' + 10;
		}
	}
	return h;
}

static void 
unicorn_set_mac(struct net_device *eth_dev, const char *mac) 
{	
	if ((mac != NULL) && (strlen(mac) >= (ETH_ALEN*2))) {
		int i;
		for (i=0; i < ETH_ALEN; i++) {
			eth_dev->dev_addr[i] = get_hex_digit(&mac[i*2]);
		}
		eth_dev->dev_addr[0] &= ~0x01; // make sure it is not a broadcast address
	} else {
		/* Generate random Ethernet address.  */
		eth_dev->dev_addr[0] = 0x00;
		get_random_bytes(&eth_dev->dev_addr[1],ETH_ALEN-1);
	}
	INFO("MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
	    eth_dev->dev_addr[0],eth_dev->dev_addr[1],eth_dev->dev_addr[2],
	    eth_dev->dev_addr[3],eth_dev->dev_addr[4],eth_dev->dev_addr[5]);
}

static void make_hw_addr(__u32 ip_addr,unsigned char *hw_addr)
{
	hw_addr[0] = 0x00;
	hw_addr[1] = 0xfe;
	*(__u32 *)(&hw_addr[2]) = ip_addr;
}

static AMSW_ModemState get_modemstate(struct unicorn_ethdrv *drv)
{
	T_MswCtrl msw_ctrl;
	AMSW_ModemState modemstate=C_AMSW_IDLE;

	if (drv->ep) {
		msw_ctrl.code = MSW_CTL_GET_STATE;
		msw_ctrl.subcode = 0;
		msw_ctrl.buffer = &modemstate;
		msw_ctrl.length = sizeof(AMSW_ModemState);
		drv->ep->msw_control(drv->ep->dev, &msw_ctrl);
	}
	DBG(ATM_D,"ADSL modem state %s\n",get_modemstate_string(modemstate));
	return modemstate;
}

static int get_link_rate(struct unicorn_ethdrv *drv)
{
	int link_rate;

	// get the ADSL link speed
	if (drv->ep) {
		drv->ep->get_adsl_linkspeed(drv->ep->dev,&drv->upstream_rate,&drv->downstream_rate);
		if ((drv->upstream_rate == 0) || (drv->downstream_rate == 0)) {
			link_rate = 0;
		} else {
			link_rate = (drv->upstream_rate*1000)/(8*ATM_CELL_SIZE);
		}
	} else {
		link_rate = 0;
	}
	DBG(ATM_D,"upstream_rate=%ld Kbits/s,downstream_rate=%ld Kbits/s,link_rate=%d cells/sec\n",
	    drv->upstream_rate,drv->downstream_rate,link_rate);
	return link_rate;
}

static int unicorn_eth_proc_read (char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct unicorn_ethdrv *drv = (struct unicorn_ethdrv *)data;
	struct net_device *eth_dev = drv->dev;
	int size;
	char *p = page;
	AMSW_ModemState modemstate = get_modemstate(drv);
	get_link_rate(drv);

	p += sprintf(p, "ADSL: status %s, modem state %s, US rate %ldKbits/s, DS rate %ldKbits/s\n", 
		     get_adsl_status_string(drv->adsl_status),get_modemstate_string(modemstate),
			       drv->upstream_rate,drv->downstream_rate);
	p += sprintf(p,"Current speed: US %ldKbits/s,DS %ldKbits/s\n",
		     drv->curr_us_rate,drv->curr_ds_rate);
	p += sprintf(p, "Bridged: %02x:%02x:%02x:%02x:%02x:%02x\n",
		     eth_dev->dev_addr[0], eth_dev->dev_addr[1], eth_dev->dev_addr[2], 
		     eth_dev->dev_addr[3], eth_dev->dev_addr[4], eth_dev->dev_addr[5]);

    /* Figure out how much of the page we used */
    size  = p - page;
    size -= off;
    
    if (size < count) {
	    *eof = 1; 
	    if (size <= 0)
		    return 0;
    } else {
	    size = count;
    }
    /* Fool caller into thinking we started where he told us to in the page */
    *start = page + off;
    return size;
}

static int skb_add_header(struct sk_buff *skb,const unsigned char *header,int len)
{
	if (skb_headroom(skb) < len) {
		if (skb_cow(skb,len)) {
			return -ENOMEM;
		}
	}
	memcpy(skb_push(skb,len),header,len);
	return 0;
}

static inline struct sk_buff *peek_tx_skb(struct unicorn_ethdrv *drv)
{
	struct sk_buff *skb;
	
	skb = skb_peek(&drv->tx_q);
	return skb;
}

static inline struct sk_buff *dequeue_tx_skb(struct unicorn_ethdrv *drv)
{
	struct sk_buff *skb;

	skb = skb_dequeue(&drv->tx_q);
	return skb;
}

static void purge_tx_q(struct unicorn_ethdrv *drv)
{
	struct sk_buff *skb;
	
	// free all buffers in transmit queue
	while ((skb = dequeue_tx_skb(drv)) != NULL) {
		dev_kfree_skb_any(skb);
		// statistics
	}
}

static int build_aal0_cell(struct unicorn_ethdrv *drv,unsigned char *cell,struct sk_buff *skb)
{
	DBG(ATM_D,"skb->len=%d\n",skb->len);
	
	if (skb->len == ATM_AAL0_SDU) {
		// from AAL0 socket
		memcpy(cell,skb->data,ATM_AAL0_SDU - ATM_CELL_PAYLOAD);
		skb_pull(skb,ATM_AAL0_SDU - ATM_CELL_PAYLOAD);
		memcpy(cell+5,skb->data,ATM_CELL_PAYLOAD);			
		skb_pull(skb,ATM_CELL_PAYLOAD);
	} else {
		// OAM loopback
		memcpy(cell,skb->data,ATM_CELL_SIZE);			
		skb_pull(skb,ATM_CELL_SIZE);
	}
	DUMP_PACKET(DATA_D,cell,ATM_CELL_SIZE);
	return 1;
}

static int build_aal5_cell(struct unicorn_ethdrv *drv,unsigned char *cell,struct sk_buff *skb)
{
	struct atm_ext_skb_data *skb_data = ATM_EXT_SKB(skb);
	int payload_len;
	int pad_len;
	
	ASSERT(skb_data != NULL);

	// Add 5 byte ATM header
	cell[0] = (drv->vpi >> 4) & 0xf;
	cell[1] = ((drv->vpi & 0xf) << 4) | ((drv->vci >> 12) & 0xf);
	cell[2] = drv->vci >> 4;
	cell[3] = (drv->vci & 0xf ) << 4;
	// The header checksum will be added by the card...

	// Add payload
	payload_len = skb->len >= ATM_CELL_PAYLOAD ? ATM_CELL_PAYLOAD : skb->len;
	if (payload_len > 0) {
		memcpy(cell+5, skb->data, payload_len);
		skb_pull(skb, payload_len);
	}
	
	pad_len = ATM_CELL_PAYLOAD - payload_len;
	// Is this the last cell ?
	if (pad_len >=  ATM_AAL5_TRAILER) {
		// Last cell
		pad_len -= ATM_AAL5_TRAILER;
		// setting pti bit in last cell
		cell[3] |= 0x02; 
	}

	DBG(DATA_D,"payload_len=%d,pad_len=%d,pti=%x\n",payload_len,pad_len,cell[3] & 0x2);

	// Add zero padding
	if (pad_len > 0) {
		memset(cell+5+payload_len, 0, pad_len);
	}

	if (cell[3] & 0x02) {
		// Add AAL5 trailer
		unsigned char *trailer = cell+ATM_CELL_SIZE-ATM_AAL5_TRAILER;
		*trailer++ = (unsigned char) 0; /* UU  = 0 */
		*trailer++ = (unsigned char) 0; /* CPI = 0 */
		*trailer++ = (unsigned char) (skb_data->tx.aal5.pdu_length >> 8);
		*trailer++ = (unsigned char) (skb_data->tx.aal5.pdu_length & 0xff);
		// Update and add CRC
		skb_data->tx.aal5.crc = ~crc32(skb_data->tx.aal5.crc, cell+5, ATM_CELL_PAYLOAD-4);
		*trailer++ = (unsigned char) (skb_data->tx.aal5.crc >>  24);
		*trailer++ = (unsigned char) (skb_data->tx.aal5.crc >>  16);
		*trailer++ = (unsigned char) (skb_data->tx.aal5.crc >>   8);
		*trailer++ = (unsigned char) (skb_data->tx.aal5.crc & 0xff);
		return 1;
	} else {
		// Update CRC
		skb_data->tx.aal5.crc = crc32(skb_data->tx.aal5.crc, cell+5, ATM_CELL_PAYLOAD);
		return 0;
	}
}

static void snd_poll(struct unicorn_ethdrv *drv)
{
	struct unicorn_entrypoints *ep = drv->ep;
	int max_cells,cells;
	unsigned char *cell;
	struct sk_buff *skb;
	int finished;
	int t;

	if (!ep) return;
	ASSERT(ep->snd_getcell != NULL);

	// do traffic shaping
	drv->ms_counter += POLL_TIME;
	if (drv->ms_counter >= 1000) {
		// 1 sec
		DBG(DATA_D,"max_cell_counter=%d,us_cell_counter=%ld\n",
		    drv->max_cell_counter,drv->us_cell_counter);
		drv->ms_counter = 0;
		drv->max_cell_counter = 0;
		drv->curr_us_rate = (drv->us_cell_counter*ATM_CELL_SIZE*8UL)/1000UL;
		drv->us_cell_counter = 0;

		// Downstream
		drv->curr_ds_rate = (drv->ds_cell_counter*ATM_CELL_SIZE*8UL)/1000UL;
		drv->ds_cell_counter = 0;
	}

	t = 1000-drv->ms_counter;
	max_cells = (((drv->link_rate - drv->max_cell_counter) * POLL_TIME) + (t/2))/t;
	drv->max_cell_counter += max_cells;
	
	cells=0;
	skb = peek_tx_skb(drv);
	while ((cells < max_cells) &&
	       (skb != NULL) && 
	       ((cell = ep->snd_getcell(ep->dev)) != NULL)) {
		++cells;
                
		// statistics
		++drv->us_cell_counter;

		switch (ATM_EXT_SKB(skb)->aal) {
		case ATM_AAL0:
			finished = build_aal0_cell(drv,cell,skb);
			break;
		case ATM_AAL5:
			finished = build_aal5_cell(drv,cell,skb);
			break;
		default:
			WARN("AAL%d not supported\n",ATM_EXT_SKB(skb)->aal==ATM_AAL0 ? 0 : ATM_EXT_SKB(skb)->aal);
			finished=1;
			break;
		}

		if (finished) {
			// Finished with this buffer
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14))
			skb_unlink(skb,&drv->tx_q);
#else
			skb_unlink(skb);
#endif
			dev_kfree_skb_any(skb);
			// Get next buffer
			skb = peek_tx_skb(drv);
		}
	}

	if (cells) {
		ep->start_transmit(ep->dev);
		DBG(DATA_D,"%d/%d cells\n",cells,max_cells);
	}
}

static void oam_loopback(struct unicorn_ethdrv *drv,unsigned char *cell)
{
	struct sk_buff *skb;
	struct atm_ext_skb_data *skb_data;

	DBG(ATM_D,"\n");

	skb = dev_alloc_skb(ATM_CELL_SIZE);
	if (!skb) {
		WARN("dev_alloc_skb of size %d failed\n",ATM_CELL_SIZE);
		return ;
	}
	memcpy(skb->data,cell,ATM_CELL_SIZE);
	skb_put(skb,ATM_CELL_SIZE);

	// AAL0
	skb_data = ATM_EXT_SKB(skb);
	skb_data->aal = ATM_AAL0;
	skb_data->tx.aal0.pdu_length = skb->len;

	// put skb on transmit queue 
	skb_queue_tail(&drv->tx_q, skb);
}

static void rcv_oam(struct unicorn_ethdrv *drv,unsigned char *cell,int vpi,int vci)
{
	unsigned char *pdu= cell+5;
	unsigned char pti = cell[3] & 0x0E;
	unsigned short rcvdcrc,crc;

	DBG(ATM_D,"vpi=%d,vci=%d,pti=0x%02x,type=0x%02x,indicator=0x%02x,LLID=0x%02x\n",
	    vpi,vci,pti,pdu[0],pdu[1],pdu[6]);

	if (vci == 21) {
		oam_loopback(drv,cell);
		return;
	}

	// Process only OAM-F4 flow and OAM-F5 flow cells
	switch (pti) {
	case 0x00:		// OAM-F4 flow cell
	case 0x04:		// OAM-F4 flow cell
		// VCI = 3  -> Segment OAM-F4
		// VCI = 4  -> End-to-end OAM-F4
		if (vci != 3 && vci != 4) return;
		break;
	case 0x08:		// Segment OAM-F5 flow cell
	case 0x0A:	        // End-to-end OAM-F5 flow cell
		break;
	default:
		return;
	}

	// Save the received CRC
	rcvdcrc =  (unsigned short)pdu[ATM_CELL_PAYLOAD-1];
	rcvdcrc += (unsigned short)pdu[ATM_CELL_PAYLOAD-2]<<8;
	rcvdcrc &= 0x03FF;

	// Put 0 in the CRC field of the received PDU
	pdu[ATM_CELL_PAYLOAD-2] &= 0xFC;
	pdu[ATM_CELL_PAYLOAD-1] = 0;

	// Computes the CRC10 and check for the result
	crc = crc10(0,pdu,ATM_CELL_PAYLOAD);
	if (crc != rcvdcrc) {
		WARN("Received OAM cell with invalid CRC\n");
		return;
	}

	// Increment the counters according to the OAM cell type
	switch(pdu[0]) {
	case 0x10:				
		++drv->oam_stats.rx_AIS;
		break;
	case 0x11:				
		++drv->oam_stats.rx_RDI;
		break;
	case 0x14:				
		++drv->oam_stats.rx_CC;
		break;
	case 0x18:
		if (pdu[6] != 0x6a) { // LLID field
			if(pdu[1] == 0x01) {
				++drv->oam_stats.rx_ne_LB;
				pdu[1] = 0x00;
				// clear CRC
				pdu[ATM_CELL_PAYLOAD-2]=pdu[ATM_CELL_PAYLOAD-1]=0;
				crc = crc10(0,pdu,ATM_CELL_PAYLOAD);
				pdu[ATM_CELL_PAYLOAD-2] = (unsigned char)(crc >> 8);
				pdu[ATM_CELL_PAYLOAD-1] = (unsigned char)(crc);
				oam_loopback(drv,cell);
				return;
			} else if (pdu[1] == 0x00) {
				++drv->oam_stats.rx_fe_LB;
			}
			// save the VPI.VCI for auto-configuration
			if ((drv->vpi == ATM_VPI_UNSPEC) && (drv->vci == ATM_VCI_UNSPEC)) {
				drv->vpi = vpi;
				drv->vci = vci;
			}
		}
		break;
	}
}

static struct sk_buff *rawcell_decode(struct unicorn_ethdrv *drv, unsigned char *cell)
{
   	struct sk_buff *skb;
	unsigned int max_mru = drv->dev->mtu+ATM_CELL_PAYLOAD; // the AAL5 padding may take up to 1 cell
	
	// get reassembly buffer
	skb = drv->reas;
	if (!skb) {
		skb = drv->reas = dev_alloc_skb(max_mru);
		if (!skb) {
			WARN("dev_alloc_skb failed\n");
			return NULL;
		}
		skb->dev = drv->dev;
	}
    
	// copy data
        if (skb->len > (max_mru-ATM_CELL_PAYLOAD)) {
		skb_trim(skb, 0);
	}
	memcpy(skb_put(skb,ATM_CELL_PAYLOAD), cell+5, ATM_CELL_PAYLOAD);

  	// check for end of buffer
  	if (cell[3] & 0x2) {
		// the aal5 buffer ends here, cut the buffer. 		
		drv->reas = NULL;
	} else {
		drv->reas = skb;
		skb = NULL;
	}
	return skb;
}

static struct sk_buff *aal5_decode(struct sk_buff *skb) 
{
	unsigned long crc = CRC32_INITIAL;
	unsigned long pdu_crc;
  	unsigned int length,pdu_length;
	
	if (skb->len && (skb->len % ATM_CELL_PAYLOAD)) {
		WARN("wrong size %d\n",skb->len);
		goto fail;
	}

	length      = (skb->tail[-6] << 8) + skb->tail[-5];
	pdu_crc     = (skb->tail[-4] << 24) + (skb->tail[-3] << 16) + (skb->tail[-2] << 8) + skb->tail[-1];
	pdu_length  = ((length + 47 + 8)/ 48) * 48;

  	DBG(DATA_D,"skb->len=%d,length=%d,pdu_crc=0x%lx,pdu_length=%d\n",skb->len,length,pdu_crc,pdu_length);
  
  	// is skb long enough ? 
  	if (skb->len < pdu_length) {
		WARN("skb to short,skb->len=%d,pdu_length=%d\n",skb->len,pdu_length);
		goto fail;
	}
  
 	// is skb too long ? 
  	if (skb->len > pdu_length) {
		WARN("readjusting illegal skb->len %d -> %d\n",skb->len,pdu_length);
		skb_pull(skb, skb->len - pdu_length);
	}

  	crc = ~crc32(crc,skb->data,pdu_length-4);
  
  	// check crc
  	if (pdu_crc != crc) {
		WARN("crc check failed!\n");
		goto fail;
	}

  	// pdu is ok 
	skb_trim(skb, length);
    	return skb;
  	
fail:
	dev_kfree_skb_any(skb);
	return NULL;
}

static void rcv_aal5(struct unicorn_ethdrv *drv,unsigned char *cell)
{
	struct sk_buff *skb;

	skb = rawcell_decode(drv,cell);
	if (!skb) 
		return;

	skb = aal5_decode(skb);
	if (!skb) 
		goto fail;
	
	DBG(DATA_D,"vpi=%d,vci=%d\n",drv->vpi,drv->vci);
	DUMP_PACKET(DATA_D,skb->data,skb->len);
	
	++drv->eth_stats.rx_packets;
	drv->eth_stats.rx_bytes += skb->len;
	
	if (drv->protocol == RFC2684_BRIDGED) {
		// remove encaps
		if (drv->encaps == LLC) {
			if ((skb->len >= sizeof(BRIDGED_LLC_OUI_PID)) &&
			    (memcmp(skb->data,BRIDGED_LLC_OUI_PID,sizeof(BRIDGED_LLC_OUI_PID))==0)) {
				skb_pull(skb,sizeof(BRIDGED_LLC_OUI_PID));
			} else {
				WARN("wrong BRIDGED_LLC_OUI_PID encaps %02x%02x\n",skb->data[0],skb->data[1]);
				goto fail;
			}
		} else {
			if ((skb->len >= sizeof(PAD)) &&
			    (memcmp(skb->data,PAD,sizeof(PAD))==0)) {
				skb_pull(skb,sizeof(PAD));
			} else {
				WARN("wrong PAD encaps %02x%02x\n",skb->data[0],skb->data[1]);
				goto fail;
			}
		}
	} else if (drv->protocol == RFC2684_ROUTED) {
		struct ethhdr ethhdr;
		struct iphdr *iphdr;
		// remove encaps
		if (drv->encaps == LLC) {
			if ((skb->len >= sizeof(ROUTED_LLC_OUI_PID)) &&
			    (memcmp(skb->data,ROUTED_LLC_OUI_PID,sizeof(ROUTED_LLC_OUI_PID))==0)) {
				skb_pull(skb,sizeof(ROUTED_LLC_OUI_PID));
			} else {
				WARN("wrong ROUTED_LLC_OUI_PID encaps %02x%02x\n",skb->data[0],skb->data[1]);
				goto fail;
			}
		}
		// add enet header
		iphdr = (struct iphdr *)skb->data;
		memcpy(ethhdr.h_dest,drv->dev->dev_addr,ETH_ALEN);
		make_hw_addr(iphdr->saddr,ethhdr.h_source);
		ethhdr.h_proto = htons(ETH_P_IP);
		if (skb_add_header(skb,(unsigned char *)&ethhdr.h_dest,ETH_HLEN)) {
			goto fail;
		}
	} else if (drv->protocol == RFC2364) {
		struct pppoe_hdr pppoe_hdr;
		struct ethhdr ethhdr;
		// remove encaps
		if (drv->encaps == LLC) {
			if ((skb->len >= sizeof(PPPOA_LLC_ENCAPS)) &&
			    (memcmp(skb->data,PPPOA_LLC_ENCAPS,sizeof(PPPOA_LLC_ENCAPS))==0)) {
				skb_pull(skb,sizeof(PPPOA_LLC_ENCAPS));	
			} else {
				WARN("wrong PPPOA_LLC_ENCAPS %02x%02x\n",skb->data[0],skb->data[1]);
				goto fail;
			}
		}
		// add PPPoE header
		pppoe_hdr.ver = 1;
		pppoe_hdr.type = 1;
		pppoe_hdr.code = 0;
		pppoe_hdr.sid = htons(pppoe_unique_session_id);
		pppoe_hdr.length = htons(skb->len); 
		if (skb_add_header(skb,(unsigned char *)&pppoe_hdr,sizeof(struct pppoe_hdr))) {
			goto fail;
		}
		// add enet header
		memcpy(ethhdr.h_dest,drv->dev->dev_addr,ETH_ALEN);
		memcpy(ethhdr.h_source,PPPOE_SERVER_ADDR,ETH_ALEN);
		ethhdr.h_proto = htons(ETH_P_PPP_SES);
		if (skb_add_header(skb,(unsigned char *)&ethhdr.h_dest,ETH_HLEN)) {
			goto fail;
		}
	}
	DUMP_PACKET(DATA_D,skb->data,skb->len);

	drv->dev->last_rx = jiffies;
	skb->protocol = eth_type_trans(skb,drv->dev);
	DBG(ATM_D,"skb->len=%d,skb->protocol=%04x\n",skb->len,htons(skb->protocol));
	netif_rx(skb);
	return;

fail:
	if (skb) 
		dev_kfree_skb_any(skb);
	++drv->eth_stats.rx_errors;
	return;
}


static void rcv_poll(struct unicorn_ethdrv *drv)
{
	struct unicorn_entrypoints *ep = drv->ep;
	unsigned char *cell;
	
	if (!ep) return;

	ASSERT(ep->rcv_getcell != NULL);

	while ((cell = ep->rcv_getcell(ep->dev)) != NULL) {
		int vpi = ((cell[0] & 0xf) << 4) | (cell[1] >> 4);
		int vci = ((cell[1] & 0xf) << 12) | (cell[2] << 4) | (cell[3] >> 4);
		// Check for OAM cells and process them
		if (((cell[3] & 0x0E) >= 8) ||		// OAM-F5
		    ((vpi==0) && (vci==21)) ||		// OAM-F4
		    ((vpi==0) && (vci==3)) ||		
		    ((vpi==0) && (vci==4))) {
			rcv_oam(drv,cell,vpi,vci);
		} else if ((vpi == drv->vpi) && (vci == drv->vci)) {
			// statistics
			++drv->ds_cell_counter;

			rcv_aal5(drv,cell);
		} else {
			DBG(ATM_D,"unknown VPI.VCI %d.%d\n",vpi,vci);
		}
	}
}

static void unicorn_poll_data(unsigned long context)
{
	struct unicorn_ethdrv *drv = (struct unicorn_ethdrv *)context;
	ADSL_STATUS prev_adsl_status;	

	if (!drv->ep || !drv->dev) goto restart;

	prev_adsl_status = drv->adsl_status;
	drv->adsl_status = drv->ep->get_adsl_status(drv->ep->dev);

	if (drv->adsl_status != prev_adsl_status) {
		// ADSL state has changed...
		if (drv->adsl_status == ADSL_STATUS_ATMREADY) {
			// get the new ADSL link rate
			drv->link_rate = get_link_rate(drv);
			//netif_start_queue(drv->dev);
		} else {
			drv->link_rate = 0;
			//netif_stop_queue(drv->dev);

			// free all buffers in transmit queues
			purge_tx_q(drv);
		}
	}

	if (drv->adsl_status == ADSL_STATUS_ATMREADY) {
		rcv_poll(drv);
		snd_poll(drv);
	}

 restart:
	drv->timer.expires = ((POLL_TIME*HZ)/1000) + jiffies;
	add_timer(&drv->timer);	
}


static void simulate_arp_reply(struct sk_buff *skb,struct net_device *eth_dev)
{
	struct ethhdr *ethhdr = (struct ethhdr *)skb->data;
	struct arphdr_eth *arphdr = (struct arphdr_eth *)&skb->data[ETH_HLEN];
				
	if (arphdr->ar_op == htons(ARPOP_REQUEST)) {
		DBG(ATM_D,"ARP request for %d.%d.%d.%d\n",NIPQUAD(arphdr->ar_tip));
		if ((arphdr->ar_sip != arphdr->ar_tip) && (arphdr->ar_sip != 0L)) {
			__u32 sip = arphdr->ar_sip;
			arphdr->ar_op = htons(ARPOP_REPLY);
			arphdr->ar_sip = arphdr->ar_tip;
			arphdr->ar_tip = sip;
			memcpy(arphdr->ar_tha,arphdr->ar_sha,ETH_ALEN);
			// Make up source HW address using target IP address
			make_hw_addr(arphdr->ar_sip,arphdr->ar_sha);

			memcpy(ethhdr->h_dest,arphdr->ar_tha,ETH_ALEN);
			memcpy(ethhdr->h_source,arphdr->ar_sha,ETH_ALEN);
			skb->protocol = eth_type_trans(skb,eth_dev);
			skb->dev = eth_dev;
			DBG(ATM_D,"skb->len=%d,skb->protocol=%04x\n",skb->len,htons(skb->protocol));
			netif_rx(skb);
		} else {
			// Checking my IP address, ignore
			dev_kfree_skb_any(skb);
		}
	} else {
		DBG(ATM_D,"not ARP request,dropping packet\n");
		dev_kfree_skb_any(skb);		
	}	
}

static void simulate_pppoe_server_reply(struct sk_buff *skb,struct net_device *eth_dev)
{
    struct ethhdr *ethhdr = (struct ethhdr *)skb->data;
    struct pppoe_hdr *pppoe_hdr = (struct pppoe_hdr *)&skb->data[ETH_HLEN];
    struct pppoe_tag *tags = (struct pppoe_tag *)&skb->data[ETH_HLEN+sizeof(struct pppoe_hdr)];
    struct pppoe_tag *ac_name_tag;

    if (skb->len < ETH_HLEN+sizeof(struct pppoe_hdr)) {
	    DBG(ATM_D,"PPPoE packet too short (%d)\n",skb->len);
	    goto drop;
    }
    
    switch (pppoe_hdr->code) {
    case PADI_CODE:
	    DBG(ATM_D,"PADI received,send PADO\n");
	    pppoe_hdr->ver = 1;
	    pppoe_hdr->type = 1;
	    pppoe_hdr->code = PADO_CODE;
	    pppoe_hdr->sid = 0;
	    
	    ac_name_tag = (struct pppoe_tag *)((unsigned char *)tags + ntohs(pppoe_hdr->length));
	    ac_name_tag->tag_type = PTT_AC_NAME;
	    ac_name_tag->tag_len = htons(2);
	    ac_name_tag->tag_data[0] = 'S';
	    ac_name_tag->tag_data[1] = 'T';
	    
	    pppoe_hdr->length = htons(ntohs(pppoe_hdr->length)+6); 
	    break;
    case PADR_CODE:
	    DBG(ATM_D,"PADR received,send PADS\n");
	    pppoe_hdr->ver = 1;
	    pppoe_hdr->type = 1;
	    pppoe_hdr->code = PADS_CODE;
	    if (++pppoe_unique_session_id == 0)
		    pppoe_unique_session_id = 1;
	    pppoe_hdr->sid = htons(pppoe_unique_session_id);
	    break;
    case PADT_CODE:
	    DBG(ATM_D,"PADT received\n");
	    goto drop;
	    break;
    case PADO_CODE:
    case PADS_CODE:
    default:
	    goto drop;
	    break;
    }

    skb->len = ETH_HLEN+sizeof(struct pppoe_hdr)+ntohs(pppoe_hdr->length);
    skb->tail = skb->data + skb->len;
    memcpy(ethhdr->h_dest,eth_dev->dev_addr,ETH_ALEN);
    memcpy(ethhdr->h_source,PPPOE_SERVER_ADDR,ETH_ALEN);
    skb->protocol = eth_type_trans(skb,eth_dev);
    skb->dev = eth_dev;
    DBG(ATM_D,"skb->len=%d,skb->protocol=%04x\n",skb->len,htons(skb->protocol));
    netif_rx(skb);
    return;

 drop:    
    DBG(ATM_D,"PPPoE packet dropped\n");
    dev_kfree_skb_any(skb);
    return;
}

static int unicorn_eth_send(struct sk_buff *skb, struct net_device *eth_dev) 
{
	struct unicorn_ethdrv *drv = (struct unicorn_ethdrv *) eth_dev->priv;
	struct atm_ext_skb_data *skb_data;
 	int status;
 
	DBG(DATA_D,"vpi=%d,vci=%d,protocol=%d,encaps=%d\n",
	    drv->vpi,drv->vci,drv->protocol,drv->encaps);
	DUMP_PACKET(DATA_D,skb->data,skb->len);

	if (!drv->ep) {
		WARN("no bus driver\n");    
		status = -ENXIO;
		goto fail;
	}
	if (drv->adsl_status != ADSL_STATUS_ATMREADY) {
		DBG(ATM_D,"ATM not ready,adsl_status=%d\n",drv->adsl_status);
		status = -EIO;
		goto fail;
	} 

	if (drv->protocol == RFC2684_BRIDGED) {
		// add encaps
		if (drv->encaps == LLC) {
			status = skb_add_header(skb,BRIDGED_LLC_OUI_PID,sizeof(BRIDGED_LLC_OUI_PID));
		} else {
			status = skb_add_header(skb,PAD,sizeof(PAD));
		}
	} else if (drv->protocol == RFC2684_ROUTED) {
		struct ethhdr *ethhdr = (struct ethhdr *)skb->data;
		if (ethhdr->h_proto == htons(ETH_P_ARP)) {
			simulate_arp_reply(skb,eth_dev);
			return 0;
		} else if (ethhdr->h_proto == htons(ETH_P_IP)) {
			// remove ethernet header
			skb_pull(skb,ETH_HLEN);
			// add encaps
			if (drv->encaps == LLC) {
				status = skb_add_header(skb,ROUTED_LLC_OUI_PID,sizeof(ROUTED_LLC_OUI_PID));
			} else {
				status = 0;
			}
		} else {
			status = -EIO; // drop
		}
	} else if (drv->protocol == RFC2364) {
		struct ethhdr *ethhdr = (struct ethhdr *)skb->data;
		if (ethhdr->h_proto == htons(ETH_P_PPP_DISC)) {
			simulate_pppoe_server_reply(skb,eth_dev);
			return 0;
		} else if (ethhdr->h_proto == htons(ETH_P_PPP_SES)) {
			// remove ethernet header + PPPOE header
			skb_pull(skb,ETH_HLEN+sizeof(struct pppoe_hdr));
			// add encaps
			if (drv->encaps == LLC) {
				status = skb_add_header(skb,PPPOA_LLC_ENCAPS,sizeof(PPPOA_LLC_ENCAPS));
			} else {
				status = 0;
			}
		} else {
			status = -EIO;// drop
		}
	} else {
		status = -EINVAL;
	}
	if (status) {
		goto fail;
	}
	DUMP_PACKET(DATA_D,skb->data,skb->len);
	++drv->eth_stats.tx_packets;
	drv->eth_stats.tx_bytes += skb->len;

	// AAL5
	skb_data = ATM_EXT_SKB(skb);
	skb_data->aal = ATM_AAL5;
	skb_data->tx.aal5.pdu_length = skb->len;
	skb_data->tx.aal5.crc = CRC32_INITIAL;

	// put skb on transmit queue 
	skb_queue_tail(&drv->tx_q, skb);
	eth_dev->trans_start = jiffies;
	return 0;

fail:
	++drv->eth_stats.tx_errors;
	DBG(ATM_D,"status=%d\n",status);
	if (skb) dev_kfree_skb(skb);
	return 0; // do not return status as this causes panic
}

void start_poll_timer(struct unicorn_ethdrv *drv)
{
	drv->ms_counter = 0;
	drv->max_cell_counter = 0;

	DBG(ATM_D,"POLL_TIME=%dms\n",POLL_TIME);

	ASSERT(drv->ep != NULL);

	// empty the receive buffer
	while ((drv->ep->rcv_getcell(drv->ep->dev)) != NULL) {
	}
	
	// start send and receive poll timer
	init_timer(&drv->timer);
	
	drv->timer.data = (unsigned long)drv;
	drv->timer.function = unicorn_poll_data;
	drv->timer.expires = ((POLL_TIME*HZ)/1000) + jiffies;
	
	add_timer(&drv->timer);	 
}

static void stop_poll_timer(struct unicorn_ethdrv *drv)
{
	// stop timer
	del_timer(&drv->timer);
	// free all buffers in transmit queues
	purge_tx_q(drv);
}

static int unicorn_eth_open(struct net_device *eth_dev) 
{
	struct unicorn_ethdrv *drv = (struct unicorn_ethdrv *)eth_dev->priv;

	DBG(ATM_D,"vpi=%d,vci=%d\n",drv->vpi,drv->vci);

	if (!drv->ep) return -ENXIO;

	drv->reas = NULL;

	// initialize transmit queue
	skb_queue_head_init(&drv->tx_q);

	drv->adsl_status = drv->ep->get_adsl_status(drv->ep->dev);
	// get the ADSL link rate
	drv->link_rate = get_link_rate(drv);

	// Install the proc_read function in /proc/net/atm/
#ifndef CONFIG_ATM
	atm_proc_root = proc_mkdir("net/atm",NULL);
	if (atm_proc_root) atm_proc_root->owner=THIS_MODULE;
#endif	
	if (atm_proc_root) {
		drv->proc_dir_entry = create_proc_read_entry("UNICORN:0",0,atm_proc_root,unicorn_eth_proc_read,drv);
		if (drv->proc_dir_entry) {
			drv->proc_dir_entry->owner = THIS_MODULE;
			INFO("created proc entry at /proc/net/atm/UNICORN:0\n");
		} else {
			WARN("no proc entry installed\n");
		}
	} else {
		WARN("no proc entry installed\n");
	}
	
	// first VCC, start data poll timer
	start_poll_timer(drv);
	
	//if (drv->adsl_status == ADSL_STATUS_ATMREADY) {
		netif_start_queue(eth_dev);
		//}
	return 0;
}

static int 
unicorn_eth_close(struct net_device *eth_dev) 
{
	struct unicorn_ethdrv *drv = (struct unicorn_ethdrv *)eth_dev->priv;
  
	DBG(ATM_D,"\n");

	netif_stop_queue(eth_dev);

	// freeing resources
	if (drv->reas) {
		dev_kfree_skb_any(drv->reas);
		drv->reas = NULL;
	}

	// freeing address 
	drv->vpi = ATM_VPI_UNSPEC;
	drv->vci = ATM_VCI_UNSPEC;

	// last VCC, stop data poll timer
	stop_poll_timer(drv);

#ifndef CONFIG_ATM
	if (atm_proc_root) remove_proc_entry("UNICORN:0",atm_proc_root);
#endif
	return 0;
}

static struct net_device_stats *unicorn_eth_stats(struct net_device *eth_dev)
{
	struct unicorn_ethdrv *drv = (struct unicorn_ethdrv *)eth_dev->priv;
	return &drv->eth_stats;
}

static int oam_send(struct unicorn_ethdrv *drv,int type,int vpi,int vci)
{
	struct sk_buff *skb;
	struct atm_ext_skb_data *skb_data;
	unsigned char *hdr,*pdu;
	unsigned short crc;
	static unsigned long tag = 0x0L; 
	const unsigned char lb_id[] = {
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF};
	const unsigned char src_id[] = {
		0x6A,0x6A,0x6A,0x6A,
		0x6A,0x6A,0x6A,0x6A,
		0x6A,0x6A,0x6A,0x6A,
		0x6A,0x6A,0x6A,0x6A};	

	DBG(ATM_D,"vpi=%d,vci=%d\n",vpi,vci);

	if (drv->adsl_status != ADSL_STATUS_ATMREADY) {
		DBG(ATM_D,"ATM not ready,adsl_status=%d\n",drv->adsl_status);
		return -EIO;
	} 

	skb = dev_alloc_skb(ATM_CELL_SIZE);
	if (!skb) {
		return -ENOMEM;
	}
	skb->dev = drv->dev;
	
	hdr = skb->data;

	// fill vpi
	hdr[0] = vpi >> 4;
	hdr[1] = (vpi & 0x0F) << 4;

	// fill vci
	hdr[1] |= (vci >> 12);
	hdr[2] = (vci >> 4) & 0xFF;
	if (type == ATM_OAM_F4) {
		hdr[3] = 0x00; // OAM F4
	} else {
		hdr[3] = 0x0A; // OAM F5
	}
	hdr[3] |= ((vci & 0x0F) << 4);
	// update the HEC
	hdr[4] = hecCompute(hdr);

	pdu = hdr+5;
	// loopback function
	pdu[0] = 0x18;
	pdu[1] = 0x01;

	// fill the correlation tag
	pdu[2] = (unsigned char)(tag >> 24);
	pdu[3] = (unsigned char)(tag >> 16);
	pdu[4] = (unsigned char)(tag >> 8);
	pdu[5] = (unsigned char)tag;
	tag++;

	// fill the loopback id
	memcpy(pdu+6,lb_id,16);

	// fill the Src Id
	memcpy(pdu+22,src_id,16);

	// fill the unused bytes
	memcpy(pdu+38,src_id,8);

	// calculate CRC
	pdu[ATM_CELL_PAYLOAD-2]=pdu[ATM_CELL_PAYLOAD-1]=0;
	crc = crc10(0,pdu,ATM_CELL_PAYLOAD);
	pdu[ATM_CELL_PAYLOAD-2] = (unsigned char)(crc >> 8);
	pdu[ATM_CELL_PAYLOAD-1] = (unsigned char)(crc);

	++drv->oam_stats.tx_LB;
	
	skb_put(skb,ATM_CELL_SIZE);

	// AAL0
	skb_data = ATM_EXT_SKB(skb);
	skb_data->aal = ATM_AAL0;
	skb_data->tx.aal0.pdu_length = skb->len;

	// put skb on transmit queue 
	skb_queue_tail(&drv->tx_q, skb);
	return 0;	
}

static int net_control(struct unicorn_ethdrv *drv,T_MswCtrl *ctrl)
{
	int err;

	switch (ctrl->code) {
	case NET_CTL_SET_VPI_VCI:
		{
			T_atm_channel *vpi_vci=ctrl->buffer;
			drv->vpi = vpi_vci->vpi;	       
			drv->vci = vpi_vci->vci;	       
			ctrl->retcode = err = 0;
			ctrl->length = 0;
		}
		break;
	case NET_CTL_GET_VPI_VCI:
		{
			T_atm_channel *vpi_vci=ctrl->buffer;
			vpi_vci->type = ATM_AAL5;
			vpi_vci->vpi = drv->vpi;
			vpi_vci->vci = drv->vci;
			ctrl->length = sizeof(T_atm_channel);
			ctrl->retcode = err = 0;
		}
		break;
	case NET_CTL_TX_OAM_CELL:
		{
			T_atm_channel *tx_oam = ctrl->buffer;
			err = oam_send(drv,tx_oam->type,tx_oam->vpi,tx_oam->vci);
			ctrl->retcode = err;
			ctrl->length = 0;
		}
		break;
	case NET_CTL_GET_OAM_STATS:
		{
			memcpy(ctrl->buffer,&drv->oam_stats,sizeof(T_oam_stats));
			ctrl->length = sizeof(T_oam_stats);
			ctrl->retcode = err = 0;
		}
		break;
	case NET_CTL_RESET_OAM_STATS:
		{
			memset(&drv->oam_stats,0,sizeof(T_oam_stats));
			ctrl->retcode = err = 0;
			ctrl->length = 0;
			
		}
		break;
	default:
		err = -ENOTTY;
		break;
	}
	return err;
}

static int unicorn_eth_ioctl(struct net_device *eth_dev, struct ifreq *rq, int cmd) 
{
	struct unicorn_ethdrv *drv = (struct unicorn_ethdrv *)eth_dev->priv;
	T_MswCtrl ctrl;
	void *user_buffer;
	int err = -ENOTTY;
	
	DBG(DATA_D,"if %s,cmd=%x\n",rq->ifr_name,cmd);

	if (!drv->ep) return -ENXIO;

	switch (cmd) {
	case ETH_MSW_CTL:
		err = copy_from_user(&ctrl,rq->ifr_data,sizeof(T_MswCtrl));
		// if a buffer is provided, copy it to kernel space
		if ((ctrl.length) && (ctrl.buffer)) {
			user_buffer = ctrl.buffer;
			ctrl.buffer = kmalloc(ctrl.length,GFP_KERNEL);
			if (!ctrl.buffer) return -ENOMEM;
			err = copy_from_user(ctrl.buffer,user_buffer,ctrl.length);
		} else {
			user_buffer = NULL;
		}

		// do the ioctl...
		if (ctrl.code < 32) {
			ASSERT(drv->ep->msw_control != NULL);
			err =  drv->ep->msw_control(drv->ep->dev,&ctrl);
		} else if (ctrl.code < 64) {
			err = net_control(drv,&ctrl);
		} else {
		}
		if ((ctrl.length) && (ctrl.buffer)) { 
			err = copy_to_user(user_buffer,ctrl.buffer,ctrl.length);
			kfree(ctrl.buffer);
			ctrl.buffer = user_buffer;
		}
		break;
	}
	return err;
}

static int 
unicorn_eth_change_mtu(struct net_device *eth_dev, int new_mtu)
{
	DBG(ATM_D,"MTU=%d\n",new_mtu);
	eth_dev->mtu = new_mtu;
	return 0;
}

static void 
unicorn_eth_set_multicast(struct net_device *eth_dev)
{
}

static void 
unicorn_eth_tx_timeout(struct net_device *eth_dev)
{
	WARN("\n");
}

module_param_string(if_name, if_name, sizeof(if_name), 0);
module_param_string(mac_address, mac_address, sizeof(mac_address), 0);
module_param(VPI, int, 0);
module_param(VCI, int, 0);
module_param_string(PROTOCOL, PROTOCOL, sizeof(PROTOCOL), 0);
module_param_string(ENCAPS, ENCAPS, sizeof(ENCAPS), 0);
#if DEBUG
#ifdef ETH_DRIVER
module_param(DebugLevel, ulong, 0);
#endif
#endif

int unicorn_eth_init(void)
{
	struct net_device *eth_dev;
	struct unicorn_ethdrv *drv;

	INFO("v %d.%d.%d, " __TIME__ " " __DATE__"\n",
	     (VERS>>8)&0xf,(VERS>>4)&0xf,VERS&0xf);
	INFO("PROTOCOL=%s,VPI=%d,VCI=%d,ENCAPS=%s\n",
	     PROTOCOL ? PROTOCOL : "",VPI,VCI,ENCAPS ? ENCAPS : "");

	if (unicorn_ethdrv) {
		WARN("driver already opened\n");
		return -ENXIO;
	}

	eth_dev = alloc_etherdev(sizeof(struct unicorn_ethdrv));
	if (!eth_dev || !eth_dev->priv) {
		WARN("no memory for drv data\n");
		return -ENOMEM;
	}
	unicorn_ethdrv = drv = eth_dev->priv;
	memset(drv, 0, sizeof(struct unicorn_ethdrv));  
	SET_MODULE_OWNER(eth_dev);
	drv->dev = eth_dev;
	drv->adsl_status = ADSL_STATUS_NOHARDWARE;
	drv->vpi = VPI;
	drv->vci = VCI;

	drv->protocol = RFC2684_BRIDGED;
	drv->encaps = LLC;
	if (PROTOCOL) {
		if (strcmp(PROTOCOL,"pppoatm")==0) {
			drv->protocol = RFC2364;
			drv->encaps = VCMUX;			
		} else if (strcmp(PROTOCOL,"pppoe")==0) {
			drv->protocol = RFC2684_BRIDGED;
			drv->encaps = LLC;
		} else if (strcmp(PROTOCOL,"br2684")==0) {	
			drv->protocol = RFC2684_BRIDGED;
			drv->encaps = LLC;
		} else if (strcmp(PROTOCOL,"ipoatm")==0) {	
			drv->protocol = RFC2684_ROUTED;
			drv->encaps = VCMUX;			
		} else {
		}
	}
	if (ENCAPS) {
		if ((strcmp(ENCAPS,"vc-encaps")==0) || 
		    (strcmp(ENCAPS,"null")==0)) {
			drv->encaps = VCMUX;
		} else if (strcmp(ENCAPS,"llc-encaps")==0) {	
			drv->encaps = LLC;
		}
	}
	DBG(ATM_D,"protocol=%d,encaps=%d\n",drv->protocol,drv->encaps);

	if (drv->protocol == RFC2364) {
		eth_dev->mtu = ETH_DATA_LEN+8;
	} else {
		eth_dev->mtu = ETH_DATA_LEN;
	}

	if (if_name) {
		strncpy (eth_dev->name,if_name,IFNAMSIZ);
	} else {
		strncpy (eth_dev->name,UNICORN_ETH_NAME,IFNAMSIZ);
	}
	// set MAC address,
	unicorn_set_mac(eth_dev,mac_address);

	eth_dev->open               = unicorn_eth_open;
	eth_dev->stop               = unicorn_eth_close;
	eth_dev->do_ioctl           = unicorn_eth_ioctl;
	eth_dev->change_mtu         = unicorn_eth_change_mtu;
	eth_dev->hard_start_xmit    = unicorn_eth_send;
	eth_dev->get_stats          = unicorn_eth_stats;
	eth_dev->set_multicast_list = unicorn_eth_set_multicast;
	eth_dev->tx_timeout         = unicorn_eth_tx_timeout;
	eth_dev->watchdog_timeo     = HZ*5;
	
	return 0;
}

void unicorn_eth_cleanup(void)
{
	struct unicorn_ethdrv *drv = unicorn_ethdrv;

	DBG(ATM_D,"\n");

	if (drv) {
		if (drv->dev) {
			kfree(drv->dev);
		}
		unicorn_ethdrv = NULL;
	}
}

#ifdef ETH_DRIVER
module_init(unicorn_eth_init);
module_exit(unicorn_eth_cleanup);
#endif

extern int unicorn_attach(struct unicorn_entrypoints *entrypoints)
{
	int status;
	struct unicorn_ethdrv *drv = unicorn_ethdrv;
	
	DBG(ATM_D,"entrypoints=%p\n",entrypoints);

	if (!unicorn_ethdrv) {
#ifdef ETH_DRIVER
		return -ENXIO;
#else
		status = unicorn_eth_init();
		if (status) {
			return status;
		}
#endif
	}	
	drv = unicorn_ethdrv;

	ASSERT(entrypoints->dev != NULL);
	ASSERT(entrypoints->snd_getcell != NULL);
	ASSERT(entrypoints->rcv_getcell != NULL);
	ASSERT(entrypoints->start_transmit != NULL);
	ASSERT(entrypoints->get_adsl_status != NULL);
	ASSERT(entrypoints->get_adsl_linkspeed != NULL);	
	drv->ep = entrypoints;

	if (register_netdev(drv->dev)) {
		WARN("register_netdev failed\n");
		return -ENXIO;
	}
	DBG(ATM_D,"Ethernet interface %s registered,MTU=%d\n",
	    drv->dev->name,drv->dev->mtu);
	return 0;
}

extern int unicorn_detach(void)
{
	struct unicorn_ethdrv *drv = unicorn_ethdrv;

	DBG(ATM_D,"\n");

	if (!drv) {
		return -ENXIO;
	}
	DBG(ATM_D,"Unregister Ethernet interface %s\n",drv->dev->name);
	unregister_netdev(drv->dev);
	drv->adsl_status = ADSL_STATUS_NOHARDWARE;
	drv->ep = NULL;
#ifdef ETH_DRIVER
#else
	unicorn_eth_cleanup();
#endif
	return 0;
}

extern int unicorn_timer(void)
{
       return 0;
}
