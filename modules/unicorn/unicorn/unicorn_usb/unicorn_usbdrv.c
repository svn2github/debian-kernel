/*
  This driver supports the Unicorn ADSL chipset from STMicroelectronics.
  The chipset consists of the ADSL DMT transceiver ST70137 and  the ST70136 
  Analog Front End (AFE).
  This file contains the USB specific routines.
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
#include <linux/usb.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/atmdev.h>
#include <asm/byteorder.h>
#include "types.h"
#include "hal.h"
#include "hard.h"
#include "rapi.h"
#include "amu/amas.h"
#include "crc.h"
#include "unicorn.h"
#include "debug.h"


// Compatability stuff
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
typedef struct usb_iso_packet_descriptor iso_packet_descriptor_t;
#ifndef FILL_BULK_URB
#define FILL_BULK_URB usb_fill_bulk_urb
#endif
#ifndef FILL_INT_URB
#define FILL_INT_URB usb_fill_int_urb
#endif
#ifndef USB_ISO_ASAP
#define USB_ISO_ASAP URB_ISO_ASAP
#endif
#ifndef USB_ST_DATAUNDERRUN 
#define USB_ST_DATAUNDERRUN	(-EREMOTEIO)
#endif
#ifndef USB_ST_BANDWIDTH_ERROR
#define USB_ST_BANDWIDTH_ERROR	(-ENOSPC)			/* too much bandwidth used */
#endif
#define ALLOC_URB(iso_pkts) usb_alloc_urb(iso_pkts,GFP_ATOMIC)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,20))
typedef struct iso_packet_descriptor iso_packet_descriptor_t;
#define ALLOC_URB(iso_pkts) usb_alloc_urb(iso_pkts)
#else
#define ALLOC_URB(iso_pkts) usb_alloc_urb(iso_pkts)
#endif


MODULE_AUTHOR ("fisaksen@bewan.com");
MODULE_DESCRIPTION ("ATM driver for the ST UNICORN ADSL modem.");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Proprietary");
#endif


// Use alternative setting 1 on interface 0 to use ISOC transfers and optimal speed
#define ALT_SETTING 1
// Use alternative setting 4 on interface 0 to use ISOC transfers and least USB bandwidth used
// #define ALT_SETTING 4

#define RETRY_UNDERRUN 1

/*
 * Submit an URB with error reporting
 */
static int SUBMIT_URB(struct urb* urb) 
{ 
	int status; 
        if (urb && !GlobalRemove) { 
           DBG(USB_D,"usb_submit_urb,urb=%p,length=%d\n",urb,urb->transfer_buffer_length); 
           urb->hcpriv = 0; 
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
           status = usb_submit_urb(urb,GFP_ATOMIC); 
#else
           status = usb_submit_urb(urb); 
#endif
	   if (status  < 0) { 
		WARN("usb_submit_urb failed,status=%d\n", status); 
                GlobalRemove = TRUE; 
	   } 
        } else { 
	   WARN("no urb or GlobalRemove\n"); 
           status = -1; 
	} 
        return status;
}

/*
 * Unlink an URB with error reporting. This is a macro so
 * the __FUNCTION__ returns the caller function name.
 */
#define UNLINK_URB(urb) \
({ \
	int status; \
        if (urb) { \
           DBG(USB_D,"usb_unlink_urb,urb=%p,actual_length=%d\n",urb,urb->actual_length); \
	   if ((status = usb_unlink_urb(urb)) < 0) { \
		DBG(1,"usb_unlink_urb failed,status=%d\n", status); \
           } \
        } else { \
	   WARN("no urb\n"); \
           status = -1; \
	} \
        status; \
})

#if DEBUG
static void dump_urb(struct urb *urb)
{
	int i;

	printk(KERN_DEBUG "hcpriv=%p,dev=%p,pipe=%x,status=%d,transfer_flags=%d\n",
	       urb->hcpriv,urb->dev,urb->pipe,urb->status,urb->transfer_flags);
	printk(KERN_DEBUG "bandwidth=%d,start_frame=%u,interval=%d,error_count=%d\n",
	    urb->bandwidth,urb->start_frame,urb->interval,urb->error_count);
	// buffers
	printk(KERN_DEBUG "transfer_buffer=%p,transfer_buffer_length=%d,actual_length=%d\n",
	       urb->transfer_buffer,urb->transfer_buffer_length,urb->actual_length);
	for (i=0; i < urb->number_of_packets; i++) {
		iso_packet_descriptor_t *frame = &urb->iso_frame_desc[i]; 
		printk(KERN_DEBUG "[%d],status=%d,actual_length=%d,length=%d,offset=%d\n",
		    i,frame->status,frame->actual_length,frame->length,frame->offset);

	}
}
#endif

//#define DUMP_URB(urb) dump_urb(urb)
#define DUMP_URB(urb)

static void fill_isoc_urb(struct urb *urb, struct usb_device *dev,
	      unsigned int pipe, void *buf, int length, int packet_size, usb_complete_t complete,
	      void *context)
{
	spin_lock_init(&urb->lock);
	urb->dev=dev;
	urb->pipe=pipe;
	urb->transfer_buffer=buf;
	urb->transfer_buffer_length=length;
	urb->transfer_flags=USB_ISO_ASAP;
	urb->start_frame = -1;
	urb->interval = 1;
	urb->complete=complete;
	urb->context=context;
	{
		iso_packet_descriptor_t *frame = urb->iso_frame_desc; 
		int offset = 0;
		int num_packets=0;

		while (length) {
			frame->offset = offset;
			frame->length = MIN(packet_size,length);
			frame->actual_length = 0;
			offset += frame->length;
			length -= frame->length;
			num_packets++;
			frame++;
		}
		urb->number_of_packets = num_packets;
	}
}

//----------------------------------------------------------------------
// Macros to read/write the little endian USB bus
//----------------------------------------------------------------------
#ifdef __BIG_ENDIAN__
static inline void USB_SWAP_BUF(WORD *buf,WORD size)
{
	int i;
	for (i=0; i < size; i++) 
		buf[i] = cpu_to_le16(buf[i]);
}
#else
#define USB_SWAP_BUF(buf,size)
#endif

//----------------------------------------------------------------------
// driver parameters
//----------------------------------------------------------------------
#if DEBUG
unsigned long DebugLevel=0;
#endif
int FrameNumber=0;


//----------------------------------------------------------------------
// MSW paramters
//----------------------------------------------------------------------
//unsigned long ActivationMode = MSW_MODE_MULTI;
unsigned long ActivationMode = MSW_MODE_ANSI;

extern unsigned long ActTimeout;
extern unsigned long DownstreamRate;
unsigned long DownstreamRate=3400;	// In Kbits/sec	
extern unsigned long eocTrace;
extern unsigned long ExchangeDelay;
extern unsigned long FmPollingRate;
extern unsigned long g_RefGain;
extern unsigned short g_TeqMode;
extern unsigned long InitTimeout;
extern unsigned long Interoperability;
extern unsigned long LCD_Trig;
extern unsigned long LOS_LOF_Trig;
extern unsigned long RetryTime;
extern unsigned long TrainingDelay;
//extern unsigned long TruncateMode;
extern unsigned long useAFE;
extern unsigned long pilotRealloc;
extern unsigned long _newCToneDetection_;
extern unsigned long useRFC019v;
extern unsigned long useRFC029v;
//extern unsigned long useRFC033v;
extern unsigned long useRFC040v;
extern unsigned long useRFC041v;
extern unsigned long useRFCFixedRate;
extern unsigned long useVCXO;
extern unsigned long _no_TS652;
extern unsigned long txPower;
extern unsigned long pollingEoc;

extern unsigned long eocTrace;
extern unsigned long useAOCChannel;
extern unsigned long setINITIALDAC;

extern unsigned long pilotRootPowerWorkAround;
extern unsigned long _gi_step_;
extern unsigned long _teq_new_delay_;
extern unsigned long TNumberOfCarrier;
extern unsigned long highCarrierOff;
extern unsigned long decreaseHighCarrier;
extern unsigned long _boostPowerGdmt_;
extern unsigned long g_ModemState;
extern unsigned long ep2Enable;
extern unsigned long ep3Enable;

unsigned long AutoActivation=1;
unsigned long LoopbackMode=0;
unsigned long MswDebugLevel=2;
unsigned long last_report = 0L;
AMSW_ModemFailure last_failure = C_AMSW_NO_HARDWARE;
AMSW_ModemEvent last_event = (AMSW_ModemEvent)-1L;
DWORD adsl_system_time = 0L;
ADSL_STATUS adsl_status = ADSL_STATUS_NOHARDWARE;
unsigned short adsl_us_cellrate = 0;
unsigned short adsl_ds_cellrate = 0;

unsigned long GlobalRemove=0;
int gLedXmit = 0;		// Transmit data indicator
int gLedRecv = 0;		// Receive data indicator
int gAtmUsbError=0;


//ULONG use2Biq = 0;	// Relay variable to delay the initilization of BiqCorr



//----------------------------------------------------------------------
// Exported functions
//----------------------------------------------------------------------
unsigned char *unicorn_snd_getcell(struct unicorn_dev *dev);
int unicorn_start_transmit(struct unicorn_dev *dev);
unsigned char *unicorn_rcv_getcell(struct unicorn_dev *dev);
int unicorn_msw_control(struct unicorn_dev *dev,T_MswCtrl *ctrl);
ADSL_STATUS unicorn_get_adsl_status(struct unicorn_dev *dev);
int unicorn_get_adsl_linkspeed(struct unicorn_dev *dev,
			       unsigned long *us_rate,unsigned long *ds_rate);


//----------------------------------------------------------------------
// R/W objects
//----------------------------------------------------------------------
#define	CELL_LENGTH             USB_CELL_LENGTH

struct send_atm {
	BOOLEAN started;
	int turn_write;				// Current Write buffer
	int turn_send;				// Current sending buffer
	unsigned char *bufs[ATM_WRITES];
	int lens[ATM_WRITES];
	unsigned long busy[ATM_WRITES];
	int maxlen;
	int bufofs;
	unsigned long long cell_count;
	spinlock_t lock;	
};

struct recv_atm {
	BOOLEAN started;
	unsigned char turn_read;
	unsigned char turn_recv;
	unsigned char *cells[256];
	int maxlen;
	int pipe_index;
	int num_reads;
	unsigned long long cell_count;
	spinlock_t lock;	
};

#define OBC_READ_CMD 	0x01	// Set if Read OBC command
#define OBC_CMD_INT_LO 	0x02	// Set after the USB INT_LO interrupt
#define OBC_CMD_INT 	0x04	// Set after the USB OBC interrupt
#define OBC_WRITE_CPLT 	0x08	// Set after the OBC write completion
#define OBC_READ_CPLT 	0x10	// Set after the OBC read IRP completion

#define OBC_LOCK(sem) down(sem)
#if 0
#define OBC_LOCK(sem) \
if (down_trylock(sem)) { \
   DBG(1,"down_trylock failed\n"); \
   rapi_unlock(); \
   down(sem); \
   rapi_lock(); \
} 
#endif
#define OBC_UNLOCK(sem) up(sem)

struct unicorn_dev {	
	struct usb_device 	*usb_dev;
	int                     alternate_setting;
	struct send_atm		send_atm;		// US ATM object for transmission
	struct recv_atm		recv_atm;		// DS ATM object for transmission
	struct urb  		*int_in_pipe[2];	// Endpoint 1	
       	struct urb	        *obc_iso_out;		// Endpoint 2	
	struct urb		*obc_iso_in;		// Endpoint 3	
	struct urb	        *atm_write[ATM_WRITES];	// ATM US Transfer (EP 4)
	struct urb		*atm_read[ATM_READS];	// ATM DS transfer (EP 5)
	struct urb		*obc_int_out;		// Endpoint 6	
	struct urb		*obc_int_in;		// Endpoint 7	

	USB_MEMORY		*usb_mem;	       // Pointer to the USB buffers
	volatile PBYTE		dma_virtual_addr;      // DMA buffer virtual address
	DWORD                   obc_sem;               // To wait for previous OBC command
	unsigned long		obc_flags;	       // OBC Flags
	struct semaphore	obc_lock;
	BOOLEAN			started;
	BOOLEAN			msw_started;		// True if the MSW is (manually) started
};

struct unicorn_dev unicorn_usb_dev;

struct unicorn_entrypoints unicorn_usb_entrypoints = {
	&unicorn_usb_dev,
	unicorn_snd_getcell,
	unicorn_rcv_getcell,
	unicorn_start_transmit,
	unicorn_msw_control,
	unicorn_get_adsl_status,
	unicorn_get_adsl_linkspeed
};
	
//----------------------------------------------------------------------
// atm_send_complete:
//----------------------------------------------------------------------
static void atm_send_complete(struct unicorn_dev *dev)
{
	struct send_atm *send_atm = &dev->send_atm;
	int t;

	DBG(RW_D,"\n");

	t = send_atm->turn_send;
	if (test_and_clear_bit(0,&send_atm->busy[t])) {
		send_atm->lens[t] = 0;
		t = (t+1)&(ATM_WRITES-1);
		send_atm->turn_send = t;
	} else {
		WARN("busy ??\n");
	}
}
	
//----------------------------------------------------------------------
//	ATM US transfer complete
//----------------------------------------------------------------------
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
static void AtmUsXferComplete(struct urb *urb,struct pt_regs *pt_regs)
#else
static void AtmUsXferComplete(struct urb *urb)
#endif
{
	struct unicorn_dev *dev = (struct unicorn_dev *)urb->context;
		
	DBG(RW_D,"status=%d,transfer_buffer_length=%d,actual_length=%d\n",
	    urb->status,urb->transfer_buffer_length,urb->actual_length);	

	if (urb->status == 0) {
		atm_send_complete(dev);
	} else {
		WARN("status=%d,transfer_buffer_length=%d,actual_length=%d\n",
		     urb->status,urb->transfer_buffer_length,urb->actual_length);	
		atm_send_complete(dev);
	}
}

//----------------------------------------------------------------------
//	Start the ATM upstream DMA
//----------------------------------------------------------------------
static void StartAtmUsXfer(struct unicorn_dev *dev,int turn,unsigned char *buffer,int length)
{
	struct urb *urb = dev->atm_write[turn];
	
	DBG(RW_D,"(%d,%d)\n",turn,length);

	if (GlobalRemove) return;

	if (dev->alternate_setting != 2) {
		// Fill the isochronous URB
		fill_isoc_urb(urb, dev->usb_dev, usb_sndisocpipe(dev->usb_dev,EP_ATM_ISO_OUT),
			      buffer, length,  
			      usb_maxpacket(dev->usb_dev,
					    usb_sndisocpipe(dev->usb_dev,EP_ATM_ISO_OUT),1), 
			      AtmUsXferComplete, dev);
	} else {
		// Fill the bulk URB
		FILL_BULK_URB(urb, dev->usb_dev, usb_sndbulkpipe(dev->usb_dev,EP_ATM_ISO_OUT),
			      buffer, length, 
			      AtmUsXferComplete, dev);
	}
	SUBMIT_URB(urb);
}

//----------------------------------------------------------------------
//	ATM DS transfer complete
//----------------------------------------------------------------------
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
static void AtmDsXferComplete(struct urb *urb,struct pt_regs *pt_regs)
#else
static void AtmDsXferComplete(struct urb *urb)
#endif
{
	struct unicorn_dev *dev = (struct unicorn_dev *)urb->context;
	struct recv_atm *recv_atm = &dev->recv_atm;
	int cells;
	unsigned char *cell;

	DBG(INTR_D,"status=%d,transfer_buffer_length=%d,actual_length=%d\n",
	    urb->status,urb->transfer_buffer_length,urb->actual_length);	

	if (GlobalRemove) return;
	if (!recv_atm->started) return;

	if ((urb->status == USB_ST_DATAUNDERRUN) && (urb->actual_length > 0)) urb->status = 0;
	if (urb->status) {
		WARN("urb->status=%d,transfer_buffer_length=%d,actual_length=%d\n",
		     urb->status,urb->transfer_buffer_length,urb->actual_length);	
		// try again...
	}

	if (urb->actual_length > 0) {
		if (usb_pipeisoc(urb->pipe)) {
			int i;
			// ISOC
			for (i=0; i < urb->number_of_packets; i++) {
				iso_packet_descriptor_t *frame = &urb->iso_frame_desc[i];
				cells = frame->actual_length/USB_CELL_LENGTH;
				cell = (unsigned char *)urb->transfer_buffer+frame->offset;
				while (cells--) {
					DBG(RW_D,"turn_recv=%d,cell=%p\n",recv_atm->turn_recv,cell);
					
					recv_atm->cells[recv_atm->turn_recv++] = cell;
					cell += USB_CELL_LENGTH;
				}
			}
		} else {
			// BULK
			cells = urb->actual_length/USB_CELL_LENGTH;
			cell = (unsigned char *)urb->transfer_buffer;
			while (cells--) {
				DBG(RW_D,"turn_recv=%d,cell=%p\n",recv_atm->turn_recv,cell);
				// check CRC
				if (!hecCheck(cell)) {
					WARN("HEC error\n");
					gAtmUsbError=1;
				}
				recv_atm->cells[recv_atm->turn_recv++] = cell;
				cell += USB_CELL_LENGTH;
			}
		}
	}
	urb->dev = dev->usb_dev;
	urb->transfer_flags = USB_ISO_ASAP;
	SUBMIT_URB(urb);
}

//----------------------------------------------------------------------
// atm_stop_rcv:
//----------------------------------------------------------------------
static void atm_stop_rcv(struct unicorn_dev *dev)
{
	struct recv_atm *recv_atm = &dev->recv_atm;
	int i;
	
	DBG(1,"started=%d\n",recv_atm->started);
	
	if (!recv_atm->started) return;
	for (i=0; i < recv_atm->num_reads; i++) {
		struct urb *urb = dev->atm_read[i];
		UNLINK_URB(urb);
	}
	recv_atm->started = FALSE;
}


//----------------------------------------------------------------------
// atm_start_rcv
//----------------------------------------------------------------------
static void atm_start_rcv(struct unicorn_dev *dev)
{
	struct recv_atm *recv_atm = &dev->recv_atm;
	int i;
	unsigned char *buffer;
	int size;
	int packet_size;

	if (recv_atm->started) return;
	recv_atm->turn_recv = 0;
	recv_atm->turn_read = 0;
	recv_atm->pipe_index = 0;

   	// Initialize the R/W
	buffer = (unsigned char *)dev->usb_mem->AtmDsBuf;
	if (dev->alternate_setting != 2) {
		// ISOC
		size = ATM_DS_CELLS_PER_PKT*USB_CELL_LENGTH*ATM_DS_ISO_PACKETS;
		recv_atm->num_reads = ATM_READS;
		packet_size = usb_maxpacket(dev->usb_dev,usb_rcvisocpipe(dev->usb_dev,EP_ATM_ISO_IN),0);
	} else {
		// BULK
		size = ATM_DS_CELLS_PER_PKT*USB_CELL_LENGTH*1;
		recv_atm->num_reads = ATM_READS;
		packet_size = 0;
	}
	recv_atm->maxlen = size;
	recv_atm->cell_count = 0;

	DBG(1,"buffer=%p,size=%d,packet_size=%d,num_reads=%d\n",
	    buffer,size,packet_size,recv_atm->num_reads);

	for (i=0; i < recv_atm->num_reads; i++) {
		struct urb *urb = dev->atm_read[i];		
		
		if (dev->alternate_setting != 2) {
			// Fill the isochronous URB
			fill_isoc_urb(urb, dev->usb_dev, usb_rcvisocpipe(dev->usb_dev,EP_ATM_ISO_IN),
				      buffer+(i*size), recv_atm->maxlen, packet_size, 
				      AtmDsXferComplete, dev);
			SUBMIT_URB(dev->atm_read[i]);
		} else {
			// Fill the bulk URB
			FILL_BULK_URB(urb, dev->usb_dev, usb_rcvbulkpipe(dev->usb_dev,EP_ATM_ISO_IN),
				      buffer+(i*size), recv_atm->maxlen, 
				      AtmDsXferComplete, dev);
			SUBMIT_URB(dev->atm_read[i]);
		}
	}
	recv_atm->started = TRUE;
}

//----------------------------------------------------------------------
// atm_stop_snd
//----------------------------------------------------------------------
static void atm_stop_snd(struct unicorn_dev *dev)
{
	struct send_atm *send_atm = &dev->send_atm;
	int i;

	DBG(1,"started=%d\n",send_atm->started);
	
	if (!send_atm->started) return;
	send_atm->started = FALSE;
	send_atm->turn_send = 0;
	send_atm->turn_write = 0;

	for (i=0; i < ATM_WRITES; i++) {
		send_atm->lens[i] = 0;
	}
}

//----------------------------------------------------------------------
// atm_start_snd
//----------------------------------------------------------------------
static void atm_start_snd(struct unicorn_dev *dev)
{
	struct send_atm *send_atm = &dev->send_atm;
	int i;
	unsigned char *buffer;
	int size;

	DBG(1,"started=%d\n",send_atm->started);

	if (send_atm->started) return;
	send_atm->bufofs = 0;
	send_atm->turn_send = 0;
	send_atm->turn_write = 0;
	
   	// Initialize the R/W
	buffer = (unsigned char *)dev->usb_mem->AtmUsBuf;
	send_atm->maxlen = size = ATM_US_CELLS_PER_PKT*USB_CELL_LENGTH*ATM_US_ISO_PACKETS;
	for (i=0; i < ATM_WRITES; i++) {
		send_atm->bufs[i] = buffer+i*size;
		send_atm->lens[i] = 0;
		send_atm->busy[i] = 0;
	}
	send_atm->cell_count = 0;
	send_atm->started = TRUE;
}


//======================================================================
//	REPORTS ADSL status from the MSW
//======================================================================

//----------------------------------------------------------------------
//	Modem Software Event Report for user-mode application
//----------------------------------------------------------------------
static const char *get_msw_event_string(AMSW_ModemEvent event)
{
	static char s[8];
	
	switch(event) {
	case C_AMSW_PEER_ATU_FOUND: return "PEER ATU FOUND";
	case C_AMSW_RESTART_REQUEST: return "RESTART REQUES";
	case C_AMSW_ACTIVATION_REQUEST: return "ACTIVATION REQUEST";       
	case C_AMSW_TO_INITIALIZING: return "TO INITIALIZING";          
	case C_AMSW_SHOWTIME: return "AMSW SHOWTIME";                 
	case C_AMSW_L3_EXECUTED: return "L3 EXECUTED";              
	case C_AMSW_L3_REJECTED: return "L3 REJECTED";              
	case C_AMSW_L1_EXECUTED: return "L1 REJECTED";                 
	case C_AMSW_L1_REJECTED: return "L1 REJECTED";                
	case C_AMSW_L0_REJECTED: return "L0 REJECTED";                
	case C_AMSW_RESTART_ACCEPTABLE: return "RESTART ACCEPTABLE";         
	case C_AMSW_SUICIDE_REQUEST: return "SUICIDE REQUEST";            
	case C_AMSW_RESTART_NOT_ACCEPTABLE: return "RESTART NOT ACCEPTABLE";     
	}
	sprintf(s,"(%d)",event);
	return s;
}

static const char *get_msw_failure_string(AMSW_ModemFailure failure)
{ 
	static char s[8];
	
	switch(failure) {
	case C_AMSW_UNCOMPATIBLE_LINECONDITIONS: return "UNCOMPATIBLE LINECONDITION";
	case C_AMSW_NO_LOCK_POSSIBLE: return "NO LOCK POSSIBLE";
	case C_AMSW_PROTOCOL_ERROR: return "PROTOCOL ERROR";
	case C_AMSW_MESSAGE_ERROR: return "MESSAGE ERROR";
	case C_AMSW_SPURIOUS_ATU_DETECTED: return "SPURIOUS ATU DETECTED";
	case C_AMSW_DS_REQ_BITRATE_TOO_HIGH_FOR_LITE: return "DS REQ BITRATE TOO HIGH FOR LITE";
	case C_AMSW_INTERLEAVED_PROFILE_REQUIRED_FOR_LITE: return "INTERLEAVED PROFILE REQUIRED FOR LITE";
	case C_AMSW_FORCED_SILENCE: return "FORCED SILENCE";
	case C_AMSW_UNSELECTABLE_OPERATION_MODE: return "UNSELECTABLE OPERATION MODE";
	case C_AMSW_STATE_REFUSED_BY_GOLDEN: return "STATE REFUSED BY GOLDEN";
	}
	sprintf(s,"(%d)",failure);
	return s;
}

static const char *get_msw_state_string(AMSW_ModemState state)
{
	static char s[8];
	
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
	}
	sprintf(s,"(%d)",state);
	return s;
}

void msw_report_event(DWORD type,DWORD code)
{
	DBG(RAPI_D,"type=%ld,code=%ld\n",type,code);

	last_report = (type << 16) | code;
	switch (type) {
	case MSW_EVENT_NONE:
		break;
	case MSW_EVENT_REPORT:
		INFO("MSW event: %s\n",get_msw_event_string(code));
		last_event = code;
		break;
	case MSW_EVENT_FAILURE:
		INFO("MSW failure: %s\n",get_msw_failure_string(code));
		last_failure = code;
		break;
	case MSW_EVENT_STATE:
		INFO("MSW state: %s\n",get_msw_state_string(code));
		break;
	case MSW_EVENT_CANCEL:
		break;
	case AMU_EVENT_ACT_TIMEOUT:
		INFO("AMU_EVENT_ACT_TIMEOUT\n");
		last_failure = C_AMSW_AMU_EVENT_ACT_TIMEOUT;
		break;
	case AMU_EVENT_INI_TIMEOUT:
		INFO("AMU_EVENT_INI_TIMEOUT\n");
		last_failure = C_AMSW_AMU_EVENT_INI_TIMEOUT;
		break;
	case AMU_EVENT_SHUTDOWN:
		INFO("AMU_EVENT_SHUTDOWN\n");
		last_failure = C_AMSW_AMU_EVENT_SHUTDOWN;
		break;
	case AMU_EVENT_RETRY:
		INFO("AMU_EVENT_RETRY\n");
		last_failure = C_AMSW_EVENT_RETRY;
		break;
	}
}

//----------------------------------------------------------------------
// Set the showtime FLAG
//----------------------------------------------------------------------
void setShowtime(void)
{
	struct unicorn_dev *dev = &unicorn_usb_dev;

	INFO("AdslStatus=%d\n",adsl_status);

	switch(adsl_status)
	{
	case ADSL_STATUS_NOHARDWARE:
	case ADSL_STATUS_ATMREADY:
		break;
	case ADSL_STATUS_NOLINK:
		adsl_system_time = xtm_gettime();
		adsl_status = ADSL_STATUS_ATMREADY;
		dev->recv_atm.cell_count = 0;
		dev->send_atm.cell_count = 0;
		atm_start_snd(dev);
		atm_start_rcv(dev);		
		break;
	}
}

//----------------------------------------------------------------------
// Reset the showtime FLAG
//----------------------------------------------------------------------
void resetShowtime(void)
{
	struct unicorn_dev *dev = &unicorn_usb_dev;

	INFO("AdslStatus=%d\n",adsl_status);

	switch(adsl_status)
	{
	case ADSL_STATUS_NOHARDWARE:
	case ADSL_STATUS_NOLINK:
		break;
	case ADSL_STATUS_ATMREADY:
		adsl_status = ADSL_STATUS_NOLINK;
		adsl_us_cellrate = 0;
		adsl_ds_cellrate = 0;
		atm_stop_snd(dev);
		atm_stop_rcv(dev);
		break;
	}
}

void HandleAtmError(void)
{
    if (gAtmUsbError) {
        gAtmUsbError = 0;
        WARN("Reset ATM FIFO\n");
        USB_controlWrite(UR_CFW,0xA2);
        USB_controlWrite(UR_CFW,0x22);
    }
}


// --------------------------------
// Led monitoring (FHLP 10/30/2001)
// --------------------------------
void HandleLeds(void)
{
	// called periodic frm AMUTask
	static WORD Leds = 0;
	static WORD LastLeds = 0;
	static int LedDelay = 0;

	// Led monitoring (FHLP 10/30/2001)
	// --------------------------------
	switch(g_ModemState)
		{
		default:
			LedDelay = 0;
			Leds = LED_POWER + LED_INIT;
			break;
			
		case C_AMSW_ACTIVATING:
		case C_AMSW_INITIALIZING:
		case C_AMSW_Ghs_HANDSHAKING:
		case C_AMSW_ANSI_HANDSHAKING:
			if (++LedDelay == 2)
				{
					LedDelay = 0;
					Leds = LED_POWER + LED_INIT;
				}
			else
				{
					Leds = LED_POWER;
			}
			break;
		
		case C_AMSW_SHOWTIME_L0:
		case C_AMSW_SHOWTIME_LQ:
		case C_AMSW_SHOWTIME_L1:
			Leds = LED_POWER;
			if (gLedXmit || gLedRecv)
				{
					gLedXmit = 0;
					gLedRecv = 0;
				Leds += (LastLeds & LED_SHOWTIME) ^ LED_SHOWTIME;
				}
			else Leds += LED_SHOWTIME;
			break;
		}
	if (Leds != LastLeds)
		{
			USB_controlWrite(UR_GPIO_DATA,Leds);
		}
	LastLeds = Leds;
}

//----------------------------------------------------------------------
// reports ATM cell rates
//----------------------------------------------------------------------
void setAtmRate(
	unsigned short upRate,
	unsigned short downRate
	)
{
	INFO("upRate=%dcells/s,downRate=%dcells/s\n",upRate,downRate);

	adsl_us_cellrate = upRate;
	adsl_ds_cellrate = downRate;
}

//----------------------------------------------------------------------
//	Copy the TOSCA hardware interrupt table applying a OR to the result
//	This function is called from the hardware ISR
//----------------------------------------------------------------------
static void CopyHardIntrTable(struct unicorn_dev *dev,WORD *IntBuf)
{
	int i;
	
	for (i=0; i<14; i++) {
		tosca_hardITABLE[i] |= cpu_to_le16(IntBuf[i+3]);
	}
}

//----------------------------------------------------------------------
//	OBC command interrupt received
//----------------------------------------------------------------------
static void ObcCmdCompletion(struct unicorn_dev *dev)
{	
	DBG(INTR_D,"obc_flags=%02lx\n",dev->obc_flags);
	
	if ((dev->obc_flags == (OBC_CMD_INT | OBC_CMD_INT_LO | OBC_WRITE_CPLT)) || 
	    (dev->obc_flags == (OBC_CMD_INT | OBC_CMD_INT_LO | OBC_WRITE_CPLT | OBC_READ_CMD | OBC_READ_CPLT))) {
		dev->obc_flags = 0;
		xsm_v(dev->obc_sem);
	}

}

//----------------------------------------------------------------------
//	Interrupt In pipe completion routine
//----------------------------------------------------------------------
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
static void IntInComplete(struct urb *urb,struct pt_regs *pt_regs)
#else
static void IntInComplete(struct urb *urb)
#endif
{
	struct unicorn_dev *dev = (struct unicorn_dev *)urb->context;
	WORD *IntBuf = urb->transfer_buffer;
	WORD isdr = cpu_to_le16(IntBuf[0]);
	WORD mask;
	WORD isr;
		
	DBG(INTR_D,"status=%d,actual_length=%d,ISDR=%04x\n",urb->status,urb->actual_length,isdr);		
	if (urb->status == 0) {
		// ok
	} else if (urb->status == USB_ST_DATAUNDERRUN) {
		// retry
		WARN("retry,status=%d\n",urb->status);
		goto retry;
	} else {
		// fatal error
		WARN("fatal error,status=%d\n",urb->status);
		GlobalRemove = TRUE;
		dev->started = FALSE;
		urb->dev = dev->usb_dev;
		urb->interval = 0;
		//UNLINK_URB(urb);
		return;
	}
	if ((urb->actual_length != 2) && (urb->actual_length != 6) && (urb->actual_length != 34)) {
		WARN("actual_length=%d\n",urb->actual_length);
		goto retry;
	}
		
	// ACTD interrupt
	if (isdr & UISDR_ACTDIF) {
		WARN("ACTD interrupt\n");
	}
	
	// INT_LO interrupt
	if (isdr & UISDR_INT_LO) {
		DBG(INTR_D,"INT_LO interrupt\n");
		dev->obc_flags |= OBC_CMD_INT_LO;
		ObcCmdCompletion(dev);
	}
	
	// Utopia FIFO interrupts
	if (isdr & UISDR_UTIRQ1) {
		DBG(INTR_D,"Utopia rising edge FIFO interrupt\n");
	}
	if (isdr & UISDR_UTIRQ2) {
		DBG(INTR_D,("Utopia falling edge FIFO interrupt\n"));
	}

	// Error interrupts
	if (isdr & UISDR_ERF) {
		if (isdr & UISDR_ERR_ATM) {
			WARN("ATM Operation Error interrupt\n");
			gAtmUsbError = 1;
		}
		if (isdr & UISDR_ERR_OBC) {
			WARN("WR OBC Operation Error interrupt\n");
		}
		if (isdr & UISDR_ERR_PIPE) {
			WARN("ERROR: WR OBC Access Error interrupt\n");
		}
	}
	
	// ADSL interrupts
	mask = UISDR_TIRQ1 | UISDR_TIRQ2;
	if (isdr & mask) {

		isr = cpu_to_le16(IntBuf[1]);
		DBG(INTR_D,"ADSL uP Interrupt,ISR = %04x\n",isr);

		// TOSCA macrocell interrupt
		mask = ISR_TOIFS;
		if (isr & mask) {
			if (urb->actual_length >= 34) {
				DBG(INTR_D,("TOSCA macrocell interrupt\n"));
				CopyHardIntrTable(dev,IntBuf);
				tosca_interrupt();
			} else {
				DBG(INTR_D,"TOSCA macrocell interrupt, too short, actual_length=%d\n",urb->actual_length);
			}
		}
		
		// Timer interrupt
		mask = ISR_TIMIF;
		if (isr & mask) {
			WARN("Timer interrupt\n");
		}
		
		// GPIO interrupt
		mask = ISR_GPIFA | ISR_GPIFB;
		if (isr & mask) {
			WARN("GPIO interrupt\n");
		}
		
		// OBC Slave Command Complete interrupt
		mask = ISR_OSIF;
		if (isr & mask){
			DBG(INTR_D,"OBC Slave Command Complete interrupt\n");
			dev->obc_flags |= (OBC_CMD_INT | OBC_CMD_INT_LO);
			ObcCmdCompletion(dev);
		}

		// OBC Master Command Complete interrupt
		mask = ISR_OMIF;
		if (isr & mask) {
			DBG(INTR_D,"OBC Master Command Complete interrupt\n");
			dev->obc_flags |= OBC_CMD_INT;
			ObcCmdCompletion(dev);
		}
	}
retry:	
	// Prepare URB for next transfer
	urb->dev = dev->usb_dev;
        urb->status = 0;
        urb->actual_length = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	SUBMIT_URB(urb);
#endif
	return;	
}

//=====================================================================
//	FUNCTIONS TO ACCESS THE HARDWARE
//======================================================================


//----------------------------------------------------------------------
//	Starts the hardware
//----------------------------------------------------------------------
static ST_STATUS start_hardware(struct unicorn_dev *dev)
{
	DBG(1,"\n");
	
	// Initialize the USB Configuration register
	if (USB_controlWrite(UR_CFW,0x22) == FAILURE) {
		WARN("USB_controlWrite(UR_CFW) failed\n");
		return FAILURE;
	}

	// Initialize the UR_IADR_IRQ register
	if (USB_controlWrite(UR_IADR_IRQ,0x100) == FAILURE) {
		WARN("USB_controlWrite(UR_IADR_IRQ) failed\n");
		return FAILURE;
	}
	
	// Initialize the ADSL uP Interrupt register
	if (USB_controlWrite(UR_ISR,
			     ISR_OSIE | ISR_OMIE | ISR_TOIES) == FAILURE) {
		WARN("USB_controlWrite(UR_ISR) failed\n");
		return FAILURE;
	}

	// Initialize the USB Interrupt & Data register
	if (USB_controlWrite(UR_ISDR,
			     UISDR_IE | UISDR_LOE | 
			     UISDR_TIE1 | UISDR_TIE2 | UISDR_UTIE1 | UISDR_UTIE2) == FAILURE) {
		WARN("USB_controlWrite(UR_ISDR) failed\n");
		return FAILURE;
	}
	
	// LED initialization
	USB_controlWrite(UR_GPIO_DIR,0xFFF1);	// Activate GPIO for leds
	USB_controlWrite(UR_GPIO_DATA,0x0E00);		// Leds are off
	
	return SUCCESS;
}

//----------------------------------------------------------------------
//	Stops the hardware
//----------------------------------------------------------------------
static void stop_hardware(struct unicorn_dev *dev)
{
	DBG(1,"\n");

	USB_controlWrite(UR_ISDR,0);
	USB_controlWrite(UR_ISR,0);
	USB_controlWrite(UR_CFW,0x1800);
	USB_controlWrite(UR_GPIO_DATA,0);		// Leds are off
}

//----------------------------------------------------------------------
//	Sets the MSW parameters to correct values
//----------------------------------------------------------------------
static void set_msw_params(void)
{
  if (!useAFE) useAFE=20174;
  if (useAFE == 70136) {
    setINITIALDAC = 93;
    g_RefGain = 38;
    txPower = 10;
  } else if (useAFE == 20174) {
    setINITIALDAC = 0;
    g_RefGain = 22;
    txPower = 14;
  } else {
    WARN("unknown AFE %ld\n",useAFE);
  }
  _no_TS652 = 1;
  useVCXO = 0;

  useAOCChannel = 0;
  pollingEoc = 1;
	
  pilotRootPowerWorkAround = TRUE;
  _gi_step_ = 1;
  _teq_new_delay_ = 1;
  //TNumberOfCarrier;
  highCarrierOff = 230;
  decreaseHighCarrier = 12;
  _boostPowerGdmt_ = 1;
  useRFC019v = 0;
  useRFC029v = 8000;
  useRFC040v = 0;
  useRFCFixedRate = 1;

  ep2Enable = 0;
  ep3Enable = 0;
  pilotRealloc=0;
  //_newCToneDetection_=1;

  //Interoperability = 1;

  INFO("MSW parameters: \nActivationMode=%lx\nActTimeout=%ld\nAutoActivation=%ld\nDebugLevel=%ld\nDownstreamRate=%ld\n",
       ActivationMode,ActTimeout,AutoActivation,DebugLevel,DownstreamRate);
  INFO("ExchangeDelay=%ld\nFmPollingRate=%ld\ng_RefGain=%ld\ng_Teqmode=%x\nInitTimeout=%ld\nInteroperability=%ld\n",
       ExchangeDelay,FmPollingRate,g_RefGain,g_TeqMode,InitTimeout,Interoperability);
  INFO("LCD_Trig=%ld\nLOS_LOF_Trig=%ld\nLoopbackMode=%ld\nMswDebugLevel=%ld\nRetryTime=%ld\nTrainingDelay=%ld\n",
       LCD_Trig,LOS_LOF_Trig,LoopbackMode,MswDebugLevel,RetryTime,TrainingDelay);
  
  INFO("useRFC019v=%ld\nuseRFC029v=%ld\nuseRFC040v=%ld\nuseRFC041v=%ld\nsetINITIALDAC=%ld\n",
       useRFC019v,useRFC029v,useRFC040v,useRFC041v,setINITIALDAC);
  
  INFO("useRFCFixedRate=%ld\nuseVCXO=%ld\n_no_TS652=%ld\nuseAFE=%ld\ntxPower=%ld\n",
       useRFCFixedRate,useVCXO,_no_TS652,useAFE,txPower);	
}

//----------------------------------------------------------------------
//	Start the USB device
//----------------------------------------------------------------------
static int start_device(struct unicorn_dev *dev,struct usb_device *usb_dev)
{
	int status;
	struct urb *urb;
	int i;
   	
	dev->usb_dev = usb_dev;

	// Select alternate setting to match speed/bandwidth used
	dev->alternate_setting = ALT_SETTING;

	if ((status = usb_set_interface (usb_dev, 0, dev->alternate_setting)) < 0) {
		if (status == USB_ST_BANDWIDTH_ERROR) {
			WARN("insufficient USB bandwidth\n");
			return status;
		} else {
			WARN("usb_set_interface (alt %d) failed,status=%d\n",
			     dev->alternate_setting,status);
			return status;
		}
	}
	
	//============================
	// Initialize the USB pipes...
	//============================

	// EP_INTERRUPT
	for (i=0; i < 2; i++) {
		if ((urb = ALLOC_URB(0)) == NULL) {
			WARN("usb_alloc_urb failed\n");
			return -ENOMEM;
		}
		dev->int_in_pipe[i] = urb;
	}

	// EP_OBC_ISO_OUT
	if ((urb = ALLOC_URB(MAX_ISO_PACKETS)) == NULL) {
		WARN("usb_alloc_urbfailed\n");
		return -ENOMEM;
	}
	dev->obc_iso_out = urb; 
	
	// EP_OBC_ISO_IN
	if ((urb = ALLOC_URB(MAX_ISO_PACKETS)) == NULL) {
		WARN("usb_alloc_urb failed\n");
		return -ENOMEM;
	}
	dev->obc_iso_in = urb; 
	   	   
	// EP_ATM_ISO_OUT
	for (i=0; i < ATM_WRITES; i++) {
		if ((urb = ALLOC_URB(MAX_ISO_PACKETS)) == NULL) {
			WARN("usb_alloc_urb failed\n");
			return -ENOMEM;
		}
		dev->atm_write[i] = urb; 
	}
 
	// EP_ATM_ISO_IN
	for (i=0; i < ATM_READS; i++) {
		if ((urb = ALLOC_URB(MAX_ISO_PACKETS)) == NULL) {
			WARN("usb_alloc_urb failed\n");
			return -ENOMEM;
		}
		dev->atm_read[i] = urb; 
	}
   	   
	// EP_OBC_INT_OUT
	if ((urb = ALLOC_URB(0)) == NULL) {
		WARN("usb_alloc_urb failed\n");
		return -ENOMEM;
	}
	dev->obc_int_out = urb; 

	// EP_OBC_INT_IN
	if ((urb = ALLOC_URB(0)) == NULL) {
		WARN("usb_alloc_urb failed\n");
		return -ENOMEM;
	}
	dev->obc_int_in = urb; 

	// Allocate memory
	if ((dev->usb_mem = kmalloc(sizeof(USB_MEMORY),GFP_DMA)) == NULL) {
		WARN("kmalloc failed\n");
		return -1;
	}

   	// Initialize OBC objects
	init_MUTEX(&dev->obc_lock);
   	if ((xsm_create("OBC ",0,0,&dev->obc_sem)) != SUCCESS) {
		return -1;
   	}
	
	for (i=0; i < 1; i++) {
		urb = dev->int_in_pipe[i];
		// Fill the interrupt URB ...
		FILL_INT_URB(urb, usb_dev,
			     usb_rcvintpipe(usb_dev, EP_INTERRUPT),
			     dev->usb_mem->IntBuf[i],sizeof(dev->usb_mem->IntBuf[i]),
			     IntInComplete,dev,1);
	
		// ... and start it
		SUBMIT_URB(urb);
	}

   	// Start the Hardware Device
   	start_hardware(dev);
   
   	dev->started = TRUE;
   	adsl_status = ADSL_STATUS_NOLINK;

   	return 0;
}

//----------------------------------------------------------------------
//	Stop the USB device
//----------------------------------------------------------------------
static void stop_device(struct unicorn_dev *dev)
{
	struct urb *urb;
	int i;

	DBG(1,"\n");

	adsl_status = ADSL_STATUS_NOHARDWARE;

	// Stop receiving and transmitting
	atm_stop_rcv(dev);
	atm_stop_snd(dev);

	// Shutdown the Modem Software
	if (dev->started) {
		if (dev->msw_started) {
		  rapi_lock();
		  msw_stop();
		  msw_exit();
		  rapi_unlock();
		  dev->msw_started = FALSE;
		}
   		dev->started = FALSE;
	}
	
	if (!GlobalRemove) {
		stop_hardware(dev);
	}

	//============================
	// Terminate the USB pipes...
	//============================

	// EP_INTERRUPT
	for (i=0; i < 2; i++) {
		if ((urb = dev->int_in_pipe[i])) {
			usb_unlink_urb(urb);
			usb_free_urb(urb);
			dev->int_in_pipe[i] = NULL;
		}
	}

	// EP_OBC_ISO_OUT
	if ((urb = dev->obc_iso_out)) {
		usb_unlink_urb(urb);
		usb_free_urb(urb);
		dev->obc_iso_out = NULL;
	}
	
	// EP_OBC_ISO_IN
	if ((urb = dev->obc_iso_in)) {
		usb_unlink_urb(urb);
		usb_free_urb(urb);
		dev->obc_iso_in = NULL;
	}
	   	   
	// EP_ATM_ISO_OUT
	for (i=0; i < ATM_WRITES; i++) {
		if ((urb = dev->atm_write[i])) {
			usb_unlink_urb(urb);
			usb_free_urb(urb);
			dev->atm_write[i] = NULL;
		}
	}
 
	// EP_ATM_ISO_IN
	for (i=0; i < ATM_READS; i++) {
		if ((urb = dev->atm_read[i])) {
			usb_unlink_urb(urb);
			usb_free_urb(urb);
			dev->atm_read[i] = NULL;
		}
	}
   	   
	// EP_OBC_INT_OUT
	if ((urb = dev->obc_int_out)) {
		usb_unlink_urb(urb);
		usb_free_urb(urb);
		dev->obc_int_out = NULL;
	}

	// EP_OBC_INT_IN
	if ((urb = dev->obc_int_in)) {
		usb_unlink_urb(urb);
		usb_free_urb(urb);
		dev->obc_int_in = NULL;
	}
	if (dev->usb_mem) {
	       kfree(dev->usb_mem);
	       dev->usb_mem = NULL;
	}

}

//----------------------------------------------------------------------
// CheckObcBuffer:
//----------------------------------------------------------------------
static BOOLEAN CheckObcBuffer(struct unicorn_dev *dev,WORD *buf,UINT n)
{
	PBYTE p = (PBYTE)buf + n*sizeof(WORD);
	if (GlobalRemove) return FALSE;
	if (p < dev->dma_virtual_addr) return FALSE;
	if (p > dev->dma_virtual_addr+offsetof(USB_MEMORY,AtmUsBuf)) return FALSE;
	return TRUE;
}

//----------------------------------------------------------------------
//	Waits for OBC command complete
//----------------------------------------------------------------------
static ST_STATUS WaitForObcCmdComplete(struct unicorn_dev *dev)
{
	// wait for semaphore to be free 
	if (xsm_p(dev->obc_sem, 0, OBC_CMD_TIMEOUT) != SUCCESS) {
		WARN("wait for obc failed (timed out),obc_flags=%02lx\n",dev->obc_flags);
		dev->obc_flags = 0;
		return FAILURE;
	}
	return SUCCESS;
}

//-----------------------------------------------------------------------------
// ObcWriteIsocComplete:
//-----------------------------------------------------------------------------
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
static void ObcWriteIsocComplete(struct urb *urb,struct pt_regs *pt_regs)
#else
static void ObcWriteIsocComplete(struct urb *urb)
#endif
{
	struct unicorn_dev *dev = (struct unicorn_dev *)urb->context;

	DBG(INTR_D,"status=%d,transfer_buffer_length=%d,actual_length=%d\n",
	    urb->status,urb->transfer_buffer_length,urb->actual_length);	

	if (urb->transfer_buffer_length == urb->actual_length) urb->status = 0;

	if (urb->status == 0) {
		dev->obc_flags |= OBC_WRITE_CPLT;
		ObcCmdCompletion(dev);
	} else {
		WARN("status=%d,transfer_buffer_length=%d,actual_length=%d\n",
		     urb->status,urb->transfer_buffer_length,urb->actual_length);
		//GlobalRemove = TRUE;
	}
}

//-----------------------------------------------------------------------------
// ObcWriteIntComplete:
//-----------------------------------------------------------------------------
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
static void ObcWriteIntComplete(struct urb *urb,struct pt_regs *pt_regs)
#else
static void ObcWriteIntComplete(struct urb *urb)
#endif
{
	struct unicorn_dev *dev = (struct unicorn_dev *)urb->context;

	DBG(INTR_D,"status=%d,transfer_buffer_length=%d,actual_length=%d\n",
	    urb->status,urb->transfer_buffer_length,urb->actual_length);	

	if (urb->transfer_buffer_length == urb->actual_length) urb->status = 0;
		
	if (urb->status == 0) {
		dev->obc_flags |= OBC_WRITE_CPLT;
		ObcCmdCompletion(dev);
	} else {
		WARN("status=%d,transfer_buffer_length=%d,actual_length=%d\n",
		     urb->status,urb->transfer_buffer_length,urb->actual_length);	
		//GlobalRemove = TRUE;
	}
	urb->interval = 0;  // set this to 0 to avoid to be re-scheduled
}

//-----------------------------------------------------------------------------
// ObcReadIsocComplete:
//-----------------------------------------------------------------------------
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
static void ObcReadIsocComplete(struct urb *urb,struct pt_regs *pt_regs)
#else
static void ObcReadIsocComplete(struct urb *urb)
#endif
{
	struct unicorn_dev *dev = (struct unicorn_dev *)urb->context;

	DBG(INTR_D,"status=%d,transfer_buffer_length=%d,actual_length=%d,start_frame=%u\n",
	    urb->status,urb->transfer_buffer_length,urb->actual_length,urb->start_frame);
	
	if (GlobalRemove) return;
	if ((urb->status == USB_ST_DATAUNDERRUN) && (urb->actual_length > 0)) urb->status = 0;
	if ((urb->status == 0) && (urb->actual_length == 0)) urb->status = USB_ST_DATAUNDERRUN;
	
	if (urb->status == 0) {
		dev->obc_flags |= OBC_READ_CPLT;
		ObcCmdCompletion(dev);
#if RETRY_UNDERRUN
	} else if (urb->status == USB_ST_DATAUNDERRUN) {
		WARN("retry,transfer_buffer_length=%d,actual_length=%d\n",
		     urb->transfer_buffer_length,urb->actual_length);	
		urb->dev = dev->usb_dev;
		if (FrameNumber) {
			urb->transfer_flags = 0;
			urb->start_frame = usb_get_current_frame_number(dev->usb_dev)+FrameNumber;
		} else {
			urb->transfer_flags = USB_ISO_ASAP;
		}
		SUBMIT_URB(urb);
#endif
	} else {
		WARN("status=%d,transfer_buffer_length=%d,actual_length=%d\n",
		     urb->status,urb->transfer_buffer_length,urb->actual_length);	
		//GlobalRemove = TRUE;
	}
}

//-----------------------------------------------------------------------------
// ObcReadIntComplete:
//-----------------------------------------------------------------------------
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19))
static void ObcReadIntComplete(struct urb *urb,struct pt_regs *pt_regs)
#else
static void ObcReadIntComplete(struct urb *urb)
#endif
{
	struct unicorn_dev *dev = (struct unicorn_dev *)urb->context;
		
	DBG(INTR_D,"status=%d,transfer_buffer_length=%d,actual_length=%d\n",
	    urb->status,urb->transfer_buffer_length,urb->actual_length);	

	if (GlobalRemove) return;
	if (urb->actual_length == urb->transfer_buffer_length) urb->status=0;
 
	if (urb->status == 0) {
		dev->obc_flags |= OBC_READ_CPLT;
		ObcCmdCompletion(dev);
	} else {
		WARN("status=%d,transfer_buffer_length=%d,actual_length=%d\n",
		     urb->status,urb->transfer_buffer_length,urb->actual_length);	
		//GlobalRemove = TRUE;
	}
	urb->interval = 0;  // set this to 0 to avoid to be re-scheduled
}

//-----------------------------------------------------------------------------
// USB_init:
//-----------------------------------------------------------------------------
ST_STATUS USB_init(
	WORD **CMDptrW1,WORD **CMDptrW2,WORD **CMDptrRd,
	WORD **CMDptrW_I1,WORD **CMDptrW_I2,WORD **CMDptrRd_I,
	WORD **ITABLEptr,WORD **IMASKptr,T_EpSettings *ep_setting
	)
{	
	struct unicorn_dev *dev = &unicorn_usb_dev;
	PBYTE p = (PBYTE)dev->usb_mem;
	
	DBG(1,"dev=%p\n",dev);

	DBG(USB_D,"dma_virtual_addr=%p\n",p);
	
	dev->dma_virtual_addr = p;
	*CMDptrW1   = (PWORD)(p + offsetof(USB_MEMORY,CmdBufW1));
	*CMDptrW2   = (PWORD)(p + offsetof(USB_MEMORY,CmdBufW2));
	*CMDptrRd   = (PWORD)(p + offsetof(USB_MEMORY,CmdBufRd));
	*CMDptrW_I1 = (PWORD)(p + offsetof(USB_MEMORY,CmdBufW_I1));
	*CMDptrW_I2 = (PWORD)(p + offsetof(USB_MEMORY,CmdBufW_I2));
	*CMDptrRd_I = (PWORD)(p + offsetof(USB_MEMORY,CmdBufRd_I));

	ep_setting->ep0_size = dev->usb_dev->descriptor.bMaxPacketSize0;
	ep_setting->ep1_size = usb_maxpacket(dev->usb_dev,usb_rcvintpipe(dev->usb_dev,EP_INTERRUPT),0);
	ep_setting->ep2_size = usb_maxpacket(dev->usb_dev,usb_sndisocpipe(dev->usb_dev,EP_OBC_ISO_OUT),1);
	ep_setting->ep3_size = usb_maxpacket(dev->usb_dev,usb_rcvisocpipe(dev->usb_dev,EP_OBC_ISO_IN),0);
	ep_setting->ep4_size = usb_maxpacket(dev->usb_dev,usb_sndisocpipe(dev->usb_dev,EP_ATM_ISO_OUT),1);
	ep_setting->ep5_size = usb_maxpacket(dev->usb_dev,usb_rcvisocpipe(dev->usb_dev,EP_ATM_ISO_IN),0);
	ep_setting->ep6_size = usb_maxpacket(dev->usb_dev,usb_sndintpipe(dev->usb_dev,EP_OBC_INT_OUT),1);
	ep_setting->ep7_size = usb_maxpacket(dev->usb_dev,usb_rcvintpipe(dev->usb_dev,EP_OBC_INT_IN),0);
	DBG(USB_D,"max packet size,ep0 %d,ep1 %d,ep2 %d,ep3 %d,ep4 %d,ep5 %d,ep6 %d,ep7 %d\n",
	    ep_setting->ep0_size,ep_setting->ep1_size,ep_setting->ep2_size,ep_setting->ep3_size,
	    ep_setting->ep4_size,ep_setting->ep5_size,ep_setting->ep6_size,ep_setting->ep7_size);

	*ITABLEptr = tosca_softITABLE;
	*IMASKptr  = tosca_softITABLE + 14;
	return SUCCESS;
}

//-----------------------------------------------------------------------------
// USB_controlRead:
//	Read an USB register
//-----------------------------------------------------------------------------
ST_STATUS USB_controlRead(BYTE addr,WORD *data)
{
	struct unicorn_dev *dev = &unicorn_usb_dev;
	int err;
	
	err = usb_control_msg(dev->usb_dev,usb_rcvctrlpipe(dev->usb_dev,0),
			      addr,USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			      0,0,data,sizeof(WORD),HZ);	
	if (err < 0) {
		goto fail;
	}		
	le16_to_cpus(data);
	DBG(USB_D,"addr=%02x,data=%04x\n",addr,*data);
	return SUCCESS;
	
fail:
	WARN("err=%d\n",err);
	return FAILURE;
}

//-----------------------------------------------------------------------------
// USB_controlWrite:
//	Write to an USB register
//-----------------------------------------------------------------------------
ST_STATUS USB_controlWrite(BYTE addr,WORD data)
{
	struct unicorn_dev *dev = &unicorn_usb_dev;
	int  err;

	DBG(USB_D,"addr=%02x,data=%04x\n",addr,data);
	
	err = usb_control_msg(dev->usb_dev,usb_sndctrlpipe(dev->usb_dev,0),
			      addr,0x40,
			      data,0,NULL,0,HZ);	
	if (err < 0) {
		goto fail;
	}	
	return SUCCESS;
	
fail:
	WARN("err=%d\n",err);
	return FAILURE;
}

//----------------------------------------------------------------------
//     ObcReset
//----------------------------------------------------------------------
static void ObcReset(void)
{
	// Force STOP ACCESS in status register and OBC FIFO clear pulse
	WORD data;
	
	if (GlobalRemove) return;

	WARN("Resetting OBC...\n");
	if(USB_controlRead(UR_STATUS,&data) == SUCCESS) {
		USB_controlWrite(UR_STATUS,(data&~3)|0x200);
	}
}

#define MAX_RETRIES 2

//-----------------------------------------------------------------------------
// USB_S_Write:
//-----------------------------------------------------------------------------
ST_STATUS USB_S_Write(T_ShortWrite *dataPtr,T_EpOut ep_out)
{
	struct unicorn_dev *dev = &unicorn_usb_dev;
	struct urb *urb;
	int retry;
	ST_STATUS status;
	int err;
	
	if (!CheckObcBuffer(dev,dataPtr->cmdBuff,dataPtr->frameSize)) return FAILURE;
	
	if ((ep_out != EP2) && (ep_out != EP6)) {
		WARN(("Invalid ep_out parameter\n"));
		return FAILURE;
	}

	OBC_LOCK(&dev->obc_lock);

	USB_SWAP_BUF(dataPtr->cmdBuff,dataPtr->frameSize);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		DBG(USB_D,"cmdBuff=%p,frameSize=%d,ep_out=%d\n",dataPtr->cmdBuff,dataPtr->frameSize,ep_out);

		dev->obc_flags = 0x00;
		
		if (ep_out == EP2) {
			// ISOC
			urb = dev->obc_iso_out;
			// Build ISOC descriptor
			fill_isoc_urb(urb, dev->usb_dev, usb_sndisocpipe(dev->usb_dev,EP_OBC_ISO_OUT),
				      dataPtr->cmdBuff, dataPtr->frameSize*sizeof(WORD),
				      usb_maxpacket(dev->usb_dev,
						    usb_sndisocpipe(dev->usb_dev,EP_OBC_ISO_OUT),1), 
				      ObcWriteIsocComplete, dev);
		} else {
			// Int
			urb = dev->obc_int_out;

			FILL_INT_URB(urb, dev->usb_dev, usb_sndintpipe(dev->usb_dev,EP_OBC_INT_OUT),
				      dataPtr->cmdBuff, dataPtr->frameSize*sizeof(WORD), 
				      ObcWriteIntComplete, dev, 1);
		}

		err = SUBMIT_URB(urb);
		if (err < 0) {
			status = FAILURE;
			goto fail;
		}
		
		status = WaitForObcCmdComplete(dev);
		if (status == FAILURE) {
			DBG(USB_D,"cmdBuff=%p,frameSize=%d,ep_out=%d\n",dataPtr->cmdBuff,dataPtr->frameSize,ep_out);
			DUMP_URB(urb);
			urb->dev = dev->usb_dev,
			urb->interval = 0,
			UNLINK_URB(urb);
			ObcReset();
		} else {
			break;
		}		
	}

	USB_SWAP_BUF(dataPtr->cmdBuff,dataPtr->frameSize);
fail:
	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//-----------------------------------------------------------------------------
// USB_L_Write:
//-----------------------------------------------------------------------------
ST_STATUS USB_L_Write(T_LongWrite *dataPtr,T_EpOut ep_out)
{
	struct unicorn_dev *dev = &unicorn_usb_dev;
	ULONG size = (dataPtr->nFrames-1)*dataPtr->frameSize + dataPtr->lastFrameSize;
	struct urb *urb;
	int retry;
	ST_STATUS status;
	int err;

	if (!CheckObcBuffer(dev,dataPtr->cmdBuff,size)) return FAILURE;
	
	if ((ep_out != EP2) && (ep_out != EP6)) {
		WARN(("Invalid ep_out parameter\n"));
		return FAILURE;
	}

	OBC_LOCK(&dev->obc_lock);

	USB_SWAP_BUF(dataPtr->cmdBuff,size);

	for (retry=0; retry < MAX_RETRIES; retry++) {
	
		DBG(USB_D,"cmdBuff=%p,nFrames=%d,frameSize=%d,lastFrameSize=%d,ep_out=%d\n",
			dataPtr->cmdBuff,dataPtr->nFrames,dataPtr->frameSize,dataPtr->lastFrameSize,ep_out);

		dev->obc_flags = 0x00;
		
		if (ep_out == EP2) {
			// ISOC
			urb = dev->obc_iso_out;
			// Build ISOC descriptor
			fill_isoc_urb(urb, dev->usb_dev, usb_sndisocpipe(dev->usb_dev,EP_OBC_ISO_OUT),
				      dataPtr->cmdBuff, size*sizeof(WORD),
				      dataPtr->frameSize*sizeof(WORD), 
				      ObcWriteIsocComplete, dev);


		} else {
			// Interrupt
			urb = dev->obc_int_out;
			
			FILL_INT_URB(urb, dev->usb_dev, usb_sndintpipe(dev->usb_dev,EP_OBC_INT_OUT),
				      dataPtr->cmdBuff, size*sizeof(WORD),
				      ObcWriteIntComplete, dev, 1);
		}
		err = SUBMIT_URB(urb);
		if (err < 0) {
			status = FAILURE;
			goto fail;
		}
		
		status = WaitForObcCmdComplete(dev);
		if (status == FAILURE) {
			DBG(USB_D,"cmdBuff=%p,nFrames=%d,frameSize=%d,lastFrameSize=%d,ep_out=%d\n",
			    dataPtr->cmdBuff,dataPtr->nFrames,dataPtr->frameSize,dataPtr->lastFrameSize,ep_out);
			DUMP_URB(urb);
			urb->dev = dev->usb_dev;
			urb->interval = 0;
			UNLINK_URB(urb);
			ObcReset();
		} else {
			break;
		}		
	}
	
	USB_SWAP_BUF(dataPtr->cmdBuff,size);
fail:
	OBC_UNLOCK(&dev->obc_lock);
	return status;

}

//-----------------------------------------------------------------------------
// USB_Read:
//-----------------------------------------------------------------------------
ST_STATUS USB_Read(T_ReadData *dataPtr,T_EpOut ep_out,T_EpIn ep_in)
{
	struct unicorn_dev *dev = &unicorn_usb_dev;
	BOOLEAN isoIn;
	PWORD ptr;
	struct urb *read_urb,*write_urb;
 	int retry;
	ST_STATUS status;
	int err;

	if (!CheckObcBuffer(dev,dataPtr->cmdBuff,dataPtr->wrSize)) return FAILURE;
	
	if ((ep_out != EP2) && (ep_out != EP6)) {
		WARN("Invalid ep_out parameter\n");
		return FAILURE;
	}

	if ((ep_in != EP3) && (ep_in != EP7) && (ep_in != EP5)) {
		WARN("Invalid ep_in parameter\n");
		return FAILURE;
	}

	OBC_LOCK(&dev->obc_lock);

	USB_SWAP_BUF(dataPtr->cmdBuff,dataPtr->wrSize);


	for (retry=0; retry < MAX_RETRIES; retry++) {

		DBG(USB_D,"cmdBuff=%p,wrSize=%d,rdSize=%d,ep_out=%d,ep_in=%d\n",
			dataPtr->cmdBuff,dataPtr->wrSize,dataPtr->rdSize,ep_out,ep_in);

		dev->obc_flags = OBC_READ_CMD;

		ptr = USB_checkIntContext() ? dev->usb_mem->CmdBufRd_I : dev->usb_mem->CmdBufRd;

		if ((ep_in == EP3) || (ep_in == EP5)) {
			int packet_size;
			int ep = ep_in == EP3  ? EP_OBC_ISO_IN : EP_ATM_ISO_IN;
			read_urb = dev->obc_iso_in;  
			isoIn = TRUE;	
			// Build ISOC descriptor
			packet_size = usb_maxpacket(dev->usb_dev,usb_rcvisocpipe(dev->usb_dev,ep),0);
			fill_isoc_urb(read_urb, dev->usb_dev, usb_rcvisocpipe(dev->usb_dev,ep),
				      ptr, dataPtr->rdSize*sizeof(WORD),
				      packet_size, 
				      ObcReadIsocComplete, dev);
		} else {
			read_urb = dev->obc_int_in;  

			isoIn = FALSE;	

			FILL_INT_URB(read_urb, dev->usb_dev, usb_rcvintpipe(dev->usb_dev,EP_OBC_INT_IN),
				      ptr, dataPtr->rdSize*sizeof(WORD),
				      ObcReadIntComplete, dev, 1);
			err = SUBMIT_URB(read_urb);
			if (err < 0) {
				status = FAILURE;
				goto fail;
			}
		}
	
		if (ep_out == EP2) {
			// ISOC
			int packet_size;
			write_urb = dev->obc_iso_out;
		
			// Build ISOC descriptor
			packet_size = usb_maxpacket(dev->usb_dev,
						    usb_sndisocpipe(dev->usb_dev,EP_OBC_ISO_OUT),1);
			fill_isoc_urb(write_urb, dev->usb_dev, usb_sndisocpipe(dev->usb_dev,EP_OBC_ISO_OUT),
				      dataPtr->cmdBuff, dataPtr->wrSize*sizeof(WORD),
				      packet_size, 
				      ObcWriteIsocComplete, dev);
		} else {
			// Interrupt
			write_urb = dev->obc_int_out;
	
			FILL_INT_URB(write_urb, dev->usb_dev, usb_sndintpipe(dev->usb_dev,EP_OBC_INT_OUT),
				      dataPtr->cmdBuff, dataPtr->wrSize*sizeof(WORD),
				      ObcWriteIntComplete, dev, 1);
			
		}

		err = SUBMIT_URB(write_urb);
		if (err < 0) {
			status = FAILURE;
			goto fail;
		}

		if (isoIn) {
			if (FrameNumber) {
				read_urb->transfer_flags &= ~USB_ISO_ASAP;
				read_urb->start_frame = usb_get_current_frame_number(dev->usb_dev)+FrameNumber;
			} else {
				// ASAP
			}
			err = SUBMIT_URB(read_urb);
			if (err < 0) {
				status = FAILURE;
				goto fail;
			}
		}
		
		status = WaitForObcCmdComplete(dev);
		if (status == FAILURE) {
			DBG(USB_D,"cmdBuff=%p,wrSize=%d,rdSize=%d,ep_out=%d,ep_in=%d\n",
			dataPtr->cmdBuff,dataPtr->wrSize,dataPtr->rdSize,ep_out,ep_in);

			// Cancel
			DUMP_URB(write_urb);
			read_urb->dev = dev->usb_dev;
			read_urb->interval = 0;
			UNLINK_URB(read_urb);
			write_urb->dev = dev->usb_dev;
			write_urb->interval = 0;
			UNLINK_URB(write_urb);
			ObcReset();
		} else {
			dataPtr->rdSize = read_urb->actual_length/2;
			DBG(USB_D,"rdSize=%d\n",dataPtr->rdSize);
			USB_SWAP_BUF(ptr,dataPtr->rdSize);
			break;
		}	
	}

	USB_SWAP_BUF(dataPtr->cmdBuff,dataPtr->wrSize);
fail:
	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
// unicorn_snd_getcell:	
// Returns a pointer to the transmit buffer
//----------------------------------------------------------------------
unsigned char *unicorn_snd_getcell(struct unicorn_dev *dev)
{
	struct send_atm *send_atm = &dev->send_atm;
	int t;
	ULONG n;
	PBYTE p;

	if (!send_atm->started) {
		WARN(("ATM Tx I/O error\n"));
		return NULL;
	}

	t = send_atm->turn_write;
	if (send_atm->lens[t] > 0) return NULL;

	n = send_atm->maxlen - send_atm->bufofs;
	if (n < CELL_LENGTH) return NULL;

	p = send_atm->bufs[t] + send_atm->bufofs;
	
	// next cell
	send_atm->bufofs += CELL_LENGTH;
	gLedXmit = 1;
	send_atm->cell_count++;
	return p;	
}

//----------------------------------------------------------------------
// unicorn_start_transmit:	
// Start transmit DMA
//----------------------------------------------------------------------
int unicorn_start_transmit(struct unicorn_dev *dev)
{
	struct send_atm *send_atm = &dev->send_atm;
	int t;
	unsigned long n;

	DBG(RW_D,"\n");

	t = send_atm->turn_write;
	n = send_atm->bufofs;
	if (n > 0) {
		send_atm->lens[t] = n;
		send_atm->turn_write = (t+1)&(ATM_WRITES-1);
		send_atm->bufofs = 0;
		test_and_set_bit(0,&send_atm->busy[t]);
		StartAtmUsXfer(dev,t,send_atm->bufs[t],n);
		return n;
	}
	return 0;
}

//----------------------------------------------------------------------
// unicorn_rcv_getcell:	
// Returns a pointer to the next cell header in the receive buffer
//----------------------------------------------------------------------
unsigned char *unicorn_rcv_getcell(struct unicorn_dev *dev)
{
	struct recv_atm *recv_atm = &dev->recv_atm;
	unsigned char *cell;

	cell = recv_atm->cells[recv_atm->turn_read];
	if (cell) {
		DBG(RW_D,"turn_read=%d,cell=%p\n",recv_atm->turn_read,cell);
		// next cell
		recv_atm->cells[recv_atm->turn_read++] = NULL;
		gLedRecv = 1;
		recv_atm->cell_count++;
	}
	return cell;
}

extern unsigned long RetryTime;
//----------------------------------------------------------------------
//	Modem Software Control functions
//----------------------------------------------------------------------
int unicorn_msw_control(struct unicorn_dev *dev,T_MswCtrl *ctrl)
{
	static unsigned long old_report = 0L;
	ctrl->retcode = -1;

	DBG(RW_D,"code=%ld,subcode=%ld,length=%ld\n",ctrl->code,ctrl->subcode,ctrl->length);

	switch(ctrl->code) {
	case MSW_CTL_INIT:
		if (ctrl->subcode <= 0) break;
		if (ctrl->subcode  >= 16) break;
		ActivationMode = ctrl->subcode ;
		dev->msw_started = TRUE;
		rapi_lock();
		msw_init(ActivationMode);
		rapi_unlock();
		ctrl->retcode = 0;
		ctrl->length = 0;
		break;

	case MSW_CTL_EXIT:
		rapi_lock();
		msw_exit();
		rapi_unlock();
		dev->msw_started = FALSE;
		ctrl->retcode = 0;
		ctrl->length = 0;
		break;

	case MSW_CTL_START:
		rapi_lock();
		msw_start();
		rapi_unlock();
		ctrl->retcode = 0;
		ctrl->length = 0;
		break;

	case MSW_CTL_STOP:
		rapi_lock();
		msw_stop();
		rapi_unlock();
		ctrl->retcode = 0;
		ctrl->length = 0;
		break;

	case MSW_CTL_SET_CONFIG:
		if (ctrl->buffer == NULL) break;
		rapi_lock();
		ctrl->retcode = AMSW_ANT_setModemConfiguration(ctrl->subcode,ctrl->buffer);
		rapi_unlock();
		ctrl->length = 0;
		break;

	case MSW_CTL_GET_CONFIG:
		if (ctrl->buffer == NULL) break;
		rapi_lock();
		ctrl->retcode = AMSW_ANT_getModemConfiguration(ctrl->subcode,ctrl->buffer);
		rapi_unlock();
		break;

	case MSW_CTL_GET_DATA:
		if (ctrl->buffer == NULL) break;
		rapi_lock();
		ctrl->retcode = AMSW_ANT_getData(ctrl->subcode,ctrl->buffer);
		rapi_unlock();
		break;

	case MSW_CTL_GET_STATE:
		if (ctrl->length < sizeof(AMSW_ModemState)) break;
		if (ctrl->buffer == NULL) break;
		rapi_lock();
		ctrl->retcode = AMSW_ANT_getModemState((AMSW_ModemState*)ctrl->buffer);
		rapi_unlock();
		break;

	case MSW_CTL_REQ_CHANGE:
		rapi_lock();
		ctrl->retcode = AMSW_ANT_requestModemStateChange((AMSW_ModemState)ctrl->subcode);
		rapi_unlock();
		ctrl->length = 0;
		break;

	case MSW_CTL_DYING_GASP:
		rapi_lock();
		ctrl->retcode = AMSW_ANT_dyingGasp();
		rapi_unlock();
		ctrl->length = 0;
		break;
	case MSW_CTL_SETCARRIERCONSTELLATION:
		ctrl->retcode = AMSW_ANT_setCarrierConstellation(ctrl->subcode);
		break;

	case MSW_CTL_WAIT_EVENT:
		if (ctrl->length < sizeof(unsigned long)) break;
		if (ctrl->buffer == NULL) break;
		if (last_report != old_report) {		
		        rapi_lock();
			ctrl->retcode = AMSW_ANT_wait_event((unsigned long*)ctrl->buffer);
		        rapi_unlock();
			old_report = last_report;
		} else {
			ctrl->buffer = 0;
			//ctrl->length = 0;
		}
		break;
	case MSW_CTL_VERSION_INFO:
		if (ctrl->length < sizeof(unsigned long)) break;
		if (ctrl->buffer == NULL) break;
		*((unsigned long *)ctrl->buffer) = VERS;
		ctrl->retcode = 0;
		break;
	case MSW_CTL_STATE_INFO:
		if (ctrl->length < sizeof(T_StateInfo)) break;
		if (ctrl->buffer == NULL) break;
		{
			T_StateInfo *info = (T_StateInfo *)ctrl->buffer;
			info->State = g_ModemState;
			ctrl->retcode = 0;
			if (info->State == C_AMSW_IDLE) {
				last_event = (AMSW_ModemEvent)-1;
			}
			if (info->State == C_AMSW_SHOWTIME_L0) {
				last_failure = (AMSW_ModemFailure)-1;
				info->TimeCnctd = xtm_elapse(adsl_system_time);
			} else {
				info->TimeCnctd = 0;
			}
			info->Report = last_event;
			info->Failure = last_failure;
			info->RcvdCells = dev->recv_atm.cell_count;
			info->SentCells = dev->send_atm.cell_count;
		}
		break;
	case MSW_CTL_SET_DEBUG_LEVEL:
		DebugLevel = ctrl->subcode;
		ctrl->retcode = 0;
		ctrl->length = 0;
		break;
	case MSW_CTL_SET_MSW_DEBUG_LEVEL:
		MswDebugLevel = ctrl->subcode;
		ctrl->retcode = 0;
		ctrl->length = 0;
		break;
	}

	DBG(RW_D,"retcode=%ld,length=%ld\n",ctrl->retcode,ctrl->length);

	return 0;
}

//----------------------------------------------------------------------
// unicorn_get_adsl_status	
//----------------------------------------------------------------------
ADSL_STATUS unicorn_get_adsl_status(struct unicorn_dev *dev)
{
	return adsl_status;
}

//----------------------------------------------------------------------
// unicorn_get_adsl_linkspeed
//----------------------------------------------------------------------
int unicorn_get_adsl_linkspeed(struct unicorn_dev *dev,
			       unsigned long *us_rate,unsigned long *ds_rate)
{
	int status;

	if (adsl_status != ADSL_STATUS_ATMREADY) {
		*us_rate = 0;
		*ds_rate = 0;
		status = -ENXIO;
	} else {
		// adjust upstream rate to avoid overflow
		unsigned long speed = (adsl_us_cellrate*15UL)/16UL;
		*us_rate = (speed * ATM_CELL_SIZE*8UL)/1000UL;
		*ds_rate = (adsl_ds_cellrate * ATM_CELL_SIZE*8UL)/1000UL;
		status = 0;
	}
	return status;
}

//----------------------------------------------------------------------
// get_frame_number_offset
//----------------------------------------------------------------------
static int get_frame_number_offset(struct usb_device *usb_dev)
{
	int ret=0; // default, USB_ISOC_ASAP 

	if (usb_dev->bus) {
		struct usb_device *root_hub = usb_dev->bus->root_hub;
		if (root_hub->descriptor.iProduct) {
			char *buf = kmalloc(256, GFP_KERNEL);
			if (buf) {
				if (usb_string(root_hub, 
					       root_hub->descriptor.iProduct, buf, 256) > 0) {
					INFO("%s\n",buf);
					if (strcmp(buf,"USB UHCI Root Hub")==0) {
						// usb-uhci driver:
						// need to wait 12 frames before reading OBC
						ret = 12;
					}
				}
				kfree(buf);
			}
		}
	}
	return ret;
}

//----------------------------------------------------------------------
// probe_unicorn_usb
//----------------------------------------------------------------------
static int do_probe(struct usb_device *usb_dev,unsigned long driver_info)
{
	struct unicorn_dev *dev = &unicorn_usb_dev;
	int status;
	
	INFO("found adapter VendorId %04x, ProductId %04x, driver_info=%ld\n",
	     usb_dev->descriptor.idVendor, usb_dev->descriptor.idProduct,driver_info);
		
	GlobalRemove = FALSE;

	useAFE = driver_info;
	set_msw_params();

	// Initialize RAPI
	if ((status = rapi_init()) != 0) {
		WARN(("inititalization of RAPI failed\n"));
		return status;
	}

	// tweaks for different host controller drivers
	FrameNumber = get_frame_number_offset(usb_dev);
	DBG(1,"FrameNumber=%d\n",FrameNumber);

	// Initialize USB adapter
	if ((status = start_device(dev,usb_dev)) != 0) {
		WARN(("inititalization of USB failed\n"));
		return status;
	}

	// Tell ATM driver we are initialized
	if ((status = unicorn_attach(&unicorn_usb_entrypoints)) != 0) {
		WARN("inititalization of ATM driver failed\n");
		return status;
	}

	// Start modem software
	if ((ActivationMode > MSW_MODE_UNKNOWN) && (ActivationMode < MSW_MODE_MAX)) {
	  rapi_lock();
	  msw_init(ActivationMode);
	  dev->msw_started = TRUE;
	  //xtm_wkafter(100);
	  if (AutoActivation) {
	    msw_start();
	  }
	  rapi_unlock();
	}
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static int __devinit probe_unicorn_usb(struct usb_interface *usb_intf,
				     const struct usb_device_id *id)
{
	struct usb_device *usb_dev = interface_to_usbdev(usb_intf);
	int status;

	status = do_probe(usb_dev,id->driver_info);
	usb_set_intfdata(usb_intf,&unicorn_usb_dev);
	return status;
}
#else
static void * __devinit probe_unicorn_usb(struct usb_device *usb_dev,
				     unsigned int ifnum,
				     const struct usb_device_id *id)
{
     if (do_probe(usb_dev,id->driver_info)) {
	     return NULL;
     } else {
	     return &unicorn_usb_dev;
     }
}
#endif

//----------------------------------------------------------------------
// disconnect_unicorn_usb
//----------------------------------------------------------------------
static void do_disconnect(struct unicorn_dev *dev)
{
	DBG(1,"\n");
	
	GlobalRemove = TRUE;
	stop_device(dev);
	rapi_exit();	
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static void __devexit disconnect_unicorn_usb(struct usb_interface *usb_intf)
{
	do_disconnect(usb_get_intfdata(usb_intf));
}
#else
static void __devexit disconnect_unicorn_usb(struct usb_device *usb_dev, void *arg)
{
	do_disconnect(arg);
}
#endif


static struct usb_device_id unicorn_usb_ids[] = {
	{ USB_DEVICE(0x0483, 0x0137),driver_info: 20174}, // STMicro reference design
	{ USB_DEVICE(0x07FA, 0xA904),driver_info: 70136}, // BeWAN
	{ USB_DEVICE(0x07FA, 0xA905),driver_info: 70136}, // BeWAN
	{ USB_DEVICE(0x07FA, 0x1012),driver_info: 20174}, // BeWAN
	{ USB_DEVICE(0x135A, 0x0001),driver_info: 20174}, // urmet
	{ }
};

MODULE_DEVICE_TABLE (usb, unicorn_usb_ids);

static struct usb_driver unicorn_usb_driver = {
	name: "unicorn_usb",
	probe: probe_unicorn_usb,
	disconnect: __devexit_p(disconnect_unicorn_usb),
	id_table: unicorn_usb_ids,
};

/* module parameters for MSW */
module_param(ActivationMode, ulong, 0);
module_param(ActTimeout, ulong, 0);
module_param(AutoActivation, ulong, 0);
module_param(DownstreamRate, ulong, 0);
module_param(eocTrace, ulong, 0);
module_param(ExchangeDelay, ulong, 0);
module_param(FmPollingRate, ulong, 0);
module_param(g_RefGain, ulong, 0);
module_param(g_TeqMode, ushort, 0);
module_param(InitTimeout, ulong, 0);
module_param(Interoperability, ulong, 0);
module_param(LCD_Trig, ulong, 0);
module_param(LOS_LOF_Trig, ulong, 0);
module_param(LoopbackMode, ulong, 0);
module_param(MswDebugLevel, ulong, 0);
module_param(RetryTime, ulong, 0);
module_param(TrainingDelay, ulong, 0);
module_param(useAFE, ulong, 0);
module_param(useRFC019v, ulong, 0);
module_param(useRFC029v, ulong, 0);
module_param(useRFC040v, ulong, 0);
module_param(useRFC041v, ulong, 0);
module_param(useVCXO, ulong, 0);
module_param(_no_TS652, ulong, 0);
#if DEBUG
module_param(DebugLevel, ulong, 0);
#endif

//----------------------------------------------------------------------
// unicorn_usb_init
//----------------------------------------------------------------------
static int __init unicorn_usb_init(void)
{
	int status;
	struct unicorn_dev *dev = &unicorn_usb_dev;

	dev->started = FALSE;

	INFO("v %d.%d.%d, " __TIME__ " " __DATE__"\n",
	     (VERS>>8)&0xf,(VERS>>4)&0xf,VERS&0xf);
	INFO("driver parameters: DebugLevel=%ld\n",
	    DebugLevel);

	status = usb_register(&unicorn_usb_driver);
	if (status < 0) {
	}
	return status;
}

static void __exit unicorn_usb_cleanup(void)
{
	unicorn_detach();
	usb_deregister(&unicorn_usb_driver);
}

module_init(unicorn_usb_init);
module_exit(unicorn_usb_cleanup);
