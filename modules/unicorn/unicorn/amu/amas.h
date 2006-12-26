#ifndef _AMAS_H_
#define _AMAS_H_
#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

// Constants and typedefs
typedef unsigned long AMSW_DataType;
typedef unsigned long AMSW_ResultCode;
typedef unsigned long AMSW_ConfigType;

/* possible structs for configData in setModemConfiguration */
typedef unsigned char  VendorIdentification[16];
typedef struct {
	unsigned char   MinNoiseMarginDnstr;
	unsigned char   MinNoiseMarginUpstr;
	unsigned char   MaxAddNoiseMarginDnstr;
	unsigned char   MaxAddNoiseMarginUpstr;
	unsigned char   TargetNoiseMarginDnstr;
	unsigned char   TargetNoiseMarginUpstr;
	signed char     MaxPSDDnstr;
	unsigned char   CarrierMask[32];
	unsigned long   RAModeDnstr;
	unsigned long   RAModeUpstr;
	unsigned short  InterlMinBitrateDnstr;
	unsigned short  InterlMinBitrateUpstr;
	unsigned short  InterlPlannedBitrateDnstr;
	unsigned short  InterlPlannedBitrateUpstr;
	unsigned short  InterlMaxBitrateDnstr;
	unsigned short  InterlMaxBitrateUpstr;
	unsigned char   InterlMaxDelayDnstr;
	unsigned char   InterlMaxDelayUpstr;
	unsigned short  FastMinBitrateDnstr;
	unsigned short  FastMinBitrateUpstr;
	unsigned short  FastPlannedBitrateDnstr;
	unsigned short  FastPlannedBitrateUpstr;
	unsigned short  FastMaxBitrateDnstr;
	unsigned short  FastMaxBitrateUpstr;
	unsigned short  LowPowerBitrateDnstr;
	unsigned short  LowPowerBitrateUpstr;
	unsigned char   POTSoverlayOperationModes;
	unsigned short  POTSoverlayPermissions[8];
	unsigned char   ISDNoverlayOperationModes;
	unsigned short  ISDNoverlayPermissions[8];
} T_AMSW_LT_OperatorConfiguration;

typedef struct {
	unsigned char   USpowerCutback;
	unsigned char   POTSoverlayOperationModes;
	unsigned short  POTSoverlayPermissions[8];
	unsigned char   ISDNoverlayOperationModes;
	unsigned short  ISDNoverlayPermissions[8];
} T_AMSW_ANT_CustomerConfiguration;

typedef struct   {
	unsigned char   countryCode;
	unsigned char   reserved;
	unsigned long   vendorCode;
	unsigned short  vendorSpecific;
} T_ITU_VendorId;

typedef struct {
	T_ITU_VendorId  ITU_VendorId;
	unsigned char   ITU_StandardRevisionNbr;
	unsigned short  ANSI_ETSI_VendorId;
	unsigned char   ANSI_ETSI_VendorRevisionNbr;
	unsigned char   ANSI_ETSI_StandardRevisionNbr;
	unsigned long   ALC_ManagementInfo;
} T_AMSW_Identification;

typedef struct {
	unsigned long   utopiaMode;
	unsigned long   utopiaFast;
	unsigned long   utopiaSlow;
	T_ITU_VendorId  vendorIdentif;
	unsigned char   goldenMode;
} T_AMSW_LT_StaticConfiguration;

typedef struct {
	unsigned long  utopiaMode;
	unsigned long  utopiaFast;
	unsigned long  utopiaSlow;
	char           serialNumber[32];
	T_ITU_VendorId vendorIdentif;
	unsigned long  maximumDownstreamLineRate;
	unsigned long  managementVersion;
	unsigned char  goldenMode;
} T_AMSW_ANT_StaticConfiguration;

typedef struct {
	unsigned char powerStateControl;
} T_AMSW_PowerStateConfiguration;

typedef struct {
	unsigned short relCapacityOccupationDnstr;
	unsigned short attainableBitrateDnstr;
	signed char	   noiseMarginDnstr;
	signed char	   outputPowerUpstr;
	unsigned char  attenuationDnstr;
} T_AMSW_LT_FarEndLineOperData;

typedef struct {
	unsigned short relCapacityOccupationDnstr;
	signed char    noiseMarginDnstr;
	signed char    outputPowerUpstr;
	unsigned char  attenuationDnstr;
	unsigned long  operationalMode;
} T_AMSW_NT_NearEndLineOperData;

typedef struct {
	unsigned short relCapacityOccupationUpstr;
	signed char    noiseMarginUpstr;
	signed char    outputPowerDnstr;
	unsigned char  attenuationUpstr;
	unsigned char  carrierLoad[128];
} T_AMSW_NT_FarEndLineOperData;

typedef struct {
	unsigned short actualBitrate;
} T_AMSW_LT_ChannelOperData;

typedef struct {
	unsigned short actualBitrate;
} T_AMSW_NT_ChannelOperData;

typedef struct {
	unsigned long  status;
	unsigned long  change;
} T_AMSW_def_bitmaps;

typedef struct {
	T_AMSW_def_bitmaps  near_end;
	T_AMSW_def_bitmaps  far_end;
} T_AMSW_def_bitmap_set;

typedef struct {
	unsigned short  FecNotInterleaved;
	unsigned short  FecInterleaved;
	unsigned short  CrcNotInterleaved;
	unsigned short  CrcInterleaved;
	unsigned short  HecNotInterleaved;
	unsigned short  HecInterleaved;
	unsigned short  TotalCellCountInterleaved;
	unsigned short  TotalCellCountNotInterleaved;
	unsigned short  ActiveCellCountInterleaved;
	unsigned short  ActiveCellCountNotInterleaved;
	unsigned short  BERInterleaved;
	unsigned short  BERNotInterleaved;
} T_AMSW_def_counters;

typedef struct {
	T_AMSW_def_counters  near_end;
	T_AMSW_def_counters  far_end;
} T_AMSW_def_counter_set;
//---------------------------------------
// STM G
//---------------------------------------
typedef struct {
	float Teq_noise_table[256];
	float hf[512];
	unsigned long a;
	short teq_global[32];
} T_AMSW_Teq;

typedef struct {
	float Teq_noise_table[256];
	float hf[512];
	unsigned long a;
	short teq_global[32];
} T_AMSW_TeqX;

typedef struct {
	unsigned long a;
} T_AMSW_Ber;

typedef struct {
	char versionA[40];
} T_AMSW_VersionMS;

typedef struct {
	unsigned long carrier;
	short X[32];
	short Y[32];
} T_AMSW_Constellation;
//---------------------------------------
//#define STARTMSW_VERSION "UnicornTest 1.02"
//---------------------------------------
/* Bitpmap for Defects */
#define LOM			0x4
#define LCDI		0x8
#define LCDNI		0x10
#define LOF			0x40
#define LOS			0x80

/* possible bitpositions for POTSoverlayOperationModes */
#define AMSW_ANSI_IDX   0
#define AMSW_ANSI       (1<<AMSW_ANSI_IDX)
#define AMSW_G_DMT_IDX  1
#define AMSW_G_DMT      (1<<AMSW_G_DMT_IDX)
#define AMSW_UAWG_IDX   2
#define AMSW_UAWG       (1<<AMSW_UAWG_IDX)
#define AMSW_G_LITE_IDX 3
#define AMSW_G_LITE     (1<<AMSW_G_LITE_IDX)
#define AMSW_MULTI		4		//MADDA

/* constants for the utopia mode */
#define C_AMSW_UTOPIA_LEVEL1            1
#define C_AMSW_UTOPIA_LEVEL2            2

/* possible bitpositions for goldenMode */
//#define TILAB_ECI_MODIFICATION
#ifdef TILAB_ECI_MODIFICATION
#define AMSW_GOLDEN_OFF (0<<0)
#define AMSW_GOLDEN_ON  (1<<0)
#else
#define AMSW_GOLDEN_OFF (1<<0)
#define AMSW_GOLDEN_ON  (1<<1)
#endif

/* possible bitpositions for powerStateControl */
#define AMSW_L1 (1<<1)
#define AMSW_L3 (1<<3)

/* possible values for AMSW_ConfigType */
#define C_AMSW_OPERATOR_CONFIGURATION 1
#define C_AMSW_STATIC_CONFIGURATION   2
#define C_AMSW_CUSTOMER_CONFIGURATION 1
#define C_AMSW_POWER_STATE_CONTROL    3

/* possible values for AMSW_ResultCode */
#define C_AMSW_ACK                          0
#define C_AMSW_REJ                          1
#define C_AMSW_ERR_COM_INV_PARAM      0x10003

/* possible values for AMSW_DataType in getData */
#define C_AMSW_PM_DATA                           0
#define C_AMSW_FM_DATA                           1
#define C_AMSW_NEAR_END_IDENTIFICATION           2
#define C_AMSW_FAR_END_IDENTIFICATION            3
#define C_AMSW_NEAR_END_LINE_DATA                4
#define C_AMSW_FAR_END_LINE_DATA                 5
#define C_AMSW_NEAR_END_CHANNEL_DATA_FAST        6
#define C_AMSW_FAR_END_CHANNEL_DATA_FAST         7
#define C_AMSW_NEAR_END_CHANNEL_DATA_INTERLEAVED 8
#define C_AMSW_FAR_END_CHANNEL_DATA_INTERLEAVED  9

// STM G
#define C_AMSW_VERSIONMS						10
#define C_AMSW_BER								11
#define C_AMSW_TEQ								12
#define C_AMSW_CONSTELLATION					13

/* possible bitpositions for the permissions */
#define AMSW_TRELLIS              (1<<0)
#define AMSW_FAST_RETRAIN         (1<<1)
#define AMSW_POWER_MANAGEMENT     (1<<2)
#define AMSW_BITSWAP              (1<<3)
#define AMSW_RS16                 (1<<4)
#define AMSW_US_PILOT_MODULATED   (1<<5)
#define AMSW_DS_PILOT_MODULATED   (1<<6)
#define AMSW_GOLDEN_CHANNEL       (1<<12)
#define AMSW_LQ                   (1<<13)

/* constants for the states */
typedef unsigned short AMSW_ModemState;

#define C_AMSW_IDLE                0
#define C_AMSW_L3                  1
#define C_AMSW_LISTENING           2
#define C_AMSW_ACTIVATING          3
#define C_AMSW_Ghs_HANDSHAKING     4
#define C_AMSW_ANSI_HANDSHAKING    5
#define C_AMSW_INITIALIZING        6
#define C_AMSW_RESTARTING          7
#define C_AMSW_FAST_RETRAIN        8
#define C_AMSW_SHOWTIME_L0         9
#define C_AMSW_SHOWTIME_LQ         10
#define C_AMSW_SHOWTIME_L1         11
#define C_AMSW_EXCHANGE            12
#define C_AMSW_TRUNCATE            13
#define C_AMSW_ESCAPE              14
#define C_AMSW_DISORDERLY          0xabcd   //STM
#define C_AMSW_RETRY			   15		//STM

/* constants for the events */
typedef unsigned char AMSW_ModemEvent;

#define C_AMSW_PEER_ATU_FOUND           0
#define C_AMSW_RESTART_REQUEST          1
#define C_AMSW_ACTIVATION_REQUEST       2
#define C_AMSW_TO_INITIALIZING          3
#define C_AMSW_SHOWTIME                 4
#define C_AMSW_L3_EXECUTED              5
#define C_AMSW_L3_REJECTED              6
#define C_AMSW_L1_EXECUTED              7
#define C_AMSW_L1_REJECTED              8
#define C_AMSW_L0_REJECTED              9
#define C_AMSW_RESTART_ACCEPTABLE       10
#define C_AMSW_SUICIDE_REQUEST          11
#define C_AMSW_RESTART_NOT_ACCEPTABLE   12

/* modemfailures */
typedef unsigned short AMSW_ModemFailure;

#define C_AMSW_UNCOMPATIBLE_LINECONDITIONS               5
#define C_AMSW_NO_LOCK_POSSIBLE                         10
#define C_AMSW_PROTOCOL_ERROR                           15
#define C_AMSW_MESSAGE_ERROR                            20
#define C_AMSW_SPURIOUS_ATU_DETECTED                    25
#define C_AMSW_DS_REQ_BITRATE_TOO_HIGH_FOR_LITE         30
#define C_AMSW_INTERLEAVED_PROFILE_REQUIRED_FOR_LITE    35
#define C_AMSW_FORCED_SILENCE                           40
#define C_AMSW_UNSELECTABLE_OPERATION_MODE              45
#define C_AMSW_STATE_REFUSED_BY_GOLDEN                  50

#define C_AMSW_AMU_EVENT_ACT_TIMEOUT                  	60 /* AMU_EVENT_ACT_TIMEOUT */
#define C_AMSW_AMU_EVENT_INI_TIMEOUT                  	65 /* AMU_EVENT_INI_TIMEOUT */
#define C_AMSW_AMU_EVENT_SHUTDOWN                  	70 /* AMU_EVENT_SHUTDOWN */
#define C_AMSW_EVENT_RETRY				75 /* AMU_EVENT_RETRY */
#define C_AMSW_UNKNOWN									80 
#define C_AMSW_UNKNOWN_FAILURE							85 
#define C_AMSW_NO_HARDWARE								90 
#define C_AMSW_NO_USB_BANDWIDTH							95 

void            AMSW_ANT_reportModemFailure(AMSW_ModemFailure p_FailureCause);
void            AMSW_ANT_reportEvent(AMSW_ModemEvent p_Event);
void            AMSW_ANT_reportModemStateChange(AMSW_ModemState p_ModemState);
AMSW_ResultCode AMSW_ANT_setModemConfiguration(AMSW_ConfigType configType, void *configData);
AMSW_ResultCode AMSW_ANT_getModemConfiguration(AMSW_ConfigType configType, void *configData);

AMSW_ResultCode	AMSW_ANT_getData(AMSW_DataType dataType, void * data);
AMSW_ResultCode	AMSW_ANT_requestModemStateChange(AMSW_ModemState requestedState);
AMSW_ResultCode	AMSW_ANT_getModemState(AMSW_ModemState *modemState);
unsigned long   AMSW_ANT_dyingGasp(void);
AMSW_ResultCode AMSW_ANT_wait_event(unsigned long *event);
unsigned long   AMSW_Chip_Halt(void);	// STM: Nick 28/8/2000

unsigned long   AMSW_Modem_SW_Init(unsigned long deviceNumber,
                                   unsigned long chipAddress,
                                   unsigned long intr);
void            AMSW_Modem_SW_Exit(void);
AMSW_ResultCode AMSW_ANT_setCarrierConstellation(long Carrier);

// STM : Initialization Mode Options

#define	MSW_MODE_UNKNOWN    0
#define MSW_MODE_ANSI       1
#define MSW_MODE_GLITE      2
#define MSW_MODE_MULTI      3
#define MSW_MODE_GDMT       4
#define MSW_MODE_MAX        5

void msw_init(unsigned short mode);            // Cold entry point of the MSW
void msw_exit(void);                // Shutdown of the MSW
void msw_start(void);               // Activation of the line
void msw_stop(void);                // Deactivation of the line

unsigned long msw_get_event(void);  // Wait for changes in the MSW state
void msw_cancel_event(void);		// Cancel the wait

void msw_report_event(unsigned long type,unsigned long code);

//void stwinmsw_Ver(T_AMSW_VersionMS *VersionMSdata); //STM Gian
char * stwinmsw_Ver(void);

// Globals
extern int amu_go;
extern unsigned long GlobalRemove;	// Driver is being removed
extern unsigned long last_report;


#ifdef WIN32
void _cdecl TestCycle(void *);
#endif
//	type of the event returned by msw_get_event()

#define	MSW_EVENT_NONE      0
#define	MSW_EVENT_REPORT    1
#define	MSW_EVENT_FAILURE   2
#define	MSW_EVENT_STATE     3
#define	MSW_EVENT_CANCEL	4

#define AMU_EVENT_ACT_TIMEOUT	5
#define AMU_EVENT_INI_TIMEOUT	25
#define AMU_EVENT_SHUTDOWN		7
#define AMU_EVENT_RETRY			8

#ifdef WIN32
//---------------------------------------------------------------------
//	STMCTRL.DLL interface
//---------------------------------------------------------------------
//	entry points in STMCTRL.DLL used by applications to access MSW data
//
//	This should be the only way to access the MSW data from user mode
//---------------------------------------------------------------------
void MswInitDll(void);			// Must be called by console applications
void MswExitDll(void);			// Must be called by console applications

void MswInit(int mode);			// Initializes the MSW
void MswExit(void);				// Shutdowns the MSW
void MswStart(void);			// Start the line activation
void MswStop(void);				// Stop the line activation

DWORD MswGetConfig(DWORD,PVOID);// Get a MSW config structure
DWORD MswGetData(DWORD,PVOID);	// Get a MSW data structure
DWORD MswDyingGasp(void);		// Send the dying gasp command
DWORD MswGetState(void);		// Get the activation state
DWORD MswSetCarrier(DWORD);		// Set Carrier for constellation retrieval

DWORD MswGetEvent(void);		// Wait for changes in the MSW state
void MswCancelEvent(void);		// Cancel the wait

char *MswCtrlVersion(void);		// Returns the version of STMCTRL.DLL

DWORD HalWriteProm(PBYTE,WORD,WORD);	// Write to the configuration EEPROM
DWORD HalReadProm(PBYTE,WORD,WORD);	// Read the configuration EEPROM
#endif

#ifdef __cplusplus
}
#endif
#endif
/* end of file */
