#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/socket.h>
//#include <atm.h>
#include <linux/atmdev.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <linux/if_packet.h>
//#include <linux/delay.h>
//#include "types.h"
#include "amu.h"
#include "unicorn.h"

//#include "cmdtest.h"

int thid; // thread's id report event

//AMSW_ModemState g_ModemState;
unsigned char g_Mode;
struct eventreported {
	int type;
	int code;
};


T_AMSW_Identification            	g_Identification;
T_AMSW_NT_NearEndLineOperData    	g_NearEndLineOperData;
T_AMSW_NT_FarEndLineOperData     	g_FarEndLineOperData;
T_AMSW_def_counter_set           	g_def_counter_set;
T_AMSW_def_bitmap_set           	g_def_bitmap_set;
T_AMSW_def_counters              	g_def_counters;
T_AMSW_NT_ChannelOperData        	g_ChannelOperData;
T_AMSW_ANT_CustomerConfiguration 	g_CustomerCfg;
T_AMSW_ANT_StaticConfiguration   	g_StaticCfg;
T_AMSW_PowerStateConfiguration   	g_PowerStateCfg;
// STM G
//T_AMSW_TeqX				g_Teq;
T_AMSW_Ber				g_Ber;
T_AMSW_VersionMS			g_VersionMS;
T_AMSW_Constellation 			g_Constellation;


#define	MSW_MODE_UNKNOWN    0
#define MSW_MODE_ANSI       1
#define MSW_MODE_GLITE      2
#define MSW_MODE_MULTI      3
#define MSW_MODE_GDMT       4		
#define MSW_MODE_MAX        5
DWORD mode=MSW_MODE_UNKNOWN;

struct ADSL_DEVICE {
	int sd;
	enum {
		ATM_DRIVER=1,
		ETH_DRIVER=2
	} type;
	union {
		struct atmif_sioc atm;
		struct ifreq eth;
	} req;
};
typedef struct ADSL_DEVICE ADSL_DEVICE;

int msw_ctrl(ADSL_DEVICE *device,T_MswCtrl *ctrl)
{
	ctrl->retcode = 0;
	if (device->type == ATM_DRIVER) {
		device->req.atm.arg = ctrl;
		device->req.atm.length = sizeof(T_MswCtrl);
		if (ioctl(device->sd,ATM_MSW_CTL,&device->req.atm) < 0) {
			return errno;
		}
		return ctrl->retcode;
	} else if (device->type == ETH_DRIVER) {
		device->req.eth.ifr_data = (char *)ctrl;
		if (ioctl(device->sd,ETH_MSW_CTL,&device->req.eth) < 0) {
			return errno;
		}
		return ctrl->retcode;
	}
	return -1;
}

int open_device(ADSL_DEVICE *device)
{
	unsigned long state=0L;
	T_MswCtrl ctrl ={MSW_CTL_GET_STATE,0,0,&state,sizeof(state)};
	int sd;

	// try first ATM socket
	sd = socket(PF_ATMPVC,SOCK_DGRAM,0);
	if (sd > 0) {
		device->sd = sd;
		device->type = ATM_DRIVER;
		device->req.atm.number = 0;
#ifdef DEBUG
		fprintf(stderr,"PF_ATMPVC socket opened,itf=%d\n",device->req.atm.number);
#endif	
		device->req.atm.arg = &ctrl;
		device->req.atm.length = sizeof(T_MswCtrl);
		if (ioctl(device->sd,ATM_MSW_CTL,&device->req.atm) == 0) {
			return 0;
		} else {
#ifdef DEBUG
			fprintf(stderr,"PF_ATMPVC msw_ctrl failed,,err=%s(%d)\n",strerror(errno),errno);
			close (sd);
#endif	
		}
	}

	// try Ethernet socket
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd > 0) {
		device->sd = sd;
		device->type = ETH_DRIVER;
		strcpy(device->req.eth.ifr_name,UNICORN_ETH_NAME);
#ifdef DEBUG
		fprintf(stderr,"AF_INET socket opened,itf=%s\n",device->req.eth.ifr_name);
#endif	
		device->req.eth.ifr_data = (char *)&ctrl;
		if (ioctl(device->sd,ETH_MSW_CTL,&device->req.eth) == 0) {
			return 0;
		} else {
#ifdef DEBUG
			fprintf(stderr,"AF_INET msw_ctrl failed,err=%s(%d)\n",strerror(errno),errno);
			close (sd);
#endif	
		}
	}
	return errno;
}

void close_device(ADSL_DEVICE *device)
{
	close(device->sd);
	device->sd = -1;
	device->type = 0;
}

static int modem_command(ADSL_DEVICE *device,unsigned long code,unsigned long subcode)
{
	int status;
	T_MswCtrl ctrl;

	ctrl.code = code;
	ctrl.subcode = subcode;
	ctrl.buffer = NULL;

	ctrl.length =0;

	status = msw_ctrl(device,&ctrl);
	if (status < 0) {
		printf("ioctl failed,errno=%d\n",errno);
		return errno;
	}
	if (ctrl.retcode < 0) {
		printf("ioctl failed,retcode=%ld\n> ",ctrl.retcode);
		return ctrl.retcode;
	}
	return C_AMSW_ACK;	
}

static int setModemConfiguration(ADSL_DEVICE *device,AMSW_ConfigType configType,void *data)
{
	int status;
	T_MswCtrl ctrl;

	ctrl.code = MSW_CTL_SET_CONFIG;
	ctrl.subcode = configType;
	ctrl.buffer = data;

	switch (configType) {
	case C_AMSW_STATIC_CONFIGURATION:
		ctrl.length = sizeof(T_AMSW_ANT_StaticConfiguration);
		break;
	case C_AMSW_CUSTOMER_CONFIGURATION:
		ctrl.length = sizeof(T_AMSW_ANT_CustomerConfiguration);
		break;
	case C_AMSW_POWER_STATE_CONTROL:
		ctrl.length = sizeof(T_AMSW_PowerStateConfiguration);
		break;
	}

	status = msw_ctrl(device,&ctrl);
	if (status < 0) {
		printf("ioctl failed,errno=%d\n",errno);
		return errno;
	}
	if (ctrl.retcode < 0) {
		printf("ioctl failed,retcode=%ld\n> ",ctrl.retcode);
		return ctrl.retcode;
	}
	return C_AMSW_ACK;	
}

static int getModemConfiguration(ADSL_DEVICE *device,AMSW_ConfigType configType,void *data)
{
	int status;
	T_MswCtrl ctrl;

	ctrl.code = MSW_CTL_GET_CONFIG;
	ctrl.subcode = configType;
	ctrl.buffer = data;

	switch (configType) {
	case C_AMSW_STATIC_CONFIGURATION:
		ctrl.length = sizeof(T_AMSW_ANT_StaticConfiguration);
		break;
	case C_AMSW_CUSTOMER_CONFIGURATION:
		ctrl.length = sizeof(T_AMSW_ANT_CustomerConfiguration);
		break;
	case C_AMSW_POWER_STATE_CONTROL:
		ctrl.length = sizeof(T_AMSW_PowerStateConfiguration);
		break;
	}

	status = msw_ctrl(device,&ctrl);
	if (status < 0) {
		printf("ioctl failed,errno=%d",errno);
		return status;
	}
	return C_AMSW_ACK;	
}

static int getData(ADSL_DEVICE *device,AMSW_DataType dataType,void *data)
{
	int status;
	T_MswCtrl ctrl;

	ctrl.code = MSW_CTL_GET_DATA;
	ctrl.subcode = dataType;
	ctrl.buffer = data;
	switch (dataType) {
	case C_AMSW_VERSIONMS:
		ctrl.length = sizeof(T_AMSW_VersionMS);
		break;
	case C_AMSW_TEQ:
		ctrl.length = sizeof(T_AMSW_TeqX);
		break;
	case C_AMSW_PM_DATA:
	case C_AMSW_FM_DATA:
		ctrl.length = sizeof(T_AMSW_def_counter_set);
		break;		
	case C_AMSW_FAR_END_IDENTIFICATION:            
	case C_AMSW_NEAR_END_IDENTIFICATION:
		ctrl.length = sizeof(T_AMSW_Identification);
		break;
	case C_AMSW_NEAR_END_LINE_DATA:  
		ctrl.length = sizeof(T_AMSW_NT_NearEndLineOperData);
		break;
	case C_AMSW_FAR_END_LINE_DATA:                 
	        ctrl.length = sizeof(T_AMSW_NT_FarEndLineOperData);
		break;
	case C_AMSW_NEAR_END_CHANNEL_DATA_FAST:		
	case C_AMSW_FAR_END_CHANNEL_DATA_FAST:    
	case C_AMSW_NEAR_END_CHANNEL_DATA_INTERLEAVED:
	case C_AMSW_FAR_END_CHANNEL_DATA_INTERLEAVED:
		ctrl.length = sizeof(T_AMSW_NT_ChannelOperData);
		break;	
	}
	status = msw_ctrl(device,&ctrl);
	if (status < 0) {
		printf("ioctl failed,errno=%d\n",status);
		return status;
	}
	return C_AMSW_ACK;	
}

static int getModemState(ADSL_DEVICE *device)
{
	int status;
	T_MswCtrl ctrl;

	ctrl.code = MSW_CTL_GET_STATE;
	ctrl.subcode = 0;
	//modemState = 0;
	ctrl.buffer = &g_ModemState;
	ctrl.length = sizeof(AMSW_ModemState);

	status = msw_ctrl(device,&ctrl);
	if (status < 0) {
		printf("ioctl failed,errno=%d\n",status);
		return status;
	}
	return C_AMSW_ACK;
}

void ModemStateUpdate(ADSL_DEVICE *device)
{
	int err;
	err = getModemState(device);
	if (err != C_AMSW_ACK)
	{
		printf("MSW Not Loaded\nDoing on console prompt:\nmodprobe unicorn_pci\n");
		//Error from AMSW_ANT_getModemState
	}
}

static int getAndPrintModemState(ADSL_DEVICE *device)
{
	int status;
	T_MswCtrl ctrl;

	AMSW_ModemState modemState;

	ctrl.code = MSW_CTL_GET_STATE;
	ctrl.subcode = 0;
	modemState = 0;
	ctrl.buffer = &modemState;
	ctrl.length = sizeof(AMSW_ModemState);

	status = msw_ctrl(device,&ctrl);
	if (status < 0) {
		printf("ioctl failed,status=%d\n",status);
		return status;
	}

	switch (modemState) {
	case C_AMSW_IDLE                :
		printf("%s\n> ","C_AMSW_IDLE");
		break;
	case C_AMSW_L3                  :
		printf("%s\n> ","C_AMSW_L3");
		break;
	case C_AMSW_LISTENING           :
		printf("%s\n> ","C_AMSW_LISTENING");
		break;
	case C_AMSW_ACTIVATING          :
		printf("%s\n> ","C_AMSW_ACTIVATIN");
		break;
	case C_AMSW_Ghs_HANDSHAKING     :
		printf("%s\n> ","C_AMSW_Ghs_HANDSHAKING");
		break;
	case C_AMSW_ANSI_HANDSHAKING    :
		printf("%s\n> ","C_AMSW_ANSI_HANDSHAKING");
		break;
	case C_AMSW_INITIALIZING        :
		printf("%s\n> ","C_AMSW_INITIALIZING");
		break;
	case C_AMSW_RESTARTING          :
		printf("%s\n> ","C_AMSW_RESTARTING");
		break;
	case C_AMSW_FAST_RETRAIN        :
		printf("%s\n> ","C_AMSW_FAST_RETRAIN");
		break;
	case C_AMSW_SHOWTIME_L0         :
		printf("%s\n> ","C_AMSW_SHOWTIME_L0");
		break;
	case C_AMSW_SHOWTIME_LQ         :
		printf("%s\n> ","C_AMSW_SHOWTIME_LQ");
		break;
	case C_AMSW_SHOWTIME_L1         :
		printf("%s\n> ","C_AMSW_SHOWTIME_L1");
		break;
	case C_AMSW_EXCHANGE            :
		printf("%s\n> ","C_AMSW_EXCHANGE");
		break;
	case C_AMSW_TRUNCATE            :
		printf("%s\n> ","C_AMSW_TRUNCATE");
		break;
	case C_AMSW_ESCAPE              :
		printf("%s\n> ","C_AMSW_ESCAP");
		break;
	case C_AMSW_DISORDERLY          :
		printf("%s\n> ","C_AMSW_DISORDERLY");
		break;
	case C_AMSW_RETRY		:
		printf("%s\n> ","C_AMSW_RETRY");
		break;
	default:
		printf("unknown modem state %d\n> ",modemState);
		break;
	}		
	return C_AMSW_ACK;
}

static int SetCarrierConstellation(ADSL_DEVICE *device, int i)
{
	int status;
	T_MswCtrl ctrl;

	ctrl.code = MSW_CTL_SETCARRIERCONSTELLATION;
	ctrl.subcode = i;
	ctrl.buffer = 0;

	ctrl.length = 0;//sizeof(T_AMSW_Constellation);

	status = msw_ctrl(device,&ctrl);
	if (status < 0) {
		printf("ioctl failed,status=%d\n",status);
		return status;
	}
	return C_AMSW_ACK;	
}

static int dyingGasp(ADSL_DEVICE *device)
{
	int status;
	T_MswCtrl ctrl;

	ctrl.code = MSW_CTL_DYING_GASP;
	ctrl.subcode = 0;
	ctrl.buffer = 0;

	ctrl.length = 0;//sizeof(T_AMSW_Constellation);

	status = msw_ctrl(device,&ctrl);
	if (status < 0) {
		printf("ioctl failed,status=%d\n",status);
		return status;
	}
	return C_AMSW_ACK;	
}

static int ping(ADSL_DEVICE *device)
{
	int status;
	T_MswCtrl ctrl;
	T_atm_channel tx_oam;

	scanf("%d.%d",&tx_oam.vpi,&tx_oam.vci);
	if (tx_oam.vpi == 0) {
		tx_oam.type = ATM_OAM_F4;
	} else {
		tx_oam.type = ATM_OAM_F5;
	}
	printf("pinging %d.%d...\n",tx_oam.vpi,tx_oam.vci);

	ctrl.code = NET_CTL_TX_OAM_CELL;
	ctrl.subcode = 0;
	ctrl.buffer = &tx_oam;
	ctrl.length = sizeof(tx_oam);

	status = msw_ctrl(device,&ctrl);
	if (status < 0) {
		printf("ioctl failed,status=%d\n",status);
		return status;
	}
	return C_AMSW_ACK;	
}

static int get_oam_stats(ADSL_DEVICE *device)
{
	int status;
	T_MswCtrl ctrl;
	T_oam_stats oam_stats;

	ctrl.code = NET_CTL_GET_OAM_STATS;
	ctrl.subcode = 0;
	ctrl.buffer = &oam_stats;
	ctrl.length = sizeof(oam_stats);

	status = msw_ctrl(device,&ctrl);
	if (status < 0) {
		printf("ioctl failed,status=%d\n",status);
		return status;
	}
	printf("-------- CELLS TRANSMITTED ----------\n");
	printf("Loopback cells                 = %04ld\n",oam_stats.tx_LB);
	printf("-------- CELLS RECEIVED -------------\n");
	printf("Continuity Check cells         = %04ld\n",oam_stats.rx_CC);
	printf("Remote Deefct Indication cells = %04ld\n",oam_stats.rx_RDI);
	printf("Alarm Indication Signal cells  = %04ld\n",oam_stats.rx_AIS);
	printf("Far-End Loopback cells         = %04ld\n",oam_stats.rx_fe_LB);
	printf("Near-End Loopback cells        = %04ld\n",oam_stats.rx_ne_LB);
	return C_AMSW_ACK;	
}

static int set_debug_level(ADSL_DEVICE *device)
{
	int status;
	T_MswCtrl ctrl;

	scanf("%ld",&ctrl.subcode);
	ctrl.code = MSW_CTL_SET_DEBUG_LEVEL;
	ctrl.buffer = NULL;
	ctrl.length = 0;

	status = msw_ctrl(device,&ctrl);
	if (status < 0) {
		printf("ioctl failed,status=%d\n",status);
		return status;
	}
	return C_AMSW_ACK;	
}

static int set_msw_debug_level(ADSL_DEVICE *device)
{
	int status;
	T_MswCtrl ctrl;

	scanf("%ld",&ctrl.subcode);
	ctrl.code = MSW_CTL_SET_MSW_DEBUG_LEVEL;
	ctrl.buffer = NULL;
	ctrl.length = 0;

	status = msw_ctrl(device,&ctrl);
	if (status < 0) {
		printf("ioctl failed,status=%d\n",status);
		return status;
	}
	return C_AMSW_ACK;	
}

void printGuiString(void)
{
	printf("\n\n HELP -> Help\n");
	printf(" ----------------------------------------------------------\n");
	printf(" ANSI           : Initialize and Start in Operation Mode: ANSI\n");
	printf(" MULTI          : Initialize and Start in Operation Mode: MULTI\n");
	printf(" GLITE          : Initialize and Start in Operation Mode: GLITE\n");
	printf(" GDMT           : Initialize and Start in Operation Mode: GDMT\n");
	printf(" UP             : Activate the Line\n");
	printf(" DOWN           : Deactivate the Line\n");
	printf(" CONFIG         : Get Near End Modem Configuration\n");
	printf(" VENDOR         : Get Vendor and Version Info\n");
	printf(" OPER           : Get Line Operational Data\n");
	printf(" FAULT          : Get Fault Counters\n");
	printf(" PERF           : Get Performance Counters\n");
	printf(" RATE           : Get Actual Bitrate\n");
	printf(" MODE           : Get Modem State\n");
	printf(" TEQ            : Get TEQ values\n");
	printf(" VER            : Get Software Version\n");
	printf(" GASP           : Send DyingGasp Command\n");
	printf(" UNLOAD         : Unload Operation Mode\n");
	printf(" CONSTELLATION #: Show Constellation Data for # carrier\n");
	printf(" PING <VPI.VCI> : Send OAM Loopback Cell\n");
	printf(" OAM            : Get OAM Statistics\n");
	printf(" DEBUG <LEVEL>  : Set Debug Level\n");
	printf(" MSW <LEVEL>    : Set Msw Debug Level\n");
	printf(" ----------------------------------------------------------\n");
	printf(" X      : Exit Console Application\n");
	printf(" ----------------------------------------------------------\n\n");
}

bool InterpretCmdString (char *cmd,ADSL_DEVICE *device)
{
	int err;
	static bool MSW_INIT = FALSE;
	int i;

	if (strcmp(cmd,"HELP") == 0)
	{
			printGuiString();
			return TRUE;
	}
	else if (strcmp(cmd,"ANSI") == 0)
	{
		err = getModemConfiguration(device,C_AMSW_CUSTOMER_CONFIGURATION, &g_CustomerCfg);
		if (err != C_AMSW_ACK)
		{
			printf("Error in AMSW_ANT_getModemConfiguration for Customer Configuration = %d\n> ", err);
			g_Mode = 0;
		}
		else g_Mode = g_CustomerCfg.POTSoverlayOperationModes;
		if (g_Mode == 0)
		{
			printf("\n");
			mode = MSW_MODE_ANSI;
			g_CustomerCfg.POTSoverlayOperationModes = AMSW_ANSI;
		        err = setModemConfiguration(device,C_AMSW_CUSTOMER_CONFIGURATION,&g_CustomerCfg);
			if(err != C_AMSW_ACK)
				printf("Error in AMSW_ANT_setModemConfiguration(CustomerConfig) (%d)\n> ", err);
			printf("Operation Mode is: ANSI\n");
			modem_command(device,MSW_CTL_START,0);
			MSW_INIT = TRUE;
		}
		else
			printf("Modem already initialized\n> ");
		return TRUE;
	}	
	else if (strcmp(cmd,"MULTI") == 0)
	{
		err = getModemConfiguration(device,C_AMSW_CUSTOMER_CONFIGURATION, &g_CustomerCfg);
		if (err != C_AMSW_ACK)
		{
			printf("Error in AMSW_ANT_getModemConfiguration for Customer Configuration = %d\n> ", err);
			g_Mode = 0;
		}
		else g_Mode = g_CustomerCfg.POTSoverlayOperationModes;
		if (g_Mode == 0)
		{
			printf("\n");
			mode = MSW_MODE_MULTI;
			g_CustomerCfg.POTSoverlayOperationModes = AMSW_G_DMT | AMSW_ANSI | AMSW_G_LITE;
		        err = setModemConfiguration(device,C_AMSW_CUSTOMER_CONFIGURATION,&g_CustomerCfg);
			if(err != C_AMSW_ACK)
				printf("Error in AMSW_ANT_setModemConfiguration(CustomerConfig) (%d)\n> ", err);
			printf("Operation Mode is: MULTI MODE\n");
			modem_command(device,MSW_CTL_START,0);
			MSW_INIT = TRUE;
		}
		else
			printf("Modem already initialized\n> ");
		return TRUE;
	}	
	else if (strcmp(cmd,"GLITE") == 0)
	{
		err = getModemConfiguration(device,C_AMSW_CUSTOMER_CONFIGURATION, &g_CustomerCfg);
		if (err != C_AMSW_ACK)
		{
			printf("Error in AMSW_ANT_getModemConfiguration for Customer Configuration = %d\n> ", err);
			g_Mode = 0;
		}
		else g_Mode = g_CustomerCfg.POTSoverlayOperationModes;
		if (g_Mode == 0)
		{
			printf("\n");
			mode = MSW_MODE_GLITE;
			g_CustomerCfg.POTSoverlayOperationModes = AMSW_G_LITE;
		        err = setModemConfiguration(device,C_AMSW_CUSTOMER_CONFIGURATION,&g_CustomerCfg);
			if(err != C_AMSW_ACK)
				printf("Error in AMSW_ANT_setModemConfiguration(CustomerConfig) (%d)\n> ", err);
			printf("Operation Mode is: G.LITE\n> ");
			modem_command(device,MSW_CTL_START,0);
			MSW_INIT = TRUE;
		}
		else
			printf("Modem already initialized\n> ");
		return TRUE;
	}
	else if (strcmp(cmd,"GDMT") == 0)
	{
		err = getModemConfiguration(device,C_AMSW_CUSTOMER_CONFIGURATION, &g_CustomerCfg);
		if (err != C_AMSW_ACK)
		{
			printf("Error in AMSW_ANT_getModemConfiguration for Customer Configuration = %d\n> ", err);
			g_Mode = 0;
		}
		else g_Mode = g_CustomerCfg.POTSoverlayOperationModes;
		if (g_Mode == 0)
		{
			printf("\n");
			mode = MSW_MODE_GDMT;
			g_CustomerCfg.POTSoverlayOperationModes = AMSW_G_DMT;
		        err = setModemConfiguration(device,C_AMSW_CUSTOMER_CONFIGURATION,&g_CustomerCfg);
			if(err != C_AMSW_ACK)
				printf("Error in AMSW_ANT_setModemConfiguration(CustomerConfig) (%d)\n> ", err);
			printf("Operation Mode is: G.DMT\n");
			modem_command(device,MSW_CTL_START,0);
       			MSW_INIT = TRUE;
		}
		else
			printf("Modem already initialized\n> ");
		return TRUE;
	}
	else if (strcmp(cmd,"UP") == 0)
	{
		ModemStateUpdate(device);
    		if (g_ModemState == C_AMSW_IDLE)
		{	
			modem_command(device,MSW_CTL_START,0);
		}
		else
		{
			printf("Modem is NOT in IDLE yet\n> ");
		}
		return TRUE;
	}	
	else if (strcmp(cmd,"DOWN") == 0)
	{
		ModemStateUpdate(device);
    		if (g_ModemState == C_AMSW_SHOWTIME_L0)
		{	
			modem_command(device,MSW_CTL_STOP,0);
		}
		else
		{
			printf("Modem is NOT in SHOWTIME yet\n> ");
		}
		return TRUE;
	}	
	else if (strcmp(cmd,"CONFIG") == 0)
	{
			err = getModemConfiguration(device,C_AMSW_STATIC_CONFIGURATION,&g_StaticCfg);
			if(err)
				printf("AMSW_ANT_getModemConfiguratio error\n");
			err = getModemConfiguration(device,C_AMSW_CUSTOMER_CONFIGURATION,&g_CustomerCfg);
			if(err)
				printf("AMSW_ANT_getModemConfiguration error\n");
			err = getModemConfiguration(device,C_AMSW_POWER_STATE_CONTROL,&g_PowerStateCfg);
			if(err)
				printf("AMSW_ANT_getModemConfiguration error\n");
			if (err) return FALSE;
	
			printf("Near End Static configuration...\n\n");	
			printf("Utopia Mode               = 0x%x\n",(unsigned int)g_StaticCfg.utopiaMode);
			printf("Utopia Fast               = 0x%x\n",(unsigned int)g_StaticCfg.utopiaFast);
			printf("Utopia utopiaSlow         = 0x%x\n",(unsigned int)g_StaticCfg.utopiaSlow);						
			printf("Serial Number             = ");
			
			for (i=0; i<32; i++) printf("%c",g_StaticCfg.serialNumber[i]);
			printf("\n\n");
	
			printf("MaximumDownstreamLineRate = %ld\n",g_StaticCfg.maximumDownstreamLineRate);
			printf("managementVersion         = 0x%x\n",(unsigned int)g_StaticCfg.managementVersion);
			printf("goldenMode                = 0x%x\n",g_StaticCfg.goldenMode);
			printf("\n");
			printf("Vendor Identification...\n\n");
			printf("Country Code              = 0x%x\n",g_StaticCfg.vendorIdentif.countryCode);
			printf("Reserved                  = 0x%x\n",g_StaticCfg.vendorIdentif.reserved);
			printf("Vendor Code               = 0x%x\n",(unsigned int)g_StaticCfg.vendorIdentif.vendorCode);
			printf("Vendor Specific           = 0x%x\n",g_StaticCfg.vendorIdentif.vendorSpecific);

			printf("\nCustomer Configuration...\n\n");
			printf("POTSoverlayOperationModes = 0x%x\n",g_CustomerCfg.POTSoverlayOperationModes);
			for (i=0;i<8;i++)
				printf("POTSoverlayPermissions[%d] = 0x%x\n",i, g_CustomerCfg.POTSoverlayPermissions[i]);

			printf("\nPower Configuration...\n\n");
			printf("PowerStateControl = 0x%x\n",g_PowerStateCfg.powerStateControl);
			printf("\n");
			printf("\n> ");
			
			return TRUE;
	}	
	else if (strcmp(cmd,"VENDOR") == 0)
	{
			printf("\n");
			err = getData(device,C_AMSW_NEAR_END_IDENTIFICATION,&g_Identification);
			if(err)
				printf("AMSW_ANT_getData error\n");
			if (err) return FALSE;
			
			if (g_Mode == AMSW_ANSI)
			{
				printf("Near End ANSI_ETSI_VendorId : %d \n",g_Identification.ANSI_ETSI_VendorId);
				printf("Near End ANSI_ETSI_StandardRevisionNbr : 0x%x \n",g_Identification.ANSI_ETSI_StandardRevisionNbr);
				printf("Near End Vendor Specific : 0x%x \n",g_Identification.ANSI_ETSI_VendorRevisionNbr);
			} else {
				printf("Near End Vendor Country Code : %d \n",g_Identification.ITU_VendorId.countryCode);
				printf("Near End Vendor Code : (0x%x) \n",(unsigned int)g_Identification.ITU_VendorId.vendorCode);
				printf("Near End ITU Revision Number : %d \n",g_Identification.ITU_StandardRevisionNbr);
				printf("Near End Vendor Specific : 0x%x \n",g_Identification.ITU_VendorId.vendorSpecific);
			}

			err = getData(device,C_AMSW_FAR_END_IDENTIFICATION,&g_Identification);
			if(err)
				printf("AMSW_ANT_getData error\n");
			
			if (err) return FALSE;
			
			if (g_Mode == AMSW_ANSI)
			{
				printf("Far End ANSI_ETSI_VendorId : %d \n",g_Identification.ANSI_ETSI_VendorId);
				printf("Far End ANSI_ETSI_StandardRevisionNbr : 0x%x \n",g_Identification.ANSI_ETSI_StandardRevisionNbr);
				printf("Far End Vendor Specific : 0x%x \n",g_Identification.ANSI_ETSI_VendorRevisionNbr);
			} 
			else 
			{
				printf("Far End Vendor Country Code : %d \n",g_Identification.ITU_VendorId.countryCode);
				printf("Far End Vendor Code : (0x%x) \n",(unsigned int)g_Identification.ITU_VendorId.vendorCode);
				printf("Far End ITU Revision Number : %d \n",g_Identification.ITU_StandardRevisionNbr);
				printf("Far End Vendor Specific : 0x%x \n",g_Identification.ITU_VendorId.vendorSpecific);
			}

			return TRUE;
	}	
	else if (strcmp(cmd,"OPER") == 0)
	{
		printf("Line operational data\n");
		ModemStateUpdate(device);
    		if (g_ModemState == C_AMSW_SHOWTIME_L0)
		{	
			err = getData(device,C_AMSW_FAR_END_LINE_DATA, &g_FarEndLineOperData);
			if (err != C_AMSW_ACK)
			{
				printf("Error in AMSW_ANT_getData for Far End Line Data = %d\n", err);
			}
			else
			{
				err = getData(device,C_AMSW_NEAR_END_LINE_DATA, &g_NearEndLineOperData);
				if (err != C_AMSW_ACK)
				{
					printf("Error in AMSW_ANT_getData for Near End Line Data = %d\n", err);
				}
				else
				{
					printf("Operation Mode Seletected : ");
					switch (g_NearEndLineOperData.operationalMode)
					{
					case 1:
						printf("ANSI\n");
						break;   
					case 2:
						printf("G.DMT\n");
						break;   
					default:
						printf("G.Lite\n");
						break;   
					}
					printf("\nDownstream Capacity Occupation : %d %%\n",g_NearEndLineOperData.relCapacityOccupationDnstr); 
					printf("Downstream Noise Margin  : %f dB \n",(((float) g_NearEndLineOperData.noiseMarginDnstr)/2.0));
					printf("Downstream Attenuation   : %f dB \n",(((float) g_NearEndLineOperData.attenuationDnstr)/2.0));
					printf("Downstream Output Power  : %f dBm \n",(((float) g_FarEndLineOperData.outputPowerDnstr)/2.0));
				}
				printf("\nUpstream Capacity Occupation : %d %%\n",g_FarEndLineOperData.relCapacityOccupationUpstr); 
				printf("Upstream Noise Margin  : %f dB \n",(((float) g_FarEndLineOperData.noiseMarginUpstr)/2.0));
				printf("Upstream Attenuation   : %f dB \n",(((float) g_FarEndLineOperData.attenuationUpstr)/2.0));
				printf("Upstream Output Power  : %f dBm \n",(((float) g_NearEndLineOperData.outputPowerUpstr)/2.0));
			
				// Display carrier load g_FarEndLineOperData.carrierLoad
				printf("\nCarrier Load (bits/tone)\n\n");
				for(err = 0; err <=127; err+=2)
				{
					printf("[%3d] -- %d\t",err * 2, ((g_FarEndLineOperData.carrierLoad[err] & 0xf0) >> 4));
					printf("[%3d] -- %d\t",err * 2+1, g_FarEndLineOperData.carrierLoad[err] & 0x0f);
					printf("[%3d] -- %d\t",err * 2+2, ((g_FarEndLineOperData.carrierLoad[err+1] & 0xf0) >> 4));
					printf("[%3d] -- %d\n",err * 2+3, g_FarEndLineOperData.carrierLoad[err+1] & 0x0f);
				}
			}
		}
		else
		{
			printf("Modem is NOT in SHOWTIME yet\n");
		}
		return TRUE;
	}
		
	else if (strcmp(cmd,"FAULT") == 0)
	{
		printf("Fault counters\n");
		ModemStateUpdate(device);
    		if (g_ModemState == C_AMSW_SHOWTIME_L0)
		{	
			err = getData(device,C_AMSW_FM_DATA, &g_def_bitmap_set);
			if (err != C_AMSW_ACK)
			{
				printf("Error in AMSW_ANT_getData for Defects = %d\n",err);
			}
			else
			{
				printf("============== NEAR END =============\n");
				if(g_def_bitmap_set.near_end.status == 0)
				{
					printf("No defects\n");
				}                        
				else
				{
					if ((g_def_bitmap_set.near_end.status & 0x4) == 0x4)
						printf("Loss Of Margin (LoM) Defect Detected\n");
					if ((g_def_bitmap_set.near_end.status & 0x8) == 0x8)
						printf("Loss Of Cell Delineation (LoCD) Defect Detected - INTERLEAVED CHANNEL\n");
					if ((g_def_bitmap_set.near_end.status & 0x10) == 0x10)
						printf("Loss Of Cell Delineation (LoCD) Defect Detected - FAST CHANNEL\n");
					if ((g_def_bitmap_set.near_end.status & 0x20) == 0x20)
						printf("Loss Of Power (LoP) Defect Detected\n");
					if ((g_def_bitmap_set.near_end.status & 0x40) == 0x40)
						printf("Loss Of Frame (LoF) Defect Detected\n");
					if ((g_def_bitmap_set.near_end.status & 0x80) == 0x80)
						printf("Loss Of Signal (LoS) Defect Detected\n");
					if ((g_def_bitmap_set.near_end.status & 0x100) == 0x100)
						printf("Dying Gasp message has either -- been sent by ATU_R (or) received by ATU_C\n");
				}

				printf("============== FAR END =============\n");
				if (g_def_bitmap_set.far_end.status == 0)
				{
		        		printf("No defects\n");
				}
				else
				{
					if ((g_def_bitmap_set.far_end.status & 0x4) == 0x04)
						printf("Loss Of Margin (LoM) Defect Detected\n");
					if ((g_def_bitmap_set.far_end.status & 0x8) == 0x08)
						printf("Loss Of Cell Delineation (LoCD)  Defect Detected - INTERLEAVED CHANNEL\n");
					if ((g_def_bitmap_set.far_end.status & 0x10) == 0x10)
						printf("Loss Of Cell Delineation (LoCD)  Defect Detected - FAST CHANNEL\n");
					if ((g_def_bitmap_set.far_end.status & 0x10) == 0x20)
						printf("Loss Of Power Defect Detected\n");
					if ((g_def_bitmap_set.far_end.status & 0x40) == 0x40)
						printf("Loss Of Frame (LoF) Defect Detected\n");
					if ((g_def_bitmap_set.far_end.status & 0x80) == 0x80)
						printf("Loss Of Signal (LoS) Defect Detected\n");
					if ((g_def_bitmap_set.far_end.status & 0x100) == 0x100)
						printf("Dying Gasp message has to be either -- sent to ATU_R (or) received from ATU_C\n");
				}
			}
	
		}
		else
		{
			printf("Modem is NOT in SHOWTIME yet\n");
		}
		return TRUE;
	}	
	else if (strcmp(cmd,"PERF") == 0)
	{
		printf("Performance counters\n");
		ModemStateUpdate(device);
    		if (g_ModemState == C_AMSW_SHOWTIME_L0)
		{	
			err = getData(device,C_AMSW_PM_DATA, &g_def_counter_set);
			if (err != C_AMSW_ACK)
			{
				printf("Error in AMSW_ANT_getData for Performance Counter");
			}
			else
			{
				printf("============= NEAR END ===========\n");
				printf("---------- FAST CHANNEL ----------\n");
				printf("FEC : %d \n",g_def_counter_set.near_end.FecNotInterleaved);
				printf("CRC : %d \n",g_def_counter_set.near_end.CrcNotInterleaved);
				printf("HEC : %d \n",g_def_counter_set.near_end.HecNotInterleaved);
				//printf("BER : %d \n",g_def_counter_set.near_end.BERNotInterleaved);
				printf("------- INTERLEAVED CHANNEL ------\n");
				printf("FEC : %d \n",g_def_counter_set.near_end.FecInterleaved);
				printf("CRC : %d \n",g_def_counter_set.near_end.CrcInterleaved);
				printf("HEC : %d \n",g_def_counter_set.near_end.HecInterleaved);
				//printf("BER : %d \n",g_def_counter_set.near_end.BERInterleaved);
				printf("============= FAR END ============\n");
				printf("---------- FAST CHANNEL ----------\n");
				printf("FEC : %d \n",g_def_counter_set.far_end.FecNotInterleaved);
				printf("CRC : %d \n",g_def_counter_set.far_end.CrcNotInterleaved);
				printf("HEC : %d \n",g_def_counter_set.far_end.HecNotInterleaved);
				//printf("BER : %d \n",g_def_counter_set.far_end.BERNotInterleaved);
				printf("------- INTERLEAVED CHANNEL ------\n");
				printf("FEC : %d \n",g_def_counter_set.far_end.FecInterleaved);
				printf("CRC : %d \n",g_def_counter_set.far_end.CrcInterleaved);
				printf("HEC : %d \n",g_def_counter_set.far_end.HecInterleaved);
				//printf("BER : %d \n",g_def_counter_set.far_end.BERInterleaved);
			}

		}
		else
		{
			printf("Modem is NOT in SHOWTIME yet\n");
		}
		return TRUE;
	}	
	else if (strcmp(cmd,"RATE") == 0)
	{
		ModemStateUpdate(device);
    		if (g_ModemState == C_AMSW_SHOWTIME_L0)
		{	
			printf("Actual bit rate\n");
			err = getData(device,C_AMSW_NEAR_END_CHANNEL_DATA_FAST, &g_ChannelOperData);
			if (err != C_AMSW_ACK)	/*thid = fork();
	
	if (thid == 0)
		report_events(device);*/

				printf("Error in AMSW_ANT_getData for Near End Line Data (Fast) = %d\n", err);
			else
				printf("Actual Bit Rate (NEAR END FAST CHANNEL):%15d Kbps \n",g_ChannelOperData.actualBitrate);
       
			err = getData(device,C_AMSW_FAR_END_CHANNEL_DATA_FAST, &g_ChannelOperData);
			if (err != C_AMSW_ACK)
				printf("Error in AMSW_ANT_getData for Far End Line Data (Fast) = %d \n", err);
			else
				printf("Actual Bit Rate (FAR END FAST CHANNEL):%16d Kbps \n",g_ChannelOperData.actualBitrate);
       
			err = getData(device,C_AMSW_NEAR_END_CHANNEL_DATA_INTERLEAVED, &g_ChannelOperData);
			if (err != C_AMSW_ACK)
				printf("Error in AMSW_ANT_getData for Near End Line Data (Interleaved) = %d\n", err);
			else
				printf("Actual Bit Rate (NEAR END INTERLEAVED CHANNEL):%8d Kbps \n",g_ChannelOperData.actualBitrate);
       
			err = getData(device,C_AMSW_FAR_END_CHANNEL_DATA_INTERLEAVED, &g_ChannelOperData);
			if(err != C_AMSW_ACK)
				printf("Error in AMSW_ANT_getData for Far End Line Data (Interleaved) = %d\n", err);
			else
				printf("Actual Bit Rate (FAR END INTERLEAVED CHANNEL):%9d Kbps \n",g_ChannelOperData.actualBitrate);
		}
		else
		{
			printf("Modem is NOT in SHOWTIME yet\n");
		}
		return TRUE;
	}	
	else if (strcmp(cmd,"GASP") == 0)
	{
		printf("\n");

		err = dyingGasp(device);
		if(err != C_AMSW_ACK)
		{
			printf("Error in AMSW_ANT_dyingGasp (%d)\n",err);
		}
			return TRUE;
	}	
	else if (strcmp(cmd,"MODE") == 0)
	{
			printf("\n");
		printf("ADSL Modem Mode is: ");
		if (g_Mode == 0)                printf("Uninitialized");
		else if (g_Mode == AMSW_ANSI)   printf("ANSI" );
		else if (g_Mode == AMSW_G_LITE) printf("GLITE" );
		else if (g_Mode == AMSW_G_DMT)  printf("GDMT" );
		else                            printf("MULTI" );
			printf("\n");
		err = getAndPrintModemState(device);
		printf("\n");
			return TRUE;
	}
	else if (strcmp(cmd,"VER") == 0)	//STM G
	{
//			T_AMSW_VersionMS *VersionMS;
			printf("\n");
			err = getData(device,C_AMSW_VERSIONMS, &g_VersionMS);
			if (err != C_AMSW_ACK)
			{
				printf("Modem code version : MODEMSW NOT LOADED\n");
			}
			else
			{
				printf("Modem code version : %s \n",g_VersionMS.versionA);
			}
					
			return TRUE;
	}
	else if (strcmp(cmd,"BER") == 0)
	{
		printf("BER\n");
		ModemStateUpdate(device);
    		if (g_ModemState == C_AMSW_SHOWTIME_L0)
		{	
			err = getData(device,C_AMSW_BER, &g_Ber);
			if (err != C_AMSW_ACK)
			{
				printf("CmdBer Error in AMSW_ANT_getData for CmdBer = %d\n", err);
			}
			else
			{
				printf("CmdBer : %ld \n",g_Ber.a);
			
			}	
		}
		else
		{
			printf("Modem is NOT in SHOWTIME yet\n");
		}
		return TRUE;
	}
	else if (strcmp(cmd,"TEQ") == 0)
	{

		printf("\n");
		ModemStateUpdate(device);
		if (g_ModemState == C_AMSW_SHOWTIME_L0)
		{
			err = getData(device,C_AMSW_TEQ, &g_Teq);
			if (err != C_AMSW_ACK)
			{
				printf("CmdTeq Error in AMSW_ANT_getData for CmdTeq = %d\n", err);
			}
			else
			{	
				printf(  "|------------------------------------------------|\n");
				printf(  "|-------- F I N A L   T E Q   C O E F F ---------|\n");
				printf(  "|------------------|-----------------------------|\n");
				printf(  "|      TEQ Noise   |     FREQUENCY   RESPONSE    |\n");
				printf(	 "|   #   |  Table   |    Real(hf[i])     Im(hf[i])|\n");
				printf(  "|-------|----------|-----------------------------|\n");
				for (i=0; i<256; i++)
				{
					printf("%d\t %f\t%f\t%f\n",i,g_Teq.Teq_noise_table[i],g_Teq.hf[i*2],g_Teq.hf[i*2+1]);
					//printf("%3d\t %10.6f\t%10.6f\t%10.6f\n",i,g_Teq.Teq_noise_table[i],g_Teq.hf[i*2],g_Teq.hf[i*2+1]);
				}

			// final teq coeff
				printf(  "|--------------------------------------------------------|\n");
				printf(  "|------------- F I N A L   T E Q   C O E F F ------------|\n");
				printf(  "|--------------------------------------------------------|\n");
				printf(  "|----------|---------------------------------------------|\n");

				for (i=0; i<8; i++)
				{
					//printf(" %-2d to %-2d\t %-7d\t%-7d\t%-7d\t%-7d\n",i*4,i*4+3,g_Teq.teq_global[i*4],g_Teq.teq_global[i*4+1],g_Teq.teq_global[i*4+2],g_Teq.teq_global[i*4+3]);
					printf(" %-2d to %-2d\t %-7d\t%-7d\t%-7d\t%-7d\n",i*4,i*4+3,g_Teq.teq_global[i*4],g_Teq.teq_global[i*4+1],g_Teq.teq_global[i*4+2],g_Teq.teq_global[i*4+3]);
				}
				printf(  "|----------|---------------------------------------------|\n");
			}
		}
		else
		{
			printf("Modem is NOT in SHOWTIME yet\n");
		}
		return TRUE;
	}
	else if (strcmp(cmd,"UNLOAD") == 0)
	{

		printf("\n");
		ModemStateUpdate(device);
		if (g_ModemState == C_AMSW_IDLE)
		{
			if (g_Mode == 0 )
			{
				printf("Modem Already Unloaded!\n");
			}	
			else
			{
				printf("Modem unloaded! \n");
				g_Mode = 0;
				g_CustomerCfg.POTSoverlayOperationModes = 0; // STM Gian verificare che serva effettivamente
				modem_command(device,MSW_CTL_EXIT,0);
			}
		}
		else
		{
			printf("To unload force Modem in IDLE (type DOWN command)\n");
		}
		return TRUE;
	}
	else if (strcmp(cmd,"CONSTELLATION") == 0)
	{
		if (scanf("%d",&i) == 1)
		{
			printf("CONSTELLATION %d\n",i);
			ModemStateUpdate(device);
    			if (g_ModemState == C_AMSW_SHOWTIME_L0)
			{	
				SetCarrierConstellation(device,i);
				if (i == 0)
				{
					printf("Constellation monitoring stop\n");
					return TRUE;
				}
				sleep(2);
				err = getData(device,C_AMSW_CONSTELLATION, &g_Constellation);
				if (err != C_AMSW_ACK)
				{
					printf("CmdConstellation Error in AMSW_ANT_getData for CmdConstellation = %d\n", err);
				}
				else
				{
					printf("CmdConstellation : %ld \n",g_Constellation.carrier);
					printf("--------------------------------\n");
					for (i=0; i< 32; i++)
						printf("%-4d %8d %8d \n",i,g_Constellation.X[i],g_Constellation.Y[i]);
					printf("--------------------------------\n");
				}
			}
			else
			{
				printf("Modem is NOT in SHOWTIME yet\n");
			}
				
		} 
		else 
		{
			printf("ERROR usage is:\n\n CONSTELLATION # \n\n where # is a carrier (integer format)\n");
			return TRUE;
		}
	}
	else if (strcmp(cmd,"PING") == 0) 
		{
			ping(device);
		}
	else if (strcmp(cmd,"OAM") == 0) 
		{
		get_oam_stats(device);
		}
	else if (strcmp(cmd,"DEBUG") == 0) 
		{
		set_debug_level(device);
		}
	else if (strcmp(cmd,"MSW") == 0) 
		{
		set_msw_debug_level(device);
		}
	else if (strcmp(cmd,"X") == 0)
	{
			return FALSE;
	}	
	else
	{
			printGuiString();
			return TRUE;
	}
	return TRUE;
}

// ADSL Modem Software calls this function to report any state changes
void AMSW_ANT_reportModemStateChange(AMSW_ModemState p_ModemState)
{
	char *s;

	g_ModemState = p_ModemState;

	switch(p_ModemState)
	{
	case C_AMSW_IDLE             : s = "IDLE";             break; 
	case C_AMSW_L3               : s = "L3";               break; 
	case C_AMSW_LISTENING        : s = "LISTENING";        break; 
	case C_AMSW_ACTIVATING       : s = "ACTIVATING";       break;
	case C_AMSW_Ghs_HANDSHAKING  : s = "Ghs_HANDSHAKING";  break; 
	case C_AMSW_ANSI_HANDSHAKING : s = "ANSI_HANDSHAKING"; break; 
	case C_AMSW_INITIALIZING     : s = "INITIALIZING";     break; 
	case C_AMSW_RESTARTING       : s = "RESTARTING";       break; 
	case C_AMSW_FAST_RETRAIN     : s = "FAST_RETRAIN";     break; 
	case C_AMSW_SHOWTIME_L0      : s = "SHOWTIME_L0";      break; 
	case C_AMSW_SHOWTIME_LQ      : s = "SHOWTIME_LQ";      break; 
	case C_AMSW_SHOWTIME_L1      : s = "SHOWTIME_L1";      break; 
	case C_AMSW_EXCHANGE         : s = "EXCHANGE";         break; 
	case C_AMSW_TRUNCATE         : s = "TRUNCATE";         break; 
	case C_AMSW_ESCAPE           : s = "ESCAPE";           break; 
	default                      : s = "Unknown State";    break; 
	}
	printf("Current Modem State (%d): %s\n> ", p_ModemState, s);

}

// ADSL Modem Software calls this function to report events
void AMSW_ANT_reportEvent(AMSW_ModemEvent p_Event)
{
	char *s;
	switch(p_Event)
	{
	default                            : s = "UNKNOWN";                break;
	case C_AMSW_PEER_ATU_FOUND         : s = "Peer ATU Found";         break;
	case C_AMSW_RESTART_REQUEST        : s = "Restart Request";        break;
	case C_AMSW_ACTIVATION_REQUEST     : s = "Activation Request";     break;
	case C_AMSW_L3_EXECUTED            : s = "L3 Executed";            break;
	case C_AMSW_L3_REJECTED            : s = "L3 Rejected";            break;
	case C_AMSW_L1_EXECUTED            : s = "L1 Executed";            break;
	case C_AMSW_L1_REJECTED            : s = "L1 Rejected";            break;
	case C_AMSW_L0_REJECTED            : s = "L0 Rejected";            break;
	case C_AMSW_RESTART_ACCEPTABLE     : s = "Restart Acceptable";     break;
	case C_AMSW_RESTART_NOT_ACCEPTABLE : s = "Restart Not Acceptable"; break;
	case C_AMSW_TO_INITIALIZING        : s = "Initializing";           break;
	case C_AMSW_SHOWTIME               : s = "Showtime";			   break;
	case C_AMSW_SUICIDE_REQUEST        : s = "Suicide Request";        break;
	}
	printf("\nEvent Reported (%d): %s\n> ", p_Event, s);

	if (p_Event == 4)
		printf("\n> ");
}

// ADSL Modem Software calls this function to report the reason for failure
void AMSW_ANT_reportModemFailure(AMSW_ModemFailure p_FailureCause)
{
	char *s;
	switch(p_FailureCause)
	{
	case C_AMSW_UNCOMPATIBLE_LINECONDITIONS           : s = "Uncompatible Line Conditions";          break;
	case C_AMSW_NO_LOCK_POSSIBLE                      : s = "No Lock Possible";                      break;
	case C_AMSW_PROTOCOL_ERROR                        : s = "Protocol Error";                        break;
	case C_AMSW_MESSAGE_ERROR                         : s = "Message Error";                         break;
	case C_AMSW_SPURIOUS_ATU_DETECTED                 : s = "Spurious ATU Detected";                 break;
	case C_AMSW_FORCED_SILENCE                        : s = "Forced Silence";                        break;
	case C_AMSW_DS_REQ_BITRATE_TOO_HIGH_FOR_LITE      : s = "Requested Bit Rate Too High";           break;
	case C_AMSW_INTERLEAVED_PROFILE_REQUIRED_FOR_LITE :	s = "Interleaved Profile Required for LITE"; break;
	case C_AMSW_UNSELECTABLE_OPERATION_MODE           : s = "Unselectable Operation Mode";           break;
	case C_AMSW_STATE_REFUSED_BY_GOLDEN               : s = "State Refused By Golden";               break;
	default                                           : s = "Unknown Cause";                         break;
	}
	printf("\nModem Init Failure: %s (%d)\n> ",s,p_FailureCause); 
}

unsigned long get_event(ADSL_DEVICE *device)
{
	int status;
	T_MswCtrl ctrl;
	unsigned long event = 0;

	ctrl.code = MSW_CTL_WAIT_EVENT;
	ctrl.subcode = 0;
	ctrl.buffer = &event;

	ctrl.length = 4; //sizeof(eventreported);

	status = msw_ctrl(device,&ctrl);
	if (status < 0) {
		return status;
	}
	return event;	
}

void report_events(ADSL_DEVICE *device)
{
	unsigned long event;
	AMSW_ModemFailure modemFailure;
	AMSW_ModemEvent   modemEvent;
	AMSW_ModemState   modemState;
	
	while (1)
	{
		sleep(1);
		event = get_event(device);
		if (event != 0)
		{
			//printf("### event in if is : %ld \n", event);
			switch (event>>16)
			{
			case MSW_EVENT_FAILURE:
				modemFailure = (AMSW_ModemFailure)(event&0xffff);
				AMSW_ANT_reportModemFailure(modemFailure);
				break;
			case MSW_EVENT_REPORT:
				modemEvent = (AMSW_ModemEvent)(event&0xffff);
				AMSW_ANT_reportEvent(modemEvent);
				break;
			case MSW_EVENT_STATE:
				modemState = (AMSW_ModemState)(event&0xffff);
				AMSW_ANT_reportModemStateChange(modemState);
				break;
			case AMU_EVENT_SHUTDOWN:
				printf("Bringing Down Line due to persistent faults!\n> ");
				break;
			case AMU_EVENT_RETRY:
				printf("> Auto-Retry will take place.....\n\n> ");
				break;
			case AMU_EVENT_ACT_TIMEOUT:
				printf("\n> Activation Timeout!\n");
				printf("\n> Going to IDLE.....\n\n> ");
				break;	
			case AMU_EVENT_INI_TIMEOUT:
				printf("\n> Initialization Timeout!\n");
				printf("\n> Going to IDLE.....\n\n> ");
				break;
			}
		}
	}
}

char * strupr(char *cmd)
{
	int i,endstr;
	endstr=strlen(cmd);
	for (i=0;i<endstr;i++)
		cmd[i] = toupper(cmd[i]);
	return cmd;
}

int main(int argc,char *argv[])
{
	char cmd[256];
	char lastcmd[256]="";	  //STM
	static ADSL_DEVICE device;

	printf("%s, v %d.%d.%d, " __TIME__ " " __DATE__"\n",
	       argv[0],(VERS>>8)&0xf,(VERS>>4)&0xf,VERS&0xf);

	if (open_device(&device) != 0) {
		printf("no ADSL device found !!\n");
		return -1;
	}
	
	thid = fork();
	
	if (thid == 0)
		report_events(&device);
		

	g_CustomerCfg.POTSoverlayOperationModes = AMSW_G_DMT | AMSW_ANSI | AMSW_G_LITE;
	mode = MSW_MODE_MULTI;

	printGuiString();

	ModemStateUpdate(&device);


	for(;;)
	{
		if (strcmp(lastcmd,"UP"))
				printf("\n> ");
		if (scanf("%s",cmd) != 1) break;
		strcpy(lastcmd,strupr(cmd));
		if (!InterpretCmdString(lastcmd,&device)) break;

	}
	
	close_device(&device);
	
	kill(thid,SIGKILL);

	return 0;
}
