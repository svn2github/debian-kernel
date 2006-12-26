/*
  This driver supports the Unicorn ADSL chipset from STMicroelectronics.
  The chipset consists of the ADSL DMT transceiver ST70137 and either the
  ST70134A, ST70136 or ST20174 Analog Front End (AFE).
  This file contains the PCI specific routines.
*/
#include <linux/config.h>
#include <linux/version.h>
#if defined(CONFIG_MODVERSIONS) && (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
#include <linux/modversions.h>
#endif
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include "types.h"
#include "hal.h"
#include "hard.h"
#include "rapi.h"
#include "amu/amas.h"
#include "unicorn.h"
#include "debug.h"

#ifndef VERS
#define VERS 0
#endif

MODULE_AUTHOR ("fisaksen@bewan.com");
MODULE_DESCRIPTION ("Driver for the ST UNICORN PCI ADSL modem chip.");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Proprietary");
#endif


#define PCI_VENDOR_ID_ST         0x104a
#define PCI_DEVICE_ID_UNICORN    0x0500

#define  ATM_CELL_SIZE 53
//----------------------------------------------------------------------
// Macros to read/write the little endian PCI bus
//----------------------------------------------------------------------
static inline void PCI_PUT_DWORD(DWORD value,volatile void *base,int reg) 
{
	writel(value,(PBYTE)base + reg);
}

static inline DWORD PCI_GET_DWORD(volatile void *base,int reg) 
{
	return readl((PBYTE)base + reg);
}

#ifdef BIG_ENDIAN
static inline void PCI_SWAP_BUF(DWORD *buf,DWORD size)
{
	int i;
	for (i=0; i < size; i++) 
		swab32(buf[i]);
}
#else
#define PCI_SWAP_BUF(buf,size)
#endif

//----------------------------------------------------------------------
// Delay in microseconds
//----------------------------------------------------------------------
#define DELAY(us) mdelay(us)

//----------------------------------------------------------------------
// driver parameters
//----------------------------------------------------------------------
#if DEBUG
unsigned long DebugLevel=0;
#endif

//----------------------------------------------------------------------
// MSW paramters
//----------------------------------------------------------------------
//unsigned long ActivationMode = MSW_MODE_MULTI;
unsigned long ActivationMode = MSW_MODE_ANSI;

//extern unsigned long ActivationTaskTimeout;
extern unsigned long ActTimeout;
extern unsigned long DownstreamRate;
unsigned long DownstreamRate=8192;	// In Kbits/sec	
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
extern unsigned long useRFC019v;
extern unsigned long useRFC029v;
//extern unsigned long useRFC033v;
extern unsigned long useRFC040v;
extern unsigned long useRFC041v;
extern unsigned long useRFCFixedRate;
extern unsigned long useVCXO;
extern unsigned long _no_TS652;

extern unsigned long eocTrace;
extern unsigned long useAOCChannel;
extern unsigned long setINITIALDAC;
extern unsigned long useAFE;
extern unsigned long pilotRealloc;
extern unsigned long _newCToneDetection_;

extern unsigned long pilotRootPowerWorkAround;
extern unsigned long _gi_step_;
extern unsigned long _teq_new_delay_;
extern unsigned long TNumberOfCarrier;
extern unsigned long highCarrierOff;
extern unsigned long decreaseHighCarrier;
extern unsigned long _boostPowerGdmt_;
extern unsigned long g_ModemState;
extern unsigned long _trellisact_;
extern unsigned long pvo_pembdpllpolarity;

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

extern unsigned long timer_int_counter;
unsigned long timer_int_counter=0;

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
#define	ATM_READS	4			// ATM downstream DMA buffers
#define	ATM_WRITES	2			// ATM upstream DMA buffers
#define	CELL_LENGTH PCI_CELL_LENGTH

struct send_atm {
	unsigned long turn_write;	        // Current Write buffer
	unsigned long turn_send;		// Current sending buffer
	unsigned char *bufs[ATM_WRITES];
	unsigned long lens[ATM_WRITES];
	unsigned long busy[ATM_WRITES];
	unsigned long maxlen;
	unsigned long bufofs;
	unsigned long send_pending;
	BOOLEAN started;
	unsigned long long cell_count;
	spinlock_t lock;
};

struct recv_atm {
	unsigned long turn_read;
	unsigned char *bufs[ATM_READS];
	unsigned long maxlen;
	BOOLEAN started;
	int turn_recv;
	unsigned long bufofs;
	unsigned long long cell_count;
	spinlock_t lock;	
};
 
//#define OBC_LOCK(sem) down(sem)
#define OBC_LOCK(sem) \
if (down_trylock(sem)) { \
   DBG(PCI_D,"down_trylock failed\n"); \
   down(sem); \
} 
#define OBC_UNLOCK(sem) up(sem)

struct unicorn_dev {
	struct pci_dev                  *pci_dev;
	volatile DWORD		        *mem;			// Memory Range of the PCI Card
	DWORD                           mem_size;               // size of PCI memory
      	void                            *dma_buffer;		// DMA Kernel Buffer
	struct send_atm		        send_atm;		// US ATM object for transmission
	struct recv_atm		        recv_atm;		// DS ATM object for transmission
	WORD			        isr_enable_mask;	// Enable bits in the ISR register
	WORD			        lacfw;		        // copy of LACFW register
	DWORD			        isdr_enable_mask;	// Enable bits in the ISDR register
	DWORD			        pcfw;			// copy of PCFW register
	unsigned int	                dma_phys_addr;		// DMA buffer physical address	
	volatile PBYTE			dma_virtual_addr;	// DMA buffer virtual address
	
	unsigned long                   obc_pending;            // OBC command pending
	struct semaphore                obc_lock;               // To protect access to OBC
	DWORD                           obc_sem;                // To wait for OBC command completed

	BOOLEAN		                started;		// Device is started
	BOOLEAN				stopping;		// Device is being stopped
	BOOLEAN				msw_started;		// True if the MSW is (manually) started
};
struct unicorn_dev unicorn_pci_dev;

struct unicorn_entrypoints unicorn_pci_entrypoints = {
	&unicorn_pci_dev,
	unicorn_snd_getcell,
	unicorn_rcv_getcell,
	unicorn_start_transmit,
	unicorn_msw_control,
	unicorn_get_adsl_status,
	unicorn_get_adsl_linkspeed
};
	
//----------------------------------------------------------------------
//	Start the ATM upstream DMA
//----------------------------------------------------------------------
void StartAtmUsXfer(struct unicorn_dev *dev,unsigned long turn,unsigned char *buffer,unsigned long length)
{
	DWORD ptr;
	DWORD size;

	ptr = dev->dma_phys_addr+offsetof(DMA_MEMORY,AtmUsBuf);
	ptr += turn*PCI_US_BUF_SIZE*sizeof(DWORD);
	size = length/sizeof(DWORD);

	DBG(RW_D,"turn=%ld,ptr=%lx\n",turn,ptr);

	if (turn & 1) {
		PCI_PUT_DWORD(ptr,dev->mem,PR_US_PTR_2);
		PCI_PUT_DWORD(size,dev->mem,PR_US_SIZE_2);
		PCI_PUT_DWORD(LACFW_EDMA_US2,dev->mem,PR_LACFW_SET);
	} else {
		PCI_PUT_DWORD(ptr,dev->mem,PR_US_PTR_1);
		PCI_PUT_DWORD(size,dev->mem,PR_US_SIZE_1);
		PCI_PUT_DWORD(LACFW_EDMA_US1,dev->mem,PR_LACFW_SET);
	}
}

//----------------------------------------------------------------------
//	Start the ATM downstream DMA
//----------------------------------------------------------------------
void StartAtmDsXfer(struct unicorn_dev *dev,unsigned long turn)
{
	DWORD ptr;

	if (dev->stopping) return;
	ptr = dev->dma_phys_addr+offsetof(DMA_MEMORY,AtmDsBuf);
	ptr += turn*PCI_DS_BUF_SIZE*sizeof(DWORD);

	DBG(RW_D,"turn=%ld,ptr=%lx\n",turn,ptr);

	if (turn & 1) {
		PCI_PUT_DWORD(ptr,dev->mem,PR_DS_PTR_2);
		PCI_PUT_DWORD(LACFW_EDMA_DS2,dev->mem,PR_LACFW_SET);
	} else {
		PCI_PUT_DWORD(ptr,dev->mem,PR_DS_PTR_1);
		PCI_PUT_DWORD(LACFW_EDMA_DS1,dev->mem,PR_LACFW_SET);
	}
}

//----------------------------------------------------------------------
// send_atm_init:	
//----------------------------------------------------------------------
static void send_atm_init(struct unicorn_dev *dev,	      
	      unsigned char *buffer,unsigned long size)
{
	struct send_atm *send_atm = &dev->send_atm;
	int i;

	DBG(RW_D,"buffer=%p,size=%ld\n",buffer,size);

	send_atm->maxlen = size;
	send_atm->bufofs = 0;
	send_atm->turn_write = 0;
	send_atm->turn_send = 0;
	for (i=0; i  <ATM_WRITES; i++) {
		send_atm->bufs[i] = buffer+i*size;
		send_atm->lens[i] = 0;
		send_atm->busy[i] = 0;
	}	
	send_atm->send_pending = 0;
	send_atm->started = FALSE;
	send_atm->cell_count = 0;
	send_atm->lock = SPIN_LOCK_UNLOCKED;
}

//----------------------------------------------------------------------
// send_atm_exit:	
//----------------------------------------------------------------------
static void send_atm_exit(struct unicorn_dev *dev)
{
	struct send_atm *send_atm = &dev->send_atm;
	send_atm->started = FALSE;
}

//----------------------------------------------------------------------
// recv_atm_init
//----------------------------------------------------------------------
static void recv_atm_init(struct unicorn_dev *dev,	
	      unsigned char *buffer,unsigned long size)
{
	struct recv_atm *recv_atm = &dev->recv_atm;
	int i;

	DBG(RW_D,"buffer=%p,size=%ld\n",buffer,size);

	recv_atm->maxlen = size;
	recv_atm->turn_read = 0;
	recv_atm->cell_count = 0;

	recv_atm->started = FALSE;
	recv_atm->turn_recv = 0;
	recv_atm->bufofs = 0;

	for (i=0; i<ATM_READS; i++) {
		recv_atm->bufs[i] = buffer+i*size;
		memset(recv_atm->bufs[i],0,size);
	}
	recv_atm->lock = SPIN_LOCK_UNLOCKED;
}

//----------------------------------------------------------------------
// recv_atm_exit:
//----------------------------------------------------------------------
static void recv_atm_exit(struct unicorn_dev *dev)
{
}

//----------------------------------------------------------------------
// atm_send_complete:
//----------------------------------------------------------------------
static void atm_send_complete(struct unicorn_dev *dev)
{
	struct send_atm *send_atm = &dev->send_atm;
	int t;
	unsigned long flags;

	DBG(RW_D,"\n");

	spin_lock_irqsave(&send_atm->lock, flags);
	send_atm->send_pending--;
	t = send_atm->turn_send;
	if (test_and_clear_bit(0,&send_atm->busy[t])) {
		send_atm->lens[t] = 0;
		t = (t+1)&(ATM_WRITES-1);
		send_atm->turn_send = t;
	}
	spin_unlock_irqrestore(&send_atm->lock, flags);
}

//----------------------------------------------------------------------
//	clear_buffer
//----------------------------------------------------------------------
static void clear_buffer(struct recv_atm *recv_atm,int i)
{
	unsigned long n;
	PDWORD p = (PDWORD)recv_atm->bufs[i];
	for (n=PCI_CELL_LENGTH-4; n<recv_atm->maxlen; n+=PCI_CELL_LENGTH)
	{
		p[n/sizeof(DWORD)] = 0;
	}
}

//----------------------------------------------------------------------
// atm_start_rcv
//----------------------------------------------------------------------
static void atm_start_rcv(struct unicorn_dev *dev)
{
	struct recv_atm *recv_atm = &dev->recv_atm;
	int i;

	if (recv_atm->started) return;

	recv_atm->started = TRUE;
	recv_atm->turn_recv = 0;
	recv_atm->turn_read = 0;
	recv_atm->bufofs = 0;

	for (i=0; i<ATM_READS; i++) {
		clear_buffer(recv_atm,i);
	}

	StartAtmDsXfer(dev,recv_atm->turn_recv++);
	StartAtmDsXfer(dev,recv_atm->turn_recv);
}

//----------------------------------------------------------------------
// atm_start_snd
//----------------------------------------------------------------------
static void atm_start_snd(struct unicorn_dev *dev)
{
	struct send_atm *send_atm = &dev->send_atm;

	send_atm->started = TRUE;
}

//----------------------------------------------------------------------
// atm_recv_complete   
//----------------------------------------------------------------------
static void atm_recv_complete(struct unicorn_dev *dev)
{
	struct recv_atm *recv_atm = &dev->recv_atm;
	PDWORD pf;
	unsigned long flags;

	DBG(RW_D,"\n");

	spin_lock_irqsave(&recv_atm->lock, flags);

	// point to the next DMA buffer
	recv_atm->turn_recv = (recv_atm->turn_recv+1)&(ATM_READS-1);

	// If the last cell of the buffer is not cleared
	// increment the read pointer before starting the DMA
	// We loose the contents of the buffer
	pf = (PDWORD)(recv_atm->bufs[recv_atm->turn_recv]+PCI_DS_BUF_SIZE-4);
	if (*pf) {
		WARN("ERROR: ATM DownStream buffer overflow\n");
		clear_buffer(recv_atm,recv_atm->turn_recv);
		recv_atm->turn_read = (recv_atm->turn_recv+1)&(ATM_READS-1);
		recv_atm->bufofs = 0;
	}
	spin_unlock_irqrestore(&recv_atm->lock, flags);

	// Start the DMA
	StartAtmDsXfer(dev,recv_atm->turn_recv);
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
	struct unicorn_dev *dev = &unicorn_pci_dev;

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
		break;
	}
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

	adsl_us_cellrate= upRate;
	adsl_ds_cellrate= downRate;
}

//----------------------------------------------------------------------
//	Copy the TOSCA hardware interrupt table applying a OR to the result
//	This function is called from the hardware ISR
//----------------------------------------------------------------------
static void CopyHardIntrTable(struct unicorn_dev *dev)
{
	int i;
	
	for (i=0; i<14; i++) {
		tosca_hardITABLE[i] |= PCI_GET_DWORD(dev->mem,PR_ITABLE+(i*sizeof(DWORD)));
	}
}

//----------------------------------------------------------------------
//	OBC command interrupt received
//----------------------------------------------------------------------
static void obc_interrupt(struct unicorn_dev *dev)
{
	if (test_and_clear_bit(0, &dev->obc_pending)) {
		xsm_v(dev->obc_sem);
	} else {
		DBG(INTR_D,"spurious OBC interrupt\n");
	}
}


//----------------------------------------------------------------------
//	Interrupt Service Routine (ISR) for IRQ Irq	
//----------------------------------------------------------------------
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static irqreturn_t unicorn_isr(int irq, void *context, struct pt_regs *regs)
#else
static void unicorn_isr(int irq, void *context, struct pt_regs *regs)
#endif
{
	struct unicorn_dev *dev = context;
	DWORD mask;			// Mask to apply to read the register
	DWORD rmask;			// Mask to apply to reset the interrupts
	DWORD isdr;			// IDSR register
	WORD isr;			// ISR register

loop:
	isdr = PCI_GET_DWORD(dev->mem,PR_ISDR);
	DBG(INTR_D,"ISDR=%lx\n",isdr);
	if ((isdr & PISDR_IF) == 0) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
		return IRQ_HANDLED;
#else
		return;
#endif		
	}
	rmask = dev->isdr_enable_mask;

	// PCI parity error and bus error check
	if (isdr & (PISDR_PEIF|PISDR_BEIF)) {
		WARN("PCI Parity/Bus Error !!!!!\n");
		dev->isdr_enable_mask = 0;			// disable all interrupts
		PCI_PUT_DWORD(PISDR_IF,dev->mem,PR_ISDR);		// Clear all interrupts pending
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
		return IRQ_HANDLED;
#else
		return;
#endif
	}

	// ATM US DMA interrupt pointer 1
	mask = PISDR_DMA_US1_IF;
	if (isdr & mask) {
		DBG(INTR_D,"ATM US ptr1 DMA interrupt\n");
		rmask |= mask;

		atm_send_complete(dev);
	}

	// ATM US DMA interrupt pointer 2
	mask = PISDR_DMA_US2_IF;
	if (isdr & mask) {
		DBG(INTR_D,"ATM US ptr2 DMA interrupt\n");
		rmask |= mask;

		atm_send_complete(dev);
	}

	// ATM DS DMA interrupt pointer 1
	mask = PISDR_DMA_DS1_IF;
	if (isdr & mask) {
		DBG(INTR_D,"ATM DS ptr1 DMA interrupt\n");
		rmask |= mask;

		atm_recv_complete(dev);
	}

	// ATM DS DMA interrupt pointer 2
	mask = PISDR_DMA_DS2_IF;
	if (isdr & mask) {
		DBG(INTR_D,"ATM DS ptr2 DMA interrupt\n");
		rmask |= mask;

		atm_recv_complete(dev);
	}

	// OBC DMA interrupt
	mask = PISDR_DMA_OBC_IF;
	if (isdr & mask) {
		DBG(INTR_D,"OBC DMA interrupt\n");
		rmask |= mask;

		obc_interrupt(dev);
	}

	// ACTD interrupt
	mask = PISDR_ACTDIF;
	if (isdr & mask) {
		DBG(INTR_D,"ACTD interrupt\n");
		rmask |= mask;
	}

	// ADSL interrupts
	mask = PISDR_AIF1 | PISDR_AIF2;
	if ((isdr & mask) == 0) {
		PCI_PUT_DWORD(rmask,dev->mem,PR_ISDR);
		goto loop;
	}
	rmask |= mask;
	PCI_PUT_DWORD(rmask,dev->mem,PR_ISDR);

	isr = (WORD)PCI_GET_DWORD(dev->mem,PR_ISR);
	DBG(INTR_D,"ADSL uP interrupt,ISR=%04x\n",isr);
	rmask = dev->isr_enable_mask;

	// TOSCA macrocell interrupt
	mask = ISR_TOIF | ISR_TOIFS;
	if (isr & mask) {
		DBG(INTR_D,"TOSCA macrocell interrupt\n");
		CopyHardIntrTable(dev);
		rmask |= mask;

		// Wake up TOSCA interrupt_task
		tosca_interrupt();
	}

	// Timer interrupt
	mask = ISR_TIMIF;
	if (isr & mask) {
		DBG(INTR_D,"Timer interrupt\n");
		rmask |= mask;
		++timer_int_counter;
#ifdef USE_HW_TIMER	
		unicorn_timer();
#endif
	}

	// GPIO interrupt
	mask = ISR_GPIFA | ISR_GPIFB;
	if (isr & mask) {
		WARN("GPIO interrupt\n");
		rmask |= mask;
	}

	// OBC Slave Command Complete interrupt
	mask = ISR_OSIF;
	if (isr & mask) {
		DBG(INTR_D,"OBC Slave Command Complete interrupt\n");
		rmask |= mask;

		obc_interrupt(dev);
	}

	// OBC Master Command Complete interrupt
	mask = ISR_OMIF;
	if (isr & mask) {
		DBG(INTR_D,"OBC Master Command Complete interrupt\n");
		rmask |= mask;
	}
	PCI_PUT_DWORD((WORD)rmask,dev->mem,PR_ISR);
	goto loop;
}

//======================================================================
//	FUNCTIONS TO ACCESS THE HARDWARE
//======================================================================

//----------------------------------------------------------------------
//	Set the Loopback mode
//----------------------------------------------------------------------
void set_loopback_mode(int loop)
{
	ST_STATUS status;
	T_SlaveDirData p;

	DBG(1,"loop=%d\n",loop);

	p.iaddr  = 0x32;
	p.idata  = loop ? 0x18 : 0;
	p.status = 0xE821;
	status = PCI_SlaveWriteDirect(&p);
	if (status == FAILURE) {
		WARN("PCI_SlaveWriteDirect failed\n");
		return;
	}
	if (loop) {
		p.iaddr  = 0x0c44;
		p.idata  = 0x2800;
		p.status = 0xE821;
		status = PCI_SlaveWriteDirect(&p);
		if (status == FAILURE) {
			WARN("PCI_SlaveWriteDirect failed\n");
		}
	}
}

//----------------------------------------------------------------------
//	Starts the hardware
//----------------------------------------------------------------------
static void start_hardware(struct unicorn_dev *dev)
{
	dev->isdr_enable_mask =
		PISDR_AIE1 | PISDR_AIE2 |
		PISDR_DMA_OBC_IE |
		PISDR_DMA_US_IE | PISDR_DMA_DS_IE |
		PISDR_PEIE | PISDR_BEIE | PISDR_ACTDIE;
	dev->isr_enable_mask = ISR_OSIE | ISR_TOIES | ISR_TIMIE;
	// dev->lacfw = 0; // half ATM cell max DMA burst size
	dev->lacfw = LACFW_W_DS_SIZE | LACFW_W_US_SIZE; // ATM cell max DMA burst size
	//dev->lacfw = LACFW_W_DS_SIZE | LACFW_W_US_SIZE | LACFW_W_WR_SIZE | LACFW_W_RD_SIZE; // max DMA burst size

	//STM:	27-11-2000: Sequence of reset for new AFE

	PCI_PUT_DWORD(0x10,dev->mem,PR_PCFW); //STM: release AFE reset
	
	DELAY(100);		
	
	PCI_PUT_DWORD(0x30,dev->mem,PR_PCFW); //STM  enable TOSCA clock
	PCI_PUT_DWORD(0x32,dev->mem,PR_PCFW); //STM: reset ADSL uP
	PCI_PUT_DWORD(0x30,dev->mem,PR_PCFW); //STM: release reset ADSL uP
	dev->pcfw = 0x30;
	
	DELAY(100);		

	PCI_PUT_DWORD(dev->isdr_enable_mask,dev->mem,PR_ISDR);
	PCI_PUT_DWORD((WORD)dev->isr_enable_mask,dev->mem,PR_ISR); 
	PCI_PUT_DWORD((WORD)0x8000,dev->mem,PR_IMASKS);  
	PCI_PUT_DWORD((WORD)0x0100,dev->mem,PR_IADR_IRQ); 
	PCI_PUT_DWORD( dev->lacfw,dev->mem,PR_LACFW); 

	PCI_PUT_DWORD(PCI_DS_BUF_SIZE,dev->mem,PR_DS_SIZE_1); 
	PCI_PUT_DWORD(PCI_DS_BUF_SIZE,dev->mem,PR_DS_SIZE_2); 

	if (LoopbackMode) {
		set_loopback_mode(1);
		
		atm_start_snd(dev);
		atm_start_rcv(dev);
	}

#ifdef USE_HW_TIMER	
	// Start TIMER_A (2ms periodic)
	PCI_PUT_DWORD(0xFFFF,dev->mem,PR_TIM_REG_A); 	
#endif
#ifdef PCI_BRIDGE_WORKAROUND
	pci_write_config_byte(dev->pci_dev,PCI_CACHE_LINE_SIZE, 0x02);
#endif
	// Program a new value for the PCI DMA access grant timeout
	pci_write_config_byte(dev->pci_dev, PCI_LATENCY_TIMER, 255);
#ifdef KT400
	// Fix for MSI KT4 Ultra motherboard
	pci_write_config_byte(dev->pci_dev, 0x40, 0);
	pci_write_config_byte(dev->pci_dev, 0x41, 0);
#else
	pci_write_config_byte(dev->pci_dev, 0x40, 255);
	pci_write_config_byte(dev->pci_dev, 0x41, 255);
#endif
}

//----------------------------------------------------------------------
//	Stops the hardware
//----------------------------------------------------------------------
static void stop_hardware(struct unicorn_dev *dev)
{
	DBG(1,"\n");

	dev->isdr_enable_mask = 0;
	dev->isr_enable_mask = 0;
	dev->lacfw = 0;

	PCI_PUT_DWORD((WORD)0,dev->mem,PR_TIM_REG_A); 
	PCI_PUT_DWORD((LACFW_EDMA_DS | LACFW_EDMA_US),dev->mem,PR_LACFW_CLR); 
	PCI_PUT_DWORD((LACFW_EDMA_WROBC | LACFW_EDMA_RDOBC),dev->mem,PR_LACFW_CLR); 

	//PCI_PUT_DWORD(0,dev->mem,PR_LACFW); 
	PCI_PUT_DWORD(0,dev->mem,PR_ISDR); 
	PCI_PUT_DWORD((WORD)0,dev->mem,PR_ISR); 
}

//----------------------------------------------------------------------
//	Find the PCI device
//----------------------------------------------------------------------
static struct pci_dev *find_unicorn(void)
{
	struct pci_dev *pci_dev;

	pci_dev = NULL;
	do {
		pci_dev = pci_find_device(PCI_VENDOR_ID_ST,
				     PCI_DEVICE_ID_UNICORN,
					  pci_dev);
		if (pci_dev) {
			DBG(1,"vendor=%04x,device=%04x,irq=%d\n",
			    pci_dev->vendor,pci_dev->device,pci_dev->irq);
			if (pci_enable_device(pci_dev)) {
				// try next card
				WARN("can't enable PCI device\n");
			} else {
				// found
				break;
			}
		}
	} while (pci_dev);
	return pci_dev;
}

//----------------------------------------------------------------------
//	Start the PCI device
//----------------------------------------------------------------------
int start_device(struct unicorn_dev *dev,struct pci_dev *pci_dev)
{
	unsigned long phys_base;
	int status;

	dev->pci_dev = pci_dev;

	pci_set_master(pci_dev);

	// Initialize the Common DMA buffer
	dev->dma_buffer = pci_alloc_consistent(pci_dev,sizeof(DMA_MEMORY),&dev->dma_phys_addr);
	if (dev->dma_buffer == NULL) {
		WARN("Failed to initialize common DMA buffer\n");
		return -ENOMEM;
	}
	dev->dma_virtual_addr = dev->dma_buffer;
	DBG(1,"DMA addr=%p, phys addr%x\n",dev->dma_buffer,dev->dma_phys_addr);

	// Initialize the memory mapped region
	phys_base = pci_resource_start(pci_dev,0);
	if (phys_base == 0) {
		WARN("Failed to initialize PCI memory\n");
		return -ENXIO;	   
	}
   
	status = pci_request_regions(pci_dev, "unicorn_pci");
	if (status) {
		WARN("Failed to initialize PCI memory,status=%d\n",status);
		return status;
	}

	dev->mem_size = pci_resource_len(pci_dev,0);
	dev->mem = ioremap(phys_base,dev->mem_size);
	if (dev->mem == NULL) {
		WARN("Failed to map PCI memory\n");
		return -ENXIO;	   
	}
	DBG(1,"PCI base phys addr=%lx,addr=%p,size=%ld\n",
	    phys_base,dev->mem,dev->mem_size);
	
	// Initialize and connect the interrupt
	status = request_irq(pci_dev->irq, unicorn_isr, SA_SHIRQ, "unicorn_pci", dev);
	if (status) {
		WARN("Failed to connect Interrupt,status=%d\n",status);
		return status;
	}

	// Initialize the R/W
	send_atm_init(dev,dev->dma_virtual_addr+offsetof(DMA_MEMORY,AtmUsBuf),
		      PCI_US_BUF_SIZE*sizeof(DWORD));
	
	recv_atm_init(dev,dev->dma_virtual_addr+offsetof(DMA_MEMORY,AtmDsBuf),
		      PCI_DS_BUF_SIZE*sizeof(DWORD));
	
   	// Initialize OBC objects
	init_MUTEX(&dev->obc_lock);
 	if ((xsm_create("OBC ",0,0,&dev->obc_sem)) != SUCCESS) {
		return -ENOMEM;
	}
	
	// Start the Hardware Device
	start_hardware(dev);
	dev->started = TRUE;
	adsl_status = ADSL_STATUS_NOLINK;
	
	return 0;
}

//----------------------------------------------------------------------
//	Stop the PCI device
//----------------------------------------------------------------------
void stop_device(struct unicorn_dev *dev)
{
	struct pci_dev *pci_dev;

	DBG(1,"\n");

	if ((pci_dev = dev->pci_dev) == NULL) {
		return;
	}

	adsl_status = ADSL_STATUS_NOHARDWARE;

	// Stop receiving and transmitting
	send_atm_exit(dev);
	recv_atm_exit(dev);

	// Shutdown the Modem Software
	if (dev->started)
		if (dev->msw_started) {
		  rapi_lock();
		  msw_stop();
		  msw_exit();
		  rapi_unlock();
		  dev->msw_started = FALSE;
		}

	// Release RAPI Objects
	dev->stopping = TRUE;                //  PC Crash Fix: 01/31/2001

	// disable any interrupt source in the device
	if (dev->started) {
		stop_hardware(dev);
		// Release interrupt
		free_irq(dev->pci_dev->irq, dev);
		// Device is not Started anymore
		dev->started = FALSE;
	}

	// Disable memory mapping and busmastering
	pci_disable_device(pci_dev);

	// Release DMA buffer
	if (dev->dma_buffer) {
		pci_free_consistent(pci_dev,sizeof(DMA_MEMORY),dev->dma_buffer,dev->dma_phys_addr);
		dev->dma_buffer = NULL;
	}
	dev->dma_phys_addr = 0;
	dev->dma_virtual_addr = NULL;
	
	if (dev->mem) {
		iounmap((void *)dev->mem);
		dev->mem = NULL;
	}
	pci_release_regions(pci_dev);
	dev->mem_size = 0L;
}


//----------------------------------------------------------------------
//	CheckObcBuffer
//----------------------------------------------------------------------
BOOLEAN CheckObcBuffer (struct unicorn_dev *dev,DWORD *buf,UINT n)
{
	PBYTE p;

	if ((DWORD)buf & 3) return FALSE;
	p = (PBYTE)buf + n*sizeof(DWORD);
	if (p > dev->dma_virtual_addr+offsetof(DMA_MEMORY,AtmUsBuf)) return FALSE;
	return TRUE;
}

//----------------------------------------------------------------------
//	ObcBufHardAddr
//----------------------------------------------------------------------
static inline DWORD ObcBufHardAddr (struct unicorn_dev *dev,DWORD *buf)
{
	return dev->dma_phys_addr + ((PBYTE)buf - dev->dma_virtual_addr);
}


//----------------------------------------------------------------------
//	Waits for OBC command complete
//----------------------------------------------------------------------
static ST_STATUS WaitForObcCmdComplete(struct unicorn_dev *dev)
{
	ST_STATUS status;
	
	// wait for semaphore to be free
	status = xsm_p(dev->obc_sem, 0, OBC_CMD_TIMEOUT);
	if (status != SUCCESS) {
		WARN("wait for obc failed (timed out)\n");
		clear_bit(0, &dev->obc_pending);
	}
	return status;
}

//----------------------------------------------------------------------
//    ObcReset
//----------------------------------------------------------------------
static void ObcReset(struct unicorn_dev *dev)
{
	// Force STOP ACCESS in status register and OBC FIFO clear pulse
	DWORD data;

	WARN("Resetting OBC...\n");

	data = PCI_GET_DWORD(dev->mem,PR_STATUS);
	PCI_PUT_DWORD((data&~3)|0x200,dev->mem,PR_STATUS); 
}


//======================================================================
//	HAL API ENTRY POINTS FOR KERNEL MODE MSW
//======================================================================

//----------------------------------------------------------------------
//	PCI_init
//----------------------------------------------------------------------
ST_STATUS PCI_init(
	DWORD **CMDptrW1,DWORD **CMDptrW2,DWORD **CMDptrRd,
	WORD **ITABLEptr,WORD **IMASKptr
	)

{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	PBYTE p = dev->dma_virtual_addr;

	DBG(1,"dma_virtual_addr=%p\n",p);

	if (!p) return FAILURE;
	*CMDptrW1  = (PDWORD)(p + offsetof(DMA_MEMORY,CmdBufW1));
	*CMDptrW2  = (PDWORD)(p + offsetof(DMA_MEMORY,CmdBufW2));
	*CMDptrRd  = (PDWORD)(p + offsetof(DMA_MEMORY,CmdBufRd));
	*ITABLEptr = tosca_softITABLE;
	*IMASKptr  = tosca_softITABLE + 14;
	return SUCCESS;
}

//----------------------------------------------------------------------
//	PCI_directWrite
//----------------------------------------------------------------------
ST_STATUS PCI_directWrite (WORD addr,DWORD data)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	int special;

	if (addr > dev->mem_size-4) {
		WARN("invalid address %x\n",addr);
		return FAILURE;
	}

	OBC_LOCK(&dev->obc_lock);

	DBG(PCI_D,"addr=%04x,data=%08lx\n",addr,data);
	special = (addr == PR_STATUS) && (data == 0xe835);
	if (special) {
		set_bit(0, &dev->obc_pending);
	}
	PCI_PUT_DWORD(data,dev->mem,addr);

	if (special) {
		// Wait for RBigi MessageWrite to complete
		if (WaitForObcCmdComplete(dev) != SUCCESS) return FAILURE;
	}

	OBC_UNLOCK(&dev->obc_lock);
	return SUCCESS;
}

//----------------------------------------------------------------------
//	PCI_directRead
//----------------------------------------------------------------------
ST_STATUS PCI_directRead(WORD addr,DWORD *data)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;

	if (addr > dev->mem_size-4) {
		WARN("invalid address %x\n",addr);
		return FAILURE;
	}

	OBC_LOCK(&dev->obc_lock);

	*data = PCI_GET_DWORD(dev->mem,addr);
	DBG(PCI_D,"addr=%04x,data=%08lx\n",addr,*data);

	OBC_UNLOCK(&dev->obc_lock);
	return SUCCESS;
}

#define MAX_RETRIES 2

//----------------------------------------------------------------------
//	PCI_SlaveWriteDirect
//----------------------------------------------------------------------
ST_STATUS PCI_SlaveWriteDirect (T_SlaveDirData *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;

	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"idata=%08lx,iaddr=%08lx,status=%08lx\n",p->idata,p->iaddr,p->status);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD((WORD)p->idata,dev->mem,PR_IDATA); 
		PCI_PUT_DWORD((WORD)p->iaddr,dev->mem,PR_IADR); 
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS); 

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;
		}
	}
	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//	PCI_SlaveReadDirect
//----------------------------------------------------------------------
ST_STATUS PCI_SlaveReadDirect (T_SlaveDirData *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;
	
	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"iaddr=%08lx,status=%08lx\n",p->iaddr,p->status);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD((WORD)p->iaddr,dev->mem,PR_IADR); 
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS); 

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;
		}
	}
	p->idata = (WORD)PCI_GET_DWORD(dev->mem,PR_IDATA);
	DBG(PCI_D,"idata=%08lx\n",p->idata);

	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//	PCI_SlaveRMWrite
//----------------------------------------------------------------------
ST_STATUS PCI_SlaveRMWrite (T_SlaveMaskData *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;

	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"idata=%08lx,iaddr=%08lx,mask=%08lx,status=%08lx\n",
	    p->idata,p->iaddr,p->mask,p->status);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD((WORD)p->idata,dev->mem,PR_IDATA);  
		PCI_PUT_DWORD((WORD)p->iaddr,dev->mem,PR_IADR);   
		PCI_PUT_DWORD((WORD)p->mask,dev->mem,PR_IRMW);   
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS); 

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;

		}
	}
	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//	PCI_SlaveWriteIndirect
//----------------------------------------------------------------------
ST_STATUS PCI_SlaveWriteIndirect (T_SlaveIndData *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;
	
	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"idata[0]=%08lx,idata[1]=%08lx,idata[2]=%08lx,icntrl=%08lx,iaddr=%08lx,status=%08lx\n",
	    p->idata[0],p->idata[1],p->idata[2],p->icntrl,p->iaddr,p->status);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD((WORD)p->idata[0],dev->mem,PR_IDATA_1); 
		PCI_PUT_DWORD((WORD)p->idata[1],dev->mem,PR_IDATA_2); 
		PCI_PUT_DWORD((WORD)p->idata[2],dev->mem,PR_IDATA_3); 
		PCI_PUT_DWORD((WORD)p->icntrl,dev->mem,PR_IDATA);  
		PCI_PUT_DWORD((WORD)p->iaddr,dev->mem,PR_IADR);   
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS); 

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;
		}
	}
	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//	PCI_SlaveReadIndirect
//----------------------------------------------------------------------
ST_STATUS PCI_SlaveReadIndirect (T_SlaveIndData *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;
	
	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"icntrl=%08lx,iaddr=%08lx,status=%08lx\n",
	    p->icntrl,p->iaddr,p->status);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD((WORD)p->icntrl,dev->mem,PR_IDATA);  
		PCI_PUT_DWORD((WORD)p->iaddr,dev->mem,PR_IADR);   
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS); 

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;
		}	
	}
	p->idata[0] = (WORD)PCI_GET_DWORD(dev->mem,PR_IDATA_1);
	p->idata[1] = (WORD)PCI_GET_DWORD(dev->mem,PR_IDATA_2);
	p->idata[2] = (WORD)PCI_GET_DWORD(dev->mem,PR_IDATA_3);
	DBG(PCI_D,"idata[0]=%08lx,idata[1]=%08lx,idata[2]=%08lx\n",
	    p->idata[0],p->idata[1],p->idata[2]);

	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//	PCI_MasterWriteDirBlock
//----------------------------------------------------------------------
ST_STATUS PCI_MasterWriteDirBlock (T_MasterWBlock *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;
	
	if (!CheckObcBuffer(dev,p->cmdBuff,p->wrSize)) return FAILURE;
	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"cmdBuff=%p,wrSize=%08lx,status=%08lx\n",
	    p->cmdBuff,p->wrSize,p->status);

 	PCI_SWAP_BUF(p->cmdBuff,p->wrSize);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD(ObcBufHardAddr(dev,p->cmdBuff),dev->mem,PR_WR_PTR);    
		PCI_PUT_DWORD(p->wrSize,dev->mem,PR_WR_SIZE);   
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS);    
		PCI_PUT_DWORD(LACFW_EDMA_WROBC,dev->mem,PR_LACFW_SET); 

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;
		}
	}
	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//	PCI_MasterReadDirBlock
//----------------------------------------------------------------------
ST_STATUS PCI_MasterReadDirBlock (T_MasterRBlock *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;
	
	if (!CheckObcBuffer(dev,p->cmdBuff,p->wrSize)) return FAILURE;
	if (!CheckObcBuffer(dev,p->resBuff,p->rdSize)) return FAILURE;	

	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"cmdBuff=%p,resBuff=%p,wrSize=%08lx,rdSize=%08lx,status=%08lx\n",
	    p->cmdBuff,p->resBuff,p->wrSize,p->rdSize,p->status);
	
 	PCI_SWAP_BUF(p->cmdBuff,p->wrSize);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD(ObcBufHardAddr(dev,p->cmdBuff),dev->mem,PR_WR_PTR);    
		PCI_PUT_DWORD(p->wrSize,dev->mem,PR_WR_SIZE);   
		PCI_PUT_DWORD(ObcBufHardAddr(dev,p->resBuff),dev->mem,PR_RD_PTR);    
		PCI_PUT_DWORD(p->rdSize,dev->mem,PR_RD_SIZE);   
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS);    
		PCI_PUT_DWORD((LACFW_EDMA_WROBC+LACFW_EDMA_RDOBC),dev->mem,PR_LACFW_SET); 

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;
		}
	}
 	PCI_SWAP_BUF(p->resBuff,p->rdSize);

	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//	PCI_MasterWriteIndBlock
//----------------------------------------------------------------------
ST_STATUS PCI_MasterWriteIndBlock (T_MasterWBlock *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;
	
	if (!CheckObcBuffer(dev,p->cmdBuff,p->wrSize)) return FAILURE;

	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);


	DBG(PCI_D,"cmdBuff=%p,wrSize=%08lx,status=%08lx\n",
	    p->cmdBuff,p->wrSize,p->status);

	PCI_SWAP_BUF(p->cmdBuff,p->wrSize);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD(ObcBufHardAddr(dev,p->cmdBuff),dev->mem,PR_WR_PTR);    
		PCI_PUT_DWORD(p->wrSize,dev->mem,PR_WR_SIZE);   
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS);    
		PCI_PUT_DWORD(LACFW_EDMA_WROBC,dev->mem,PR_LACFW_SET);   

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;
		}
	}
	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//	PCI_MasterReadIndBlock
//----------------------------------------------------------------------
ST_STATUS PCI_MasterReadIndBlock (T_MasterRBlock *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;
	
	if (!CheckObcBuffer(dev,p->cmdBuff,p->wrSize)) return FAILURE;
	if (!CheckObcBuffer(dev,p->resBuff,p->rdSize)) return FAILURE;

	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"cmdBuff=%p,resBuff=%p,wrSize=%08lx,rdSize=%08lx,status=%08lx\n",
	    p->cmdBuff,p->resBuff,p->wrSize,p->rdSize,p->status);

	PCI_SWAP_BUF(p->cmdBuff,p->wrSize);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD(ObcBufHardAddr(dev,p->cmdBuff),dev->mem,PR_WR_PTR);    
		PCI_PUT_DWORD(p->wrSize,dev->mem,PR_WR_SIZE);   
		PCI_PUT_DWORD(ObcBufHardAddr(dev,p->resBuff),dev->mem,PR_RD_PTR);    
		PCI_PUT_DWORD(p->rdSize,dev->mem,PR_RD_SIZE);   
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS);    
		PCI_PUT_DWORD((LACFW_EDMA_WROBC+LACFW_EDMA_RDOBC),dev->mem,PR_LACFW_SET); 

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;
		}
	}
 	PCI_SWAP_BUF(p->resBuff,p->rdSize);

	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//      PCI_MasterWriteDirBurst
//----------------------------------------------------------------------
ST_STATUS PCI_MasterWriteDirBurst (T_MasterWBurst *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;

	if (!CheckObcBuffer(dev,p->cmdBuff,p->wrSize)) return FAILURE;

	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"cmdBuff=%p,baseAddr=%08lx,wrSize=%08lx,status=%08lx\n",
	    p->cmdBuff,p->baseAddr,p->wrSize,p->status);

	PCI_SWAP_BUF(p->cmdBuff,p->wrSize);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD(ObcBufHardAddr(dev,p->cmdBuff),dev->mem,PR_WR_PTR);     
		PCI_PUT_DWORD(p->wrSize,dev->mem,PR_WR_SIZE);    
		PCI_PUT_DWORD((WORD)p->baseAddr,dev->mem,PR_IADR_BURST); 
		PCI_PUT_DWORD((WORD)p->wrSize,dev->mem,PR_SIZE_BURST); 
		PCI_PUT_DWORD((WORD)0x8000,dev->mem,PR_IMASKM);     
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS);     
		PCI_PUT_DWORD(LACFW_EDMA_WROBC,dev->mem,PR_LACFW_SET);  

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;
		}
	}
	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//	PCI_MasterReadDirBurst
//----------------------------------------------------------------------
ST_STATUS PCI_MasterReadDirBurst (T_MasterRBurst *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;

	if (!CheckObcBuffer(dev,p->resBuff,p->rdSize)) return FAILURE;

	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"resBuff=%p,baseAddr=%08lx,wrSize=%08lx,rdSize=%08lx,status=%08lx\n",
	    p->resBuff,p->baseAddr,p->wrSize,p->rdSize,p->status);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD(ObcBufHardAddr(dev,p->resBuff),dev->mem,PR_RD_PTR);     
		PCI_PUT_DWORD(p->rdSize,dev->mem,PR_RD_SIZE);    
		PCI_PUT_DWORD((WORD)p->baseAddr,dev->mem,PR_IADR_BURST); 
		PCI_PUT_DWORD((WORD)p->wrSize,dev->mem,PR_SIZE_BURST); 
		PCI_PUT_DWORD((WORD)0x8000,dev->mem,PR_IMASKM);     
		PCI_PUT_DWORD(LACFW_EDMA_RDOBC,dev->mem,PR_LACFW_SET);  
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS);     

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;
		}
	}
 	PCI_SWAP_BUF(p->resBuff,p->rdSize);

	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//	PCI_MasterWriteIndBurst
//----------------------------------------------------------------------
ST_STATUS PCI_MasterWriteIndBurst (T_MasterWBurst *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;

	if (!CheckObcBuffer(dev,p->cmdBuff,p->wrSize)) return FAILURE;

	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"cmdBuff=%p,baseAddr=%08lx,wrSize=%08lx,status=%08lx\n",
	    p->cmdBuff,p->baseAddr,p->wrSize,p->status);

	PCI_SWAP_BUF(p->cmdBuff,p->wrSize);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD(ObcBufHardAddr(dev,p->cmdBuff),dev->mem,PR_WR_PTR);     
		PCI_PUT_DWORD(p->wrSize,dev->mem,PR_WR_SIZE);    
		PCI_PUT_DWORD((WORD)p->baseAddr,dev->mem,PR_IADR_BURST); 
		PCI_PUT_DWORD((WORD)(p->wrSize/3),dev->mem,PR_SIZE_BURST); 
		PCI_PUT_DWORD((WORD)0x8000,dev->mem,PR_IMASKM);     
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS);     
		PCI_PUT_DWORD(LACFW_EDMA_WROBC,dev->mem,PR_LACFW_SET);  

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;
		}
	}
	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//	PCI_MasterReadIndBurst
//----------------------------------------------------------------------
ST_STATUS PCI_MasterReadIndBurst (T_MasterRBurst *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;

	if (!CheckObcBuffer(dev,p->resBuff,p->rdSize)) return FAILURE;

	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"resBuff=%p,baseAddr=%08lx,wrSize=%08lx,rdSize=%08lx,status=%08lx\n",
	    p->resBuff,p->baseAddr,p->wrSize,p->rdSize,p->status);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD(ObcBufHardAddr(dev,p->resBuff),dev->mem,PR_RD_PTR);     
		PCI_PUT_DWORD(p->rdSize,dev->mem,PR_RD_SIZE);    
		PCI_PUT_DWORD((WORD)p->baseAddr,dev->mem,PR_IADR_BURST); 
		PCI_PUT_DWORD((WORD)p->wrSize,dev->mem,PR_SIZE_BURST); 
		PCI_PUT_DWORD((WORD)0x8000,dev->mem,PR_IMASKM);     
		PCI_PUT_DWORD(LACFW_EDMA_RDOBC,dev->mem,PR_LACFW_SET);  
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS);     

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;
		}
	}
 	PCI_SWAP_BUF(p->resBuff,p->rdSize);

	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//	PCI_MessageRead
//----------------------------------------------------------------------
ST_STATUS PCI_MessageRead (T_Msg *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;

	if (!CheckObcBuffer(dev,p->cmdBuff,p->reqSize)) return FAILURE;

	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"cmdBuff=%p,SocAddr=%08lx,SocCntrl=%08lx,mask=%08lx,reqSize=%08lx,status=%08lx\n",
	    p->cmdBuff,p->SocAddr,p->SocCntrl,p->mask,p->reqSize,p->status);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD(ObcBufHardAddr(dev,p->cmdBuff),dev->mem,PR_RD_PTR);     
		PCI_PUT_DWORD(p->reqSize,dev->mem,PR_RD_SIZE);    
		PCI_PUT_DWORD((WORD)p->SocAddr,dev->mem,PR_IADR_MSG);   
		PCI_PUT_DWORD((WORD)p->SocCntrl,dev->mem,PR_IADR_CHK);   
		PCI_PUT_DWORD((WORD)p->mask,dev->mem,PR_IMASKM);     
		PCI_PUT_DWORD((WORD)p->reqSize,dev->mem,PR_SIZE_BURST); 
		PCI_PUT_DWORD(LACFW_EDMA_RDOBC,dev->mem,PR_LACFW_SET);  
		PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS);     

		if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
			ObcReset(dev);
		} else {
			break;
		}
	}
	p->exeSize = PCI_GET_DWORD(dev->mem,PR_RD_MOVED);
	PCI_SWAP_BUF(p->cmdBuff,p->reqSize);

	DBG(PCI_D,"exeSize=%08lx\n",p->exeSize);

	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

//----------------------------------------------------------------------
//	PCI_MessageWrite
//----------------------------------------------------------------------
ST_STATUS PCI_MessageWrite (T_Msg *p)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	ST_STATUS status=FAILURE;
	int retry;

	if (!CheckObcBuffer(dev,p->cmdBuff,p->reqSize)) return FAILURE;

	OBC_LOCK(&dev->obc_lock);
	set_bit(0, &dev->obc_pending);

	DBG(PCI_D,"cmdBuff=%p,SocAddr=%08lx,SocCntrl=%08lx,mask=%08lx,reqSize=%08lx,status=%08lx\n",
	    p->cmdBuff,p->SocAddr,p->SocCntrl,p->mask,p->reqSize,p->status);

	PCI_SWAP_BUF(p->cmdBuff,p->reqSize);

	for (retry=0; retry < MAX_RETRIES; retry++) {

		PCI_PUT_DWORD(ObcBufHardAddr(dev,p->cmdBuff),dev->mem,PR_WR_PTR);     
		PCI_PUT_DWORD(p->reqSize,dev->mem,PR_WR_SIZE);    
		PCI_PUT_DWORD((WORD)p->SocAddr,dev->mem,PR_IADR_MSG);   
		PCI_PUT_DWORD((WORD)p->SocCntrl,dev->mem,PR_IADR_CHK);   
		PCI_PUT_DWORD((WORD)p->mask,dev->mem,PR_IMASKM);     
		PCI_PUT_DWORD((WORD)p->reqSize,dev->mem,PR_SIZE_BURST); 

		//STM bigi
		//write status for all msg except RBigi long packet
		if(p->reqSize < 128) {
			PCI_PUT_DWORD((WORD)p->status,dev->mem,PR_STATUS);     
			DBG(PCI_D,"status=%08lx\n",p->status);
		}
		PCI_PUT_DWORD(LACFW_EDMA_WROBC,dev->mem,PR_LACFW_SET);  
	
		// STM 25/10/2000
		if(p->reqSize < 128) {
			if ((status = WaitForObcCmdComplete(dev)) != SUCCESS) {
				ObcReset(dev);
			} else {
				break;
			}
			p->exeSize = PCI_GET_DWORD(dev->mem,PR_WR_MOVED);
			DBG(PCI_D,"exeSize=%08lx\n",p->exeSize);
		} else {
			status = SUCCESS;
			break;
		}
	}

	OBC_UNLOCK(&dev->obc_lock);
	return status;
}

#ifdef NOT_USED
//----------------------------------------------------------------------
//	PCI_ReadConfigHeader
//----------------------------------------------------------------------
ST_STATUS PCI_ReadConfigHeader (PVOID Buffer,ULONG Offset,ULONG Count)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	unsigned char *val=Buffer;

	if (Offset+Count > 64) {
		WARN("failed, invalid argument\n");
		return FAILURE;
	}
	DBG(PCI_D,"buffer=%p,offset=%08lx,count=%08lx\n",
	    Buffer,Offset,Count);

	while (Count--) {
		if (pci_read_config_byte(dev->pci_dev, Offset++, val++)) {
			WARN("pci_read_config_byte failed\n");
			return FAILURE;
		}
	}
	return SUCCESS;
}

//----------------------------------------------------------------------
//	PCI_WriteConfigHeader
//----------------------------------------------------------------------
ST_STATUS PCI_WriteConfigHeader (PVOID Buffer,ULONG Offset,ULONG Count)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	unsigned char *val=Buffer;

	if (Offset+Count > 64) {
		WARN("failed, invalid argument\n");
		return FAILURE;
	}
	DBG(PCI_D,"buffer=%p,offset=%08lx,count=%08lx\n",
	    Buffer,Offset,Count);

	while (Count--) {
		if (pci_write_config_byte(dev->pci_dev, Offset++, *val++)) {
			WARN("pci_write_config_byte failed\n");
			return FAILURE;
		}
	}	
	return SUCCESS;
}

//----------------------------------------------------------------------
//	PCI_ReadDeviceSpecificConfig
//----------------------------------------------------------------------
ST_STATUS PCI_ReadDeviceSpecificConfig (PVOID Buffer,ULONG Offset,ULONG Count)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	unsigned char *val=Buffer;

	if (Offset+Count > 192) {
		WARN("failed, invalid argument\n");
		return FAILURE;
	}
	DBG(PCI_D,"buffer=%p,offset=%08lx,count=%08lx\n",
	    Buffer,Offset,Count);

	while (Count--) {
		if (pci_read_config_byte(dev->pci_dev, Offset++, val++)) {
			WARN("pci_read_config_byte failed\n");
			return FAILURE;
		}
		Offset++;
		val++;
	}	
	return SUCCESS;
}

//----------------------------------------------------------------------
//	PCI_WriteDeviceSpecificConfig
//----------------------------------------------------------------------
ST_STATUS PCI_WriteDeviceSpecificConfig (PVOID Buffer,ULONG Offset,ULONG Count)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;
	unsigned char *val=Buffer;

	if (Offset+Count > 192) {
		WARN("failed, invalid argument\n");
		return FAILURE;
	}
	DBG(PCI_D,"buffer=%p,offset=%08lx,count=%08lx\n",
	    Buffer,Offset,Count);

	while (Count--) {
		if (pci_write_config_byte(dev->pci_dev, Offset++, *val++)) {
			WARN("pci_write_config_byte failed\n");
			return FAILURE;
		}

	}	
	return SUCCESS;
}
#endif

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
		WARN("ATM Tx I/O error\n");
		return NULL;
	}

	t = send_atm->turn_write;
	if (send_atm->lens[t] > 0) return NULL;

	n = send_atm->maxlen - send_atm->bufofs;
	if (n < CELL_LENGTH) return NULL;

	p = send_atm->bufs[t] + send_atm->bufofs;
	
	// next cell
	send_atm->bufofs += CELL_LENGTH;
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
	unsigned long flags;

	DBG(RW_D,"\n");

	spin_lock_irqsave(&send_atm->lock, flags);
	
	t = send_atm->turn_write;
	n = send_atm->bufofs;
	if (n > 0) {
		send_atm->lens[t] = n;
		send_atm->turn_write = (t+1)&(ATM_WRITES-1);
		send_atm->bufofs = 0;
		set_bit(0,&send_atm->busy[t]);
		send_atm->send_pending++;
		spin_unlock_irqrestore(&send_atm->lock, flags);
		StartAtmUsXfer(dev,t,send_atm->bufs[t],n);
		return n;
	}
	spin_unlock_irqrestore(&send_atm->lock, flags);
	return 0;
}

//----------------------------------------------------------------------
// unicorn_rcv_getcell:	
// Returns a pointer to the next cell header in the receive buffer
//----------------------------------------------------------------------
unsigned char *unicorn_rcv_getcell(struct unicorn_dev *dev)
{
	struct recv_atm *recv_atm = &dev->recv_atm;
	int t;
	PBYTE p;
	PDWORD pf;
	unsigned long flags;

	spin_lock_irqsave(&recv_atm->lock, flags);

	t = recv_atm->turn_read;
	p = recv_atm->bufs[t] + recv_atm->bufofs;
	pf = (PDWORD)(p+PCI_CELL_LENGTH-4);

	if (*pf) {
		*pf = 0;
		recv_atm->bufofs += PCI_CELL_LENGTH;
		if (recv_atm->bufofs == recv_atm->maxlen) {
			recv_atm->turn_read = (t+1)&(ATM_READS-1);
			recv_atm->bufofs = 0;
		}
		recv_atm->cell_count++;
	} else {
		p = NULL;
	}

	spin_unlock_irqrestore(&recv_atm->lock, flags);
	return p;
}

//----------------------------------------------------------------------
//	Modem Software Control functions
//----------------------------------------------------------------------
int unicorn_msw_control(struct unicorn_dev *dev,T_MswCtrl *ctrl)
{
	static unsigned long old_report = 0L;
	ctrl->retcode = -1;

	DBG(RW_D,"code=%lx,subcode=%lx,buffer=%p,length=%ld\n",
	    ctrl->code,ctrl->subcode,ctrl->buffer,ctrl->length);

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
		*us_rate = (adsl_us_cellrate * ATM_CELL_SIZE*8UL)/1000UL;
		*ds_rate = (adsl_ds_cellrate * ATM_CELL_SIZE*8UL)/1000UL;
		status = 0;
	}
	return status;
}

/* module parameters for MSW */
MODULE_PARM(ActivationMode, "i");
//MODULE_PARM(ActivationTaskTimeout, "i");
MODULE_PARM(ActTimeout, "i");
MODULE_PARM(AutoActivation, "i");
//MODULE_PARM(BreakOnEntry, "i");
MODULE_PARM(DownstreamRate, "i");
MODULE_PARM(eocTrace, "i");
//MODULE_PARM(ExchangeDelay, "i");
MODULE_PARM(FmPollingRate, "i");
//MODULE_PARM(g_RefGain, "i");
MODULE_PARM(g_TeqMode, "i");
MODULE_PARM(InitTimeout, "i");
MODULE_PARM(Interoperability, "i");
MODULE_PARM(LCD_Trig, "i");
MODULE_PARM(LOS_LOF_Trig, "i");
MODULE_PARM(LoopbackMode, "i");
MODULE_PARM(MswDebugLevel, "i");
MODULE_PARM(RetryTime, "i");
MODULE_PARM(setINITIALDAC, "i");
//MODULE_PARM(TrainingDelay, "i");
//MODULE_PARM(TruncateMode, "i");
MODULE_PARM(useAFE, "i");
//MODULE_PARM(useRFC019v, "i");
//MODULE_PARM(useRFC029v, "i");
//MODULE_PARM(useRFC033v, "i");
//MODULE_PARM(useRFC040v, "i");
MODULE_PARM(useRFC041v, "i");
//MODULE_PARM(useRFCFixedRate, "i");
//MODULE_PARM(useVCXO, "i");
//MODULE_PARM(_no_TS652, "i");
#if DEBUG
MODULE_PARM(DebugLevel, "i");
#endif

static int __init
unicorn_pci_init(void)
{
	int status;
	struct unicorn_dev *dev = &unicorn_pci_dev;
	struct pci_dev *pci_dev;

	pci_dev = find_unicorn();
	if (!pci_dev) {
		WARN("no PCI cards found\n");
		return -ENXIO;
	}
	// guess type of AFE
	if (!useAFE) {
		if (pci_dev->subsystem_device == 0xA888) {
			useAFE = 70134;
		} else if (pci_dev->subsystem_device == 0xA889) {
			useAFE = 20174;
		} else {
			useAFE = 20174;
		}
		
	}
	INFO("AFE %ld\n",useAFE);

	pilotRealloc=1;
	_newCToneDetection_ = 1;
	setINITIALDAC = 93;
	if (useAFE == 70134) { 
		_no_TS652 = 0;
		g_RefGain=28;
	} else if (useAFE == 70136) {
		_no_TS652 = 1;
		g_RefGain=26;
	} else if (useAFE == 20174) {
		_no_TS652 = 1;
		g_RefGain=20;
	} else {
		WARN("unknown AFE %ld\n",useAFE);
	}

	useVCXO = 0;  

	useAOCChannel = 2;
	DownstreamRate = 8128;

	pilotRootPowerWorkAround = TRUE;
	_gi_step_ = 1;
	_teq_new_delay_ = 1;
	//TNumberOfCarrier;
	highCarrierOff = 240;
	decreaseHighCarrier = 12;
	_boostPowerGdmt_ = 1;
	useRFC019v = 0;
	useRFC029v = 8000;
	useRFC040v = 0;
	useRFCFixedRate = 1;
	TrainingDelay = 120;
	ExchangeDelay = 20;
	_trellisact_ = 1;
	pvo_pembdpllpolarity=1;

	Interoperability=0;

	INFO("v %d.%d.%d, " __TIME__ " " __DATE__"\n",
	     (VERS>>8)&0xf,(VERS>>4)&0xf,VERS&0xf);

	INFO("MSW parameters: \nActivationMode=%lx\nActTimeout=%ld\nAutoActivation=%ld\nDebugLevel=%ld\nDownstreamRate=%ld\n",
	     ActivationMode,ActTimeout,AutoActivation,DebugLevel,DownstreamRate);
	INFO("ExchangeDelay=%ld\nFmPollingRate=%ld\ng_RefGain=%ld\ng_Teqmode=%x\nInitTimeout=%ld\nInteroperability=%ld\n",
	     ExchangeDelay,FmPollingRate,g_RefGain,g_TeqMode,InitTimeout,Interoperability);
	INFO("LCD_Trig=%ld\nLOS_LOF_Trig=%ld\nLoopbackMode=%ld\nMswDebugLevel=%ld\nRetryTime=%ld\nTrainingDelay=%ld\n",
	     LCD_Trig,LOS_LOF_Trig,LoopbackMode,MswDebugLevel,RetryTime,TrainingDelay);
	
	INFO("useRFC019v=%ld\nuseRFC029v=%ld\nuseRFC040v=%ld\nuseRFC041v=%ld\nsetINITIALDAC=%ld\n",
	     useRFC019v,useRFC029v,useRFC040v,useRFC041v,setINITIALDAC);
	INFO("useRFCFixedRate=%ld\nuseVCXO=%ld\n_no_TS652=%ld\n",
	     useRFCFixedRate,useVCXO,_no_TS652);
	
	INFO("driver parameters: DebugLevel=%ld\n",
	    DebugLevel);

	// Initialize RAPI
	if ((status = rapi_init()) != 0) {
		WARN("inititalization of RAPI failed\n");
		return -ENOMEM;
	}

	// Initialize PCI card
	if ((status = start_device(dev,pci_dev)) != 0) {
		WARN("inititalization of PCI failed\n");
		return status;
	}

	// Tell ATM driver we are initialized
	if ((status = unicorn_attach(&unicorn_pci_entrypoints)) != 0) {
		WARN("inititalization of ATM driver failed\n");
		return status;
	}

	// Start the Modem Software
	if (ActivationMode) {
	  rapi_lock();
	  msw_init(ActivationMode);
	  dev->msw_started = TRUE;
	  xtm_wkafter(100);
	  if (AutoActivation) {
	    msw_start();
	  }
	  rapi_unlock();
	}


	// If loopback mode, no need for line activation to enable ATM
	if (LoopbackMode) {
		setShowtime();
		setAtmRate(1000,1000);
	}
	return 0;
}	

static void __exit
unicorn_pci_cleanup(void)
{
	struct unicorn_dev *dev = &unicorn_pci_dev;

	DBG(1,"\n");

	unicorn_detach();

	stop_device(dev);

	rapi_exit();

	INFO("driver removed\n");
}

module_init(unicorn_pci_init);
module_exit(unicorn_pci_cleanup);

static struct pci_device_id unicorn_pci_tbl[] __initdata = {
	{PCI_VENDOR_ID_ST, PCI_DEVICE_ID_UNICORN, PCI_ANY_ID, PCI_ANY_ID},
	{ }				/* Terminating entry */
};
MODULE_DEVICE_TABLE(pci, unicorn_pci_tbl);

