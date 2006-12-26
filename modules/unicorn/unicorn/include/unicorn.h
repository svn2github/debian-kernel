#ifndef __unicorn_h__
#define __unicorn_h__

#define MAX_DEVICES 1

#define UNICORN_ATM_NAME "atm0"
#define UNICORN_ETH_NAME "dsl0"

#define ATM_MSW_CTL _IOW('a',ATMIOC_PHYPRV+0,struct atmif_sioc)
#define ETH_MSW_CTL SIOCDEVPRIVATE


typedef struct {
	unsigned long code;
	unsigned long subcode;
	unsigned long retcode;
	void *buffer;
	unsigned long length;
} T_MswCtrl;

typedef struct {
	unsigned long State;
	unsigned long Report;
	unsigned long Failure;
	unsigned long TimeCnctd;
	unsigned long long RcvdCells;
	unsigned long long SentCells;
} T_StateInfo;

#define ATM_OAM_F4	0xF4
#define ATM_OAM_F5	0xF5 

typedef struct {
	int type;
	int vpi;
	int vci;
} T_atm_channel;

typedef struct {
	unsigned long tx_LB;
	unsigned long rx_AIS;
	unsigned long rx_RDI;
	unsigned long rx_CC;
	unsigned long rx_ne_LB;
	unsigned long rx_fe_LB;
} T_oam_stats;

// holds the global ADSL status returned to the ATM driver
typedef enum {
	ADSL_STATUS_NOHARDWARE,		// No hardware detected
	ADSL_STATUS_NOLINK,		// Hardware detected / No link
	ADSL_STATUS_ATMREADY		// Hardware detected / On line
} ADSL_STATUS;

// code 0-31 for MSW
#define	MSW_CTL_INIT		0	// For msw_init()
#define	MSW_CTL_EXIT		1	// For msw_exit()
#define	MSW_CTL_START		2	// For msw_start()
#define	MSW_CTL_STOP		3	// For msw_stop()
#define	MSW_CTL_SET_CONFIG	4	// For AMSW_ANT_getModemConfiguration()
#define	MSW_CTL_GET_CONFIG	5	// For AMSW_ANT_setModemConfiguration()
#define	MSW_CTL_REQ_CHANGE	6	// For AMSW_ANT_requestModemStateChange
#define	MSW_CTL_GET_DATA	7	// For AMSW_ANT_getData()
#define	MSW_CTL_GET_STATE	8	// For AMSW_ANT_getModemState()
#define	MSW_CTL_DYING_GASP	9	// For AMSW_ANT_dyingGasp()
#define	MSW_CTL_WAIT_EVENT	10	// Wait for MSW event
#define	MSW_CTL_CANCEL_WAIT	11	// Cancel wait event ioctl
#define	MSW_CTL_VERSION_INFO	12	// Get the driver version
#define	MSW_CTL_STATE_INFO	13	// Get last status information from MSW
#define	MSW_CTL_SPY			14 // Execute a SPY command
#define MSW_CTL_SETCARRIERCONSTELLATION 15 // to select the Constellation to re

#define MSW_CTL_SET_DEBUG_LEVEL  16      // set DebugLevel
#define MSW_CTL_SET_MSW_DEBUG_LEVEL  17      // set MswDebugLevel

// code 32-63 for NET
#define NET_CTL_SET_VPI_VCI      32
#define NET_CTL_GET_VPI_VCI      33
#define NET_CTL_TX_OAM_CELL      34
#define NET_CTL_GET_OAM_STATS    35
#define NET_CTL_RESET_OAM_STATS  36

struct unicorn_dev;

/*
  Called by ATM driver to get a pointer to the next cell to transmit or receive.
*/
typedef unsigned char * (*unicorn_getcell_t)(struct unicorn_dev *dev);

/*
  Called by ATM driver start transmitting cells.
*/
typedef int (*unicorn_start_transmit_t)(struct unicorn_dev *dev);

/* 
   Called by ATM driver for MSW ioctl's. 
*/
typedef int (*unicorn_msw_control_t)(struct unicorn_dev *dev,T_MswCtrl *ctrl);

/* 
   Called by ATM driver to get the ADSL status 
*/
typedef ADSL_STATUS (*unicorn_get_adsl_status_t)(struct unicorn_dev *dev);

/* 
   Called by ATM driver to get the ADSL link speed in Kbits/sec
*/
typedef int (*unicorn_get_adsl_linkspeed_t)(struct unicorn_dev *dev,
					    unsigned long *us_rate,unsigned long *ds_rate);

/*
  Entrypoints for the USB/PCI driver
*/
typedef struct unicorn_entrypoints {
	struct unicorn_dev *dev; /* Private data for USB/PCI driver */
	unicorn_getcell_t snd_getcell;
	unicorn_getcell_t rcv_getcell;
	unicorn_start_transmit_t start_transmit;
	unicorn_msw_control_t msw_control;
	unicorn_get_adsl_status_t get_adsl_status;
	unicorn_get_adsl_linkspeed_t get_adsl_linkspeed;
} unicorn_entrypoints_t;

/* 
   Called by BUS driver when driver loaded (attached)
*/
extern int unicorn_attach(struct unicorn_entrypoints *entrypoints); 

/* 
   Called by BUS driver when driver unloaded (detached)
*/
extern int unicorn_detach(void);

/* 
   Called by BUS driver when HW timer expires
*/
extern int unicorn_timer(void);

#endif
