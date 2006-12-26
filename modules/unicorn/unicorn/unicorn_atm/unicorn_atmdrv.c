/*
  This driver supports the Unicorn ADSL chipset from STMicroelectronics.
  The chipset consists of the ADSL DMT transceiver ST70137 and either the
  ST70134A or ST70136 Analog Front End (AFE).
  This file contains the ATM interface and SAR routines.
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
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/skbuff.h>
#include <asm/uaccess.h>
#include "types.h"
#include "amu/amas.h"
#include "crc.h"
#include "unicorn.h"
#include "debug.h"

#ifdef ATM_DRIVER
MODULE_AUTHOR ("fisaksen@bewan.com");
MODULE_DESCRIPTION ("ATM driver for the ST UNICORN ADSL modem.");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
#endif


// poll for data every POLL_TIME msec 
#ifdef USE_HW_TIMER
// minimum 2ms
#define POLL_TIME 6
#else
// multiple of 10ms in most cases
#define POLL_TIME 10
#endif

#define MAX_VC 8

// Private data
typedef struct unicorn_atmdrv {
	/* atm driver */
	struct atm_dev *atm_dev;
	struct sk_buff_head rt_tx_q;  // Real-Time transmit queue - CBR,VBR
	struct sk_buff_head tx_q;     // "Normal" transmit queue - UBR
	ADSL_STATUS adsl_status;
	unsigned long downstream_rate;
	unsigned long upstream_rate;
	unsigned long us_cell_counter;
	unsigned long ds_cell_counter;
	unsigned long curr_us_rate;
	unsigned long curr_ds_rate;
	int num_vccs;
	struct atm_vcc *vccs[MAX_VC];
	/* traffic shaping */
#ifdef USE_HW_TIMER
	int hw_tick;
#else
	struct timer_list timer;    
#endif
	int ms_counter;
	int max_cell_counter;  // counts max number of cells/sec for traffic shaping
	// Statistics
	T_oam_stats oam_stats;
	int vpi; 
	int vci;
	/* pci/usb driver entrypoints */
	struct unicorn_entrypoints *ep;
} unicorn_atmdrv_t;

// extended skb_data structure used when encoding cells
struct atm_ext_skb_data {
	struct atm_skb_data atm_skb_data;
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


static const char driver_name[] = "UNICORN";

struct unicorn_atmdrv *unicorn_atmdrv = NULL;

// driver parameters
char mac_address[ETH_ALEN*2 + 1] = { 0x0 };
#if DEBUG
#ifdef ATM_DRIVER
unsigned long DebugLevel=0; // ATM_D,DATA_D
#else
extern unsigned long DebugLevel;
#endif
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static int unicorn_atm_open(struct atm_vcc *vcc);
#else
static int unicorn_atm_open(struct atm_vcc *vcc, short vpi, int vci);
#endif
static void unicorn_atm_close(struct atm_vcc *vcc);
static int unicorn_atm_ioctl(struct atm_dev *dev, unsigned int cmd, void *arg);
static int unicorn_atm_send(struct atm_vcc *vcc, struct sk_buff *skb);
static int unicorn_atm_proc_read (struct atm_dev * atm_dev, loff_t * pos, char * page);


static const struct atmdev_ops atm_devops = 
{
	open:       unicorn_atm_open,
	close:      unicorn_atm_close,
	ioctl:      unicorn_atm_ioctl,
	send:       unicorn_atm_send,
	proc_read:  unicorn_atm_proc_read,
	owner:	    THIS_MODULE,
};

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

struct atm_dev *
unicorn_atm_startdevice(struct unicorn_atmdrv *drv, const struct atmdev_ops *devops) 
{
	struct atm_dev *atm_dev;

	atm_dev = atm_dev_register(driver_name, devops, -1, 0);
	if (!atm_dev) {
		return NULL;
	}

	DBG(ATM_D,"itf=%d\n",atm_dev->number);

	atm_dev->ci_range.vpi_bits = ATM_CI_MAX;
	atm_dev->ci_range.vci_bits = ATM_CI_MAX;
	atm_dev->signal = ATM_PHY_SIG_LOST;
	atm_dev->link_rate = 128*1000/(8*ATM_CELL_SIZE);

	atm_dev->dev_data = drv;
	drv->atm_dev = atm_dev;

	return atm_dev;
}

static void unicorn_atm_stopdevice(struct unicorn_atmdrv *drv) 
{
	struct atm_dev *atm_dev;
	
	if (!(atm_dev = drv->atm_dev)) {
		return;
	}

	atm_dev->link_rate = 0;	
	atm_dev->signal = ATM_PHY_SIG_LOST;
	atm_dev_deregister(atm_dev);
	drv->atm_dev = NULL;
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

static void unicorn_set_mac(struct unicorn_atmdrv *drv, const char *mac) 
{	
	struct atm_dev *atm_dev;

	if (!(atm_dev = drv->atm_dev)) {
		return;
	}
	if ((mac != NULL) && (strlen(mac) >= (ESI_LEN*2))) {
		int i;
		for (i=0; i < ESI_LEN; i++) {
			atm_dev->esi[i] = get_hex_digit(&mac[i*2]);
		}
		atm_dev->esi[0] &= ~0x01; // make sure it is not a broadcast address
	} else {
		/* Generate random Ethernet address.  */
		atm_dev->esi[0] = 0x00;
		get_random_bytes(&atm_dev->esi[1],ESI_LEN-1);
	}
	INFO("ESI=%02x:%02x:%02x:%02x:%02x:%02x\n",
	    atm_dev->esi[0],atm_dev->esi[1],atm_dev->esi[2],
	    atm_dev->esi[3],atm_dev->esi[4],atm_dev->esi[5]);
}


static AMSW_ModemState get_modemstate(struct unicorn_atmdrv *drv)
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

static int get_link_rate(struct unicorn_atmdrv *drv)
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

static int unicorn_atm_proc_read (struct atm_dev * atm_dev, loff_t * pos, char * page) 
{
	struct unicorn_atmdrv *drv = (struct unicorn_atmdrv *)atm_dev->dev_data;
	int left = *pos;
  
	if (!left--) {
		AMSW_ModemState modemstate = get_modemstate(drv);
		get_link_rate(drv);
		return sprintf(page, "ADSL: status %s, modem state %s, US rate %ldKbits/s, DS rate %ldKbits/s\n", 
			       get_adsl_status_string(drv->adsl_status),get_modemstate_string(modemstate),
			       drv->upstream_rate,drv->downstream_rate);

	}
	if (!left--) {
		return sprintf(page,"Current speed: US %ldKbits/s,DS %ldKbits/s\n",
			       drv->curr_us_rate,drv->curr_ds_rate);
	}

	if (!left--) {
		return sprintf(page, "Current speed: US %ldKbits/s,DS %ldKbits/s\n",
			       drv->curr_us_rate,drv->curr_ds_rate);
	}
	if (!left--) {
		return sprintf(page, "Bridged: %02x:%02x:%02x:%02x:%02x:%02x\n",
			       atm_dev->esi[0], atm_dev->esi[1], atm_dev->esi[2], atm_dev->esi[3], 
			       atm_dev->esi[4], atm_dev->esi[5]);
	
	}
	if (!left--) {
		return sprintf(page, "AAL5: tx %d ( %d err ), rx %d ( %d err, %d drop )\n",
			       atomic_read(&atm_dev->stats.aal5.tx), atomic_read(&atm_dev->stats.aal5.tx_err),
			       atomic_read(&atm_dev->stats.aal5.rx), atomic_read(&atm_dev->stats.aal5.rx_err),
			       atomic_read(&atm_dev->stats.aal5.rx_drop));
	}
	if (!left--) {
		return sprintf(page, "AAL0: tx %d ( %d err ), rx %d ( %d err, %d drop )\n",
			       atomic_read(&atm_dev->stats.aal0.tx), atomic_read(&atm_dev->stats.aal0.tx_err),
			       atomic_read(&atm_dev->stats.aal0.rx), atomic_read(&atm_dev->stats.aal0.rx_err),
			       atomic_read(&atm_dev->stats.aal0.rx_drop));
	}
	return 0;
}

static void tx_free_skb(struct sk_buff *skb)
{
	struct atm_vcc *vcc;

	if (skb) {
		vcc = ATM_EXT_SKB(skb)->atm_skb_data.vcc;
		if (vcc && vcc->pop) {
			vcc->pop(vcc,skb);
		} else {
			dev_kfree_skb_any(skb);
		}
	}
}

static void tx_unlink_skb(struct unicorn_atmdrv *drv,struct sk_buff *skb)
{
	if (skb) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14))
		struct atm_vcc *vcc = ATM_EXT_SKB(skb)->atm_skb_data.vcc;
		if (vcc) {
			if ((vcc->qos.txtp.traffic_class == ATM_CBR) || 
			    (vcc->qos.txtp.traffic_class ==  ATM_VBR)) {
				skb_unlink(skb, &drv->rt_tx_q);
			} else {
				skb_unlink(skb, &drv->tx_q);
			}
		} else {
			WARN("cannot unlink skb, no vcc in skb\n");
		}
#else
		skb_unlink(skb);
#endif
	}
}

static inline struct sk_buff *peek_tx_skb(struct unicorn_atmdrv *drv)
{
	struct sk_buff *skb;
	
	skb = skb_peek(&drv->rt_tx_q);
	if (!skb) {
		skb = skb_peek(&drv->tx_q);
	}
	return skb;
}

static inline struct sk_buff *dequeue_tx_skb(struct unicorn_atmdrv *drv)
{
	struct sk_buff *skb;

	skb = skb_dequeue(&drv->rt_tx_q);
	if (!skb) {
		skb = skb_dequeue(&drv->tx_q);
	}
	return skb;
}

static void purge_tx_q(struct unicorn_atmdrv *drv)
{
	struct sk_buff *skb;
	
	// free all buffers in transmit queues
	while ((skb = dequeue_tx_skb(drv)) != NULL) {
		tx_free_skb(skb);
	}
}

static int build_aal0_cell(unsigned char *cell,struct sk_buff *skb)
{
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

static int build_aal5_cell(unsigned char *cell,struct sk_buff *skb)
{
	struct atm_ext_skb_data *skb_data = ATM_EXT_SKB(skb);
	struct atm_vcc *vcc = skb_data->atm_skb_data.vcc;
	int payload_len;
	int pad_len;
	
	ASSERT(skb_data != NULL);
	ASSERT(vcc != NULL);

	// Add 5 byte ATM header
	cell[0] = (vcc->vpi >> 4) & 0xf;
	cell[1] = ((vcc->vpi & 0xf) << 4) | ((vcc->vci >> 12) & 0xf);
	cell[2] = vcc->vci >> 4;
	cell[3] = (vcc->vci & 0xf ) << 4;
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

static void snd_poll(struct unicorn_atmdrv *drv)
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
	max_cells = (((drv->atm_dev->link_rate - drv->max_cell_counter) * POLL_TIME) + (t/2))/t;
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
			finished = build_aal0_cell(cell,skb);
			break;
		case ATM_AAL5:
			finished = build_aal5_cell(cell,skb);
			break;
		default:
			WARN("AAL%d not supported\n",ATM_EXT_SKB(skb)->aal==ATM_AAL0 ? 0 : ATM_EXT_SKB(skb)->aal);
			finished=1;
			break;
		}

		if (finished) {
			// Finished with this buffer
			tx_unlink_skb(drv,skb);
			tx_free_skb(skb); 	
			// Get next buffer
			skb = peek_tx_skb(drv);
		}
	}

	if (cells) {
		ep->start_transmit(ep->dev);
		DBG(DATA_D,"%d/%d cells\n",cells,max_cells);
	}
}

static void oam_loopback(struct unicorn_atmdrv *drv,unsigned char *cell)
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
	skb_data->atm_skb_data.vcc = NULL;
	skb_data->aal = ATM_AAL0;
	skb_data->tx.aal0.pdu_length = skb->len;

	// put skb on transmit queue 
	skb_queue_tail(&drv->tx_q, skb);
}

static void rcv_oam(struct unicorn_atmdrv *drv,unsigned char *cell,int vpi,int vci)
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

static struct atm_vcc *find_vcc(struct unicorn_atmdrv *drv, int vpi, int vci)
{
    struct atm_vcc* vcc;
    int i;

    for (i=0; i < MAX_VC; i++) {
	    vcc = drv->vccs[i];
	    if (vcc) {
		    if ((vcc->vpi == vpi) && (vcc->vci == vci)) {
			    return vcc;
		    }
	    }
    }

    for (i=0; i < MAX_VC; i++) {
	    vcc = drv->vccs[i];
	    if (vcc) {
		    if ((vcc->vpi == ATM_VPI_UNSPEC)  && (vcc->vci == ATM_VCI_UNSPEC)) {
			    if (vcc->qos.aal == ATM_AAL0) {
				    // raw cell
				    return vcc;
			    }
		    }
	    }
    }
    return NULL;
}

static void rcv_aal0(struct unicorn_atmdrv *drv,struct atm_vcc *vcc,unsigned char *cell)
{
   	struct sk_buff *skb;

	DUMP_PACKET(DATA_D,cell,ATM_CELL_SIZE);

	skb = dev_alloc_skb(ATM_AAL0_SDU);
	if (skb) {
		memcpy(skb_put(skb,ATM_AAL0_SDU - ATM_CELL_PAYLOAD), cell,ATM_AAL0_SDU - ATM_CELL_PAYLOAD);
		memcpy(skb_put(skb,ATM_CELL_PAYLOAD), cell+5, ATM_CELL_PAYLOAD);
		
		DBG(DATA_D,"vpi=%d,vci=%d\n",vcc->vpi,vcc->vci);
		DUMP_PACKET(DATA_D,skb->data,skb->len);

		if (atm_charge(vcc, skb->truesize)) {
			atomic_inc(&vcc->stats->rx);
			vcc->push(vcc, skb);
		} else {
			WARN("dropping incoming packet,size=%d\n",skb->truesize);
			atomic_inc(&vcc->stats->rx_drop);
			dev_kfree_skb_any(skb);
		}
	} else {
		WARN("dev_alloc_skb failed\n");
		atomic_inc(&vcc->stats->rx_err);
	}
}

static struct sk_buff *rawcell_decode(struct atm_vcc *vcc, unsigned char *cell)
{
   	struct sk_buff *skb;
	unsigned int max_mru = vcc->qos.rxtp.max_sdu+ATM_CELL_PAYLOAD; // the AAL5 padding may take up to 1 cell
	
	DUMP_PACKET(DATA_D,cell,ATM_CELL_SIZE);

	// get reassembly buffer
	skb = vcc->dev_data;
	if (!skb) {
		skb = dev_alloc_skb(max_mru);
		if (!skb) {
			WARN("dev_alloc_skb failed\n");
			return NULL;
		}
		vcc->dev_data = skb;
	}
    
	// copy data
        if (skb->len > (max_mru-ATM_CELL_PAYLOAD)) {
		WARN("packet too long, skb->len=%d,max_mru=%d\n",skb->len,max_mru);
		skb_trim(skb, 0);
	}
	memcpy(skb_put(skb,ATM_CELL_PAYLOAD), cell+5, ATM_CELL_PAYLOAD);

  	// check for end of buffer
  	if (cell[3] & 0x2) {
		// the aal5 buffer ends here, cut the buffer. 		
		vcc->dev_data = NULL;
	} else {
		vcc->dev_data = skb;
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
		return NULL;
	}

	length      = (skb->tail[-6] << 8) + skb->tail[-5];
	pdu_crc     = (skb->tail[-4] << 24) + (skb->tail[-3] << 16) + (skb->tail[-2] << 8) + skb->tail[-1];
	pdu_length  = ((length + 47 + 8)/ 48) * 48;

  	DBG(DATA_D,"skb->len=%d,length=%d,pdu_crc=0x%lx,pdu_length=%d\n",skb->len,length,pdu_crc,pdu_length);
  
  	// is skb long enough ? 
  	if (skb->len < pdu_length) {
		WARN("skb too short,skb->len=%d,pdu_length=%d\n",skb->len,pdu_length);
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

static void rcv_aal5(struct unicorn_atmdrv *drv,struct atm_vcc *vcc,unsigned char *cell)
{
	struct sk_buff *skb;

	skb = rawcell_decode(vcc,cell);
	if (skb) {
		skb = aal5_decode(skb);
		if (skb) {
			DBG(DATA_D,"vpi=%d,vci=%d\n",vcc->vpi,vcc->vci);
			DUMP_PACKET(DATA_D,skb->data,skb->len);
			if (atm_charge(vcc, skb->truesize)) {
				atomic_inc(&vcc->stats->rx);
				vcc->push(vcc, skb);
			} else {
				WARN("dropping incoming packet,size=%d\n",skb->truesize);
				atomic_inc(&vcc->stats->rx_drop);
				dev_kfree_skb_any(skb);
			}
		} else {
			atomic_inc(&vcc->stats->rx_err);
		}
	}
}


static void rcv_poll(struct unicorn_atmdrv *drv)
{
	struct unicorn_entrypoints *ep = drv->ep;
	unsigned char *cell;
	struct atm_vcc *vcc;
	
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
		} else if ((vcc = find_vcc(drv,vpi,vci)) != NULL) {
			++drv->ds_cell_counter;

			switch (vcc->qos.aal) {
			case ATM_AAL0:
				rcv_aal0(drv,vcc,cell);
				break;
			case ATM_AAL5:
				rcv_aal5(drv,vcc,cell);
				break;
			default:
				WARN("AAL%d not supported\n",
				     vcc->qos.aal==ATM_AAL0 ? 0 : vcc->qos.aal);
				atomic_inc(&vcc->stats->rx_err);
				break;
			}
		} else {
			DBG(ATM_D,"wrong VPI.VCI %d.%d\n",vpi,vci);
		}
	}
}

static void unicorn_poll_data(unsigned long context)
{
	struct unicorn_atmdrv *drv = (struct unicorn_atmdrv *)context;
	ADSL_STATUS prev_adsl_status;	

	if (!drv->ep) return;
	if (!drv->atm_dev) return;

	prev_adsl_status = drv->adsl_status;
	drv->adsl_status = drv->ep->get_adsl_status(drv->ep->dev);

	if (drv->adsl_status != prev_adsl_status) {
		// ADSL state has changed...
		if (drv->adsl_status == ADSL_STATUS_ATMREADY) {
			drv->atm_dev->signal = ATM_PHY_SIG_FOUND;				
			// get the new ADSL link rate
			drv->atm_dev->link_rate = get_link_rate(drv);
		} else {
			drv->atm_dev->signal = ATM_PHY_SIG_LOST;	
			drv->atm_dev->link_rate = 0;

			// free all buffers in transmit queues
			purge_tx_q(drv);
		}
	}

	if (drv->adsl_status == ADSL_STATUS_ATMREADY) {
		rcv_poll(drv);
		snd_poll(drv);
	}
#ifndef USE_HW_TIMER	
	drv->timer.expires = ((POLL_TIME*HZ)/1000) + jiffies;
	add_timer(&drv->timer);	
#endif
}


static int unicorn_atm_send(struct atm_vcc *vcc, struct sk_buff *skb) 
{
	struct unicorn_atmdrv *drv = (struct unicorn_atmdrv *) vcc->dev->dev_data;
	struct atm_ext_skb_data *skb_data;
 	int status;
 
	DBG(DATA_D,"vpi=%d,vci=%d\n",vcc->vpi,vcc->vci);
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

  	switch (vcc->qos.aal) {
	case ATM_AAL0:
		// save context
		skb_data = ATM_EXT_SKB(skb);
		skb_data->atm_skb_data.vcc = vcc;
		skb_data->atm_skb_data.vcc = vcc;
		skb_data->aal = ATM_AAL5 ;
		skb_data->tx.aal0.pdu_length = skb->len;
		break;
	case ATM_AAL5:
		// save context
		skb_data = ATM_EXT_SKB(skb);
		skb_data->atm_skb_data.vcc = vcc;
		skb_data->aal = ATM_AAL5 ;
		skb_data->tx.aal5.pdu_length = skb->len;
		skb_data->tx.aal5.crc = CRC32_INITIAL;
		break;
	default:
		WARN("AAL%d not supported\n",vcc->qos.aal==ATM_AAL0 ? 0 : vcc->qos.aal);    
		status = -EINVAL;
		goto fail;
	};

	// put skb on transmit queue according to traffic class
	if ((vcc->qos.txtp.traffic_class == ATM_CBR) || 
	    (vcc->qos.txtp.traffic_class ==  ATM_VBR)) {
		skb_queue_tail(&drv->rt_tx_q, skb);
	} else {
		skb_queue_tail(&drv->tx_q, skb);
	}
	atomic_inc(&vcc->stats->tx);
	return 0;

fail:
	DBG(ATM_D,"status=%d\n",status);
	tx_free_skb(skb);
	return status;
}

void start_poll_timer(struct unicorn_atmdrv *drv)
{
	drv->ms_counter = 0;
	drv->max_cell_counter = 0;

	ASSERT(drv->ep != NULL);

	// empty the receive buffer
	while ((drv->ep->rcv_getcell(drv->ep->dev)) != NULL) {
	}
	
	// start poll timer
#ifdef USE_HW_TIMER
	drv->hw_tick=0;
#else	
	// start send and receive poll timer
	init_timer(&drv->timer);
	
	drv->timer.data = (unsigned long)drv;
	drv->timer.function = unicorn_poll_data;
	drv->timer.expires = ((POLL_TIME*HZ)/1000) + jiffies;
	
	add_timer(&drv->timer);	 
#endif
}

static void stop_poll_timer(struct unicorn_atmdrv *drv)
{
#ifndef USE_HW_TIMER
	// stop timer
	del_timer(&drv->timer);
#endif
	// free all buffers in transmit queues
	purge_tx_q(drv);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
static int unicorn_atm_open(struct atm_vcc *vcc, short vpi, int vci) 
#else
static int unicorn_atm_open(struct atm_vcc *vcc) 
#endif
{
	struct atm_dev *atm_dev = vcc->dev;
	struct unicorn_atmdrv *drv = (struct unicorn_atmdrv *)atm_dev->dev_data;	
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))	
	vcc->vpi = vpi;
	vcc->vci = vci;
#endif

	DBG(ATM_D,"AAL%d,vpi=%d,vci=%d,traffic_class=%d\n",
	    vcc->qos.aal==ATM_AAL0 ? 0 : vcc->qos.aal,vcc->vpi,vcc->vci,
	    vcc->qos.txtp.traffic_class);

	if (!drv->ep) return -ENXIO;

	switch(vcc->qos.aal) {
	case ATM_AAL5:
		if ((vcc->vpi == ATM_VPI_UNSPEC) || (vcc->vci == ATM_VCI_UNSPEC)) {
			WARN("VCC unspecified not supported for AAL%d\n",vcc->qos.aal);
			return -EINVAL;
		}
	case ATM_AAL0:
		break;
	default:
		WARN("AAL%d not supported\n",vcc->qos.aal==ATM_AAL0 ? 0 : vcc->qos.aal);
		return -EINVAL;
	}

	vcc->dev_data = NULL;

	set_bit(ATM_VF_ADDR,    &vcc->flags);
	set_bit(ATM_VF_PARTIAL, &vcc->flags);
	set_bit(ATM_VF_READY,   &vcc->flags);
 
	// set MAC address,
	unicorn_set_mac(drv,mac_address);

	// initialize transmit queues
	skb_queue_head_init(&drv->rt_tx_q);
	skb_queue_head_init(&drv->tx_q);


	drv->adsl_status = drv->ep->get_adsl_status(drv->ep->dev);
	atm_dev->signal = ADSL_STATUS_ATMREADY ? ATM_PHY_SIG_FOUND : ATM_PHY_SIG_LOST;	
	// get the ADSL link rate
	atm_dev->link_rate = get_link_rate(drv);

	if (drv->num_vccs < MAX_VC) {
		int i;
		for (i=0; i < MAX_VC; i++) {
			if (!drv->vccs[i]) {
				drv->vccs[i] = vcc;
				break;
			}
		}
		if (++drv->num_vccs == 1) {
			// first VCC, start data poll timer
			start_poll_timer(drv);
		}
	} else {
		return -EINVAL;
	}
	return 0;
}

static void unicorn_atm_close(struct atm_vcc *vcc) 
{
	struct atm_dev *atm_dev = vcc->dev;
	struct unicorn_atmdrv *drv = (struct unicorn_atmdrv *)atm_dev->dev_data;
  
	DBG(ATM_D,"\n");

	// freeing resources
	if (vcc->dev_data) {
		dev_kfree_skb_any(vcc->dev_data);
		vcc->dev_data = NULL;
	}
	clear_bit(ATM_VF_PARTIAL, &vcc->flags);

	// freeing address 
	vcc->vpi = ATM_VPI_UNSPEC;
	vcc->vci = ATM_VCI_UNSPEC;
	clear_bit(ATM_VF_ADDR, &vcc->flags);

	if (drv->num_vccs > 0) {	
		int i;
		for (i=0; i < MAX_VC; i++) {
			if (drv->vccs[i] == vcc) {
				drv->vccs[i] = NULL;
				break;
			}
		}
		if (--drv->num_vccs == 0) {
			// last VCC, stop data poll timer
			stop_poll_timer(drv);
		}
	} else {
	}
	return;
}

static int oam_send(struct unicorn_atmdrv *drv,int type,int vpi,int vci)
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
	skb_data->atm_skb_data.vcc = NULL;
	skb_data->aal = ATM_AAL0;
	skb_data->tx.aal0.pdu_length = skb->len;

	// put skb on transmit queue 
	skb_queue_tail(&drv->tx_q, skb);
	return 0;	
}

static int net_control(struct unicorn_atmdrv *drv,T_MswCtrl *ctrl)
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

static int unicorn_atm_ioctl(struct atm_dev *atm_dev, unsigned int cmd, void *arg) 
{
	struct unicorn_atmdrv *drv = (struct unicorn_atmdrv *)atm_dev->dev_data;
	T_MswCtrl ctrl;
	void *user_buffer;
	int err = -ENOTTY;

	DBG(ATM_D,"cmd=%c%x\n",_IOC_TYPE(cmd),_IOC_NR(cmd));

	if (!drv->ep) return -ENXIO;

	switch (cmd) {
	case ATM_QUERYLOOP:
		err = put_user(ATM_LM_NONE, (int *) arg) ? -EFAULT : 0;
		break;
	case ATM_MSW_CTL:
		err = copy_from_user(&ctrl,arg,sizeof(T_MswCtrl));
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


extern int unicorn_timer(void)
{
#ifdef USE_HW_TIMER
       struct unicorn_atmdrv *drv = unicorn_atmdrv;
 
       if (drv && drv->atm_dev) {
	       if (drv->num_vccs) {
		       // 2ms periodic
		       if (++drv->hw_tick == (POLL_TIME/2)) {
			       drv->hw_tick = 0;
			       unicorn_poll_data((unsigned long)drv); 
		       }
	       }
       }
#endif
       return 0;
}

module_param_string(mac_address, mac_address, sizeof(mac_address), 0);
#if DEBUG
#ifdef ATM_DRIVER
module_param(DebugLevel, ulong, 0);
#endif
#endif

int unicorn_atm_init(void)
{
	struct unicorn_atmdrv *drv;

#ifdef ATM_DRIVER
	INFO("v %d.%d.%d, " __TIME__ " " __DATE__"\n",
	     (VERS>>8)&0xf,(VERS>>4)&0xf,VERS&0xf);
#endif  
	if (unicorn_atmdrv) {
		WARN("driver already opened\n");
		return -ENXIO;
	}
	drv = kmalloc(sizeof(struct unicorn_atmdrv), GFP_KERNEL);
	if (!drv) {
		WARN("no memory for drv data\n");
		return -ENOMEM;
	}
	memset(drv, 0, sizeof(struct unicorn_atmdrv));  
	drv->adsl_status = ADSL_STATUS_NOHARDWARE;
	drv->vpi = ATM_VPI_UNSPEC;
	drv->vci = ATM_VCI_UNSPEC;

	unicorn_atmdrv = drv;

	if (unicorn_atm_startdevice(drv, &atm_devops) == NULL) {
		return -ENXIO;
	}
	return 0;
}

void unicorn_atm_cleanup(void)
{
	struct unicorn_atmdrv *drv = unicorn_atmdrv;

	DBG(ATM_D,"\n");

	if (drv) {
		unicorn_atm_stopdevice(drv);
		unicorn_atmdrv = NULL;
		kfree(drv);
	}
}

#ifdef ATM_DRIVER
module_init(unicorn_atm_init);
module_exit(unicorn_atm_cleanup);
#endif

extern int unicorn_attach(struct unicorn_entrypoints *entrypoints)
{
	int status;
	struct unicorn_atmdrv *drv;

	DBG(ATM_D,"entrypoints=%p\n",entrypoints);
	
	if (!unicorn_atmdrv) {
#ifdef ATM_DRIVER
		return -ENXIO;
#else
		status = unicorn_atm_init();
		if (status) {
			return status;
		}
#endif
	}
	drv = unicorn_atmdrv;

	ASSERT(entrypoints->dev != NULL);
	ASSERT(entrypoints->snd_getcell != NULL);
	ASSERT(entrypoints->rcv_getcell != NULL);
	ASSERT(entrypoints->start_transmit != NULL);
	ASSERT(entrypoints->get_adsl_status != NULL);
	ASSERT(entrypoints->get_adsl_linkspeed != NULL);

	drv->ep = entrypoints;

	drv->adsl_status = ADSL_STATUS_NOLINK;

	return 0;
}

extern int unicorn_detach(void)
{
	struct unicorn_atmdrv *drv = unicorn_atmdrv;

	DBG(ATM_D,"\n");

	if (!drv) {
		return -ENXIO;
	}
	drv->adsl_status = ADSL_STATUS_NOHARDWARE;
	drv->ep = NULL;
#ifdef ATM_DRIVER
#else
	unicorn_atm_cleanup();
#endif
	return 0;
}
