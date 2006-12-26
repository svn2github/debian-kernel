#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <gtk/gtk.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/atmdev.h>
#include "amu.h"
#include "unicorn.h"
#include "support.h"


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


/* globals */
static ADSL_DEVICE g_device={-1,0};
static GtkWidget *g_main_window=NULL;

static T_StateInfo last_info = { C_AMSW_IDLE,-1,-1,0L};
static T_AMSW_def_counter_set last_dcs = {{0}};
static T_AMSW_def_bitmap_set last_dbs = {{0}};

static int g_send_oam=0;
int g_vpi,g_vci;
static T_oam_stats last_oam_counters = {0};
/* config */
struct {
	int modulation;
	int vpi;
	int vci;
	char protocol[128];
	char encaps[128];
} AdslConfig = {AMSW_ANSI,0,0,"",""};
	
#define	CURVES	3		// Maximum number of curves
#define	MAXVAL	256		// Size of the curve table

int mCurves;
float mCurve[CURVES][MAXVAL];	// Tables of values
/* Backing pixmap for drawing area */
static GdkPixmap *pixmap = NULL;

	// colors
static const GdkColor chart_colors[CURVES] = { 
	{0,65535,0,0}, // red
	{0,0,65535,0}, // green
	{0,0,0,65535}, // blue
};

static void set_color(GtkWidget *widget,const GdkColor *color)
{
	GtkRcStyle *rc_style = gtk_rc_style_new ();
	rc_style->fg[GTK_STATE_NORMAL] = *color;
	rc_style->color_flags[GTK_STATE_NORMAL] |= GTK_RC_FG;
	gtk_widget_modify_style (widget,rc_style);
	gtk_rc_style_unref (rc_style);
}

static void label_set_text(GtkWidget *main_window,const char *field, const char *text)
{
	gtk_label_set_text(GTK_LABEL(lookup_widget(GTK_WIDGET(main_window),field)),text);	
}

static void label_set_number(GtkWidget *main_window,const char *field, int number)
{
	char str[8];

	g_snprintf(str,8,"%d",number);
	
	label_set_text(main_window,field,str);	
}

static void label_set_float(GtkWidget *main_window,const char *field, float number)
{
	char str[8];

	g_snprintf(str,8,"%.2f",number);
	
	label_set_text(main_window,field,str);	
}

static int label_get_number(GtkWidget *main_window,const char *field)
{
	const gchar *str;

	str = gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(main_window),field)));
	return atoi(str);
}

static void hide_widget(GtkWidget *main_window,const char *field) 
{
	GtkWidget *widget = lookup_widget(GTK_WIDGET(main_window),field);
	
	gtk_widget_hide(GTK_WIDGET(widget));
}

static void show_widget(GtkWidget *main_window,const char *field) 
{
	GtkWidget *widget = lookup_widget(GTK_WIDGET(main_window),field);
	
	gtk_widget_show_now(GTK_WIDGET(widget));	
}

const char *modem_state_string(AMSW_ModemState modemState)
{
	switch(modemState) {
	case C_AMSW_IDLE: return N_("IDLE");
	case C_AMSW_L3:  return N_("L3");
	case C_AMSW_LISTENING:  return N_("LISTENING");
	case C_AMSW_ACTIVATING:  return N_("ACTIVATING");
	case C_AMSW_Ghs_HANDSHAKING:  return N_("Ghs_HANDSHAKING");
	case C_AMSW_ANSI_HANDSHAKING:  return N_("ANSI_HANDSHAKING");
	case C_AMSW_INITIALIZING:  return N_("INITIALIZING");
	case C_AMSW_RESTARTING:  return N_("RESTARTING");
	case C_AMSW_FAST_RETRAIN:  return N_("FAST_RETRAIN");
	case C_AMSW_SHOWTIME_L0:  return N_("SHOWTIME_L0");
	case C_AMSW_SHOWTIME_LQ:  return N_("SHOWTIME_LQ");
	case C_AMSW_SHOWTIME_L1:  return N_("SHOWTIME_L1");
	case C_AMSW_EXCHANGE:  return N_("EXCHANGE");
	case C_AMSW_TRUNCATE:  return N_("TRUNCATE");
	case C_AMSW_ESCAPE:  return N_("ESCAPE");
	case C_AMSW_RETRY:  return N_("RETRY");
	case C_AMSW_DISORDERLY:  return N_("DISORDERLY");
	default: return N_(" ");
	}
}

const char *modem_event_string(AMSW_ModemEvent event)
{
	switch(event) {
	case C_AMSW_PEER_ATU_FOUND: return N_("Peer ATU Found");
	case C_AMSW_RESTART_REQUEST: return N_("Restart Request");
	case C_AMSW_ACTIVATION_REQUEST: return N_("Activation Request");
	case C_AMSW_TO_INITIALIZING: return N_("Initializing");
	case C_AMSW_SHOWTIME: return N_("Showtime");
	case C_AMSW_L3_EXECUTED: return N_("L3 Executed");
	case C_AMSW_L3_REJECTED: return N_("L3 Rejected");
	case C_AMSW_L1_EXECUTED: return N_("L1 Executed");
	case C_AMSW_L1_REJECTED: return N_("L1 Rejected");
	case C_AMSW_L0_REJECTED: return N_("L0 Rejected");
	case C_AMSW_RESTART_ACCEPTABLE: return N_("Restart Acceptable");
	case C_AMSW_SUICIDE_REQUEST: return N_("Suicide Request");
	case C_AMSW_RESTART_NOT_ACCEPTABLE: return N_("Restart Not Acceptable");
	default: return N_(" ");
	}
}

const char *modem_failure_string(AMSW_ModemFailure failure)
{
	switch(failure) {
	case C_AMSW_UNCOMPATIBLE_LINECONDITIONS: return N_("Uncompatible Line Conditions");
	case C_AMSW_NO_LOCK_POSSIBLE: return N_("No Lock Possible");
	case C_AMSW_PROTOCOL_ERROR: return N_("Protocol Error");
	case C_AMSW_MESSAGE_ERROR: return N_("Message Error");
	case C_AMSW_SPURIOUS_ATU_DETECTED: return N_("Spurious ATU Detected");
	case C_AMSW_DS_REQ_BITRATE_TOO_HIGH_FOR_LITE: return N_("Requested Bit Rate Too High");
	case C_AMSW_INTERLEAVED_PROFILE_REQUIRED_FOR_LITE: return N_("Interleaved Profile Required for LITE");
	case C_AMSW_FORCED_SILENCE: return N_("Forced Silence");
	case C_AMSW_UNSELECTABLE_OPERATION_MODE: return N_("Unselectable Operation Mode");
	case C_AMSW_STATE_REFUSED_BY_GOLDEN: return N_("State Refused By Golden");
	case C_AMSW_AMU_EVENT_ACT_TIMEOUT: return N_("Activation Timeout");
	case C_AMSW_AMU_EVENT_INI_TIMEOUT: return N_("Initialization Timeout");
	case C_AMSW_AMU_EVENT_SHUTDOWN: return N_("Bringing Down Line due to persistent faults");
	case C_AMSW_EVENT_RETRY: return N_("Retry");
	case C_AMSW_UNKNOWN: return N_("Unknown");
	case C_AMSW_UNKNOWN_FAILURE: return N_("Unknown failure");
	case C_AMSW_NO_HARDWARE: return N_("No hardware detected");
	case C_AMSW_NO_USB_BANDWIDTH: return N_("USB bandwidth unavailable");
	default: return N_(" ");
	}
}

const char *modulation_string(unsigned long operationalMode)
{
	switch(operationalMode) {
	case AMSW_ANSI: return N_("ANSI");
	case AMSW_G_LITE: return N_("G.lite");
	case AMSW_G_DMT: return N_("G.dmt");
	default: return N_(" ");
	}
}

static void read_adsl_config(const char *filename)
{
	FILE *file;
	char line[64];

	file = fopen(filename,"r");
	if (!file) return;
	
	while (fgets(line,sizeof(line),file)) {
		if (line[0] == '#') {
		} else if (sscanf(line,"MODULATION=%d",&AdslConfig.modulation)) {
		} else if (sscanf(line,"VPI=%d",&AdslConfig.vci)) {
		} else if (sscanf(line,"VCI=%d",&AdslConfig.vpi)) {
		} else if (sscanf(line,"PROTOCOL=%s",AdslConfig.protocol)) {
		} else if (sscanf(line,"ENCAPS=%s",AdslConfig.encaps)) {
		} else {
		}
	}
	fclose(file);
}

int msw_ctrl(T_MswCtrl *ctrl)
{
	ADSL_DEVICE *device = &g_device;

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
			fprintf(stderr,"PF_ATMPVC msw_ctrl failed\n");
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
			fprintf(stderr,"AF_INET msw_ctrl failed\n");
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

static void get_state_info(T_StateInfo *info)
{
	T_MswCtrl ctrl;

	info->State = 0;
	info->Report = -1;
	info->Failure = C_AMSW_NO_HARDWARE;
	info->TimeCnctd = 0L;

	ctrl.code = MSW_CTL_STATE_INFO;
	ctrl.subcode = 0;
	ctrl.retcode = 0;
	ctrl.buffer = info;
	ctrl.length = sizeof(T_StateInfo);

	msw_ctrl(&ctrl);
}

void get_bit_rates(unsigned short *bit_rates)
{
	int i;

	for (i=0; i < 4; i++) {
		T_AMSW_NT_ChannelOperData cod;
		T_MswCtrl ctrl = 
			{MSW_CTL_GET_DATA,C_AMSW_NEAR_END_CHANNEL_DATA_FAST+i,
			 0,&cod,sizeof(T_AMSW_NT_ChannelOperData)};
		bit_rates[i] = 0;
		msw_ctrl(&ctrl);
		if (ctrl.retcode == 0) {
			bit_rates[i] = cod.actualBitrate;
		} else {
			bit_rates[i] = 0;
		}
	}	
}

void get_near_end_line_oper_data( T_AMSW_NT_NearEndLineOperData *nelod )
{	
	T_MswCtrl ctrl = {MSW_CTL_GET_DATA,C_AMSW_NEAR_END_LINE_DATA,0,
			  nelod,sizeof(T_AMSW_NT_NearEndLineOperData)};
	memset(nelod,0,sizeof(T_AMSW_NT_NearEndLineOperData));
	msw_ctrl(&ctrl);
}

void get_far_end_line_oper_data( T_AMSW_NT_FarEndLineOperData *felod )
{	
	T_MswCtrl ctrl = {MSW_CTL_GET_DATA,C_AMSW_FAR_END_LINE_DATA,0,
			  felod,sizeof(T_AMSW_NT_FarEndLineOperData)};
	memset(felod,0,sizeof(T_AMSW_NT_FarEndLineOperData));
	msw_ctrl(&ctrl);
}

static void get_power_tables(float table[CURVES][MAXVAL])
{
	T_AMSW_TeqX teq;
	int i;
	T_MswCtrl ctrl = {MSW_CTL_GET_DATA,C_AMSW_TEQ,0,
			  &teq,sizeof(T_AMSW_TeqX)};
	
	int mScaling = 1;
	

	memset(&teq,0,sizeof(T_AMSW_TeqX));

	msw_ctrl(&ctrl);

	for (i=0; i < MAXVAL; i++) {
		// Echo + Noise
		float x = teq.Teq_noise_table[i]/mScaling + 0.000001;
		table[0][i] = 10*log10(x);

		// Receive Path
		x = teq.hf[2*i]*teq.hf[2*i]/mScaling/mScaling + 
			teq.hf[2*i+1]*teq.hf[2*i+1]/mScaling/mScaling +
			0.000001;
		table[1][i] = 20*log10(sqrt(x));

		// SNR
		if (i < 32) table[2][i] = 0;
		else table[2][i] = table[1][i] - table[0][i];
	}
}

static void get_counters( T_AMSW_def_counter_set *dcs )
{	
	T_MswCtrl ctrl = {MSW_CTL_GET_DATA,C_AMSW_PM_DATA,0,dcs,sizeof(T_AMSW_def_counter_set)};
	memset(dcs,0,sizeof(T_AMSW_def_counters));
	msw_ctrl(&ctrl);
}

static void get_oam_stats( T_oam_stats *oam_stats )
{	
	T_MswCtrl ctrl = {NET_CTL_GET_OAM_STATS,0,0,oam_stats,sizeof(T_oam_stats)};
	memset(oam_stats,0,sizeof(T_oam_stats));
	msw_ctrl(&ctrl);
}

static void get_bitmaps( T_AMSW_def_bitmap_set *dbs )
{	
	T_MswCtrl ctrl = {MSW_CTL_GET_DATA,C_AMSW_FM_DATA,0,dbs,sizeof(T_AMSW_def_bitmap_set)};
	memset(dbs,0,sizeof(T_AMSW_def_bitmap_set));
	msw_ctrl(&ctrl);
}

static void get_ident( T_AMSW_Identification *ident )
{	
	T_MswCtrl ctrl = {MSW_CTL_GET_DATA,C_AMSW_FAR_END_IDENTIFICATION,0,
			  ident,sizeof(T_AMSW_Identification)};
	memset(ident,0xFF,sizeof(T_AMSW_Identification));
	msw_ctrl(&ctrl);
}

static void get_version(T_AMSW_VersionMS *version)
{
	T_MswCtrl ctrl = {MSW_CTL_GET_DATA,C_AMSW_VERSIONMS,0,version,sizeof(T_AMSW_VersionMS)};
	
	memset(&version,0,sizeof(version));
	msw_ctrl(&ctrl);
}

static void get_driver_version(unsigned long *version)
{
	T_MswCtrl ctrl = {MSW_CTL_VERSION_INFO,0,0,version,sizeof(unsigned long)};
	*version = 0;
	msw_ctrl(&ctrl);
}

static void state_showtime(GtkWidget *main_window)
{
	unsigned short bit_rates[4];
	T_AMSW_NT_NearEndLineOperData nelod;
	T_AMSW_NT_FarEndLineOperData felod;
	T_AMSW_Identification ident;

	/* status panel */
	get_bit_rates(bit_rates);

	label_set_number(main_window,"NearEndFastChannelText",bit_rates[0]);
	label_set_number(main_window,"FarEndFastChannelText",bit_rates[1]);
	label_set_number(main_window,"NearEndInterleavedChannelText",bit_rates[2]);
	label_set_number(main_window,"FarEndInterleavedChannelText",bit_rates[3]);

	/* line panel */
	get_near_end_line_oper_data(&nelod);
	get_far_end_line_oper_data(&felod);

	label_set_text(main_window,"ModulationText",modulation_string(nelod.operationalMode));
	label_set_number(main_window,"DsCapacityOccupationText",nelod.relCapacityOccupationDnstr);
	label_set_float(main_window,"DsNoiseMarginText",(float)nelod.noiseMarginDnstr/2.0);
	label_set_float(main_window,"DsAttenuationText",(float)nelod.attenuationDnstr/2.0);
	label_set_float(main_window,"DsOutputPowerText",(float)felod.outputPowerDnstr/2.0);
	label_set_number(main_window,"UsCapacityOccupationText",felod.relCapacityOccupationUpstr);
	label_set_float(main_window,"UsNoiseMarginText",(float)felod.noiseMarginUpstr/2.0);
	label_set_float(main_window,"UsAttenuationText",(float)felod.attenuationUpstr/2.0);
	label_set_float(main_window,"UsOutputPowerText",(float)nelod.outputPowerUpstr/2.0);

	mCurves = CURVES;
	get_power_tables(mCurve);

	/* Test panel */
	if (g_device.type == ATM_DRIVER) {
		gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(main_window),"GenerateLBcellsButton"),TRUE);
	}
	/* info panel */
	get_ident(&ident);
		
	if (nelod.operationalMode == AMSW_ANSI) {
		label_set_number(main_window,"VendorCodeText",ident.ANSI_ETSI_VendorId);
		label_set_number(main_window,"ITURevisionNumberText",ident.ANSI_ETSI_StandardRevisionNbr);
		label_set_number(main_window,"VendorSpecificInfoText",ident.ANSI_ETSI_VendorRevisionNbr);
	} else {
		// G.dmt or G.lite
		char str[8];
		g_snprintf(str,8,"%c%c%c%c",
			   (int)ident.ITU_VendorId.vendorCode>>24,
			   (int)ident.ITU_VendorId.vendorCode>>16&255,
			   (int)ident.ITU_VendorId.vendorCode>>8&255,
			   (int)ident.ITU_VendorId.vendorCode&255);
		label_set_text(main_window,"VendorCodeText",str);
		label_set_number(main_window,"VendorCountryCodeText",ident.ITU_VendorId.countryCode);
		label_set_number(main_window,"ITURevisionNumberText",ident.ITU_StandardRevisionNbr);
		label_set_number(main_window,"VendorSpecificInfoText",ident.ITU_VendorId.vendorSpecific);
	}
}

static void state_idle(GtkWidget *main_window)
{
	T_AMSW_VersionMS version;

	/* Status panel */
	label_set_text(main_window,"ModemStateText",modem_state_string(last_info.State));
	//label_set_text(main_window,"RemoteReportText",modem_event_string(last_info.Report));
	//label_set_text(main_window,"LastFailureText",modem_failure_string(last_info.Failure));
	label_set_text(main_window,"TimeConnectedText","--:--:--");

	label_set_number(main_window,"NearEndFastChannelText",0);
	label_set_number(main_window,"FarEndFastChannelText",0);
	label_set_number(main_window,"NearEndInterleavedChannelText",0);
	label_set_number(main_window,"FarEndInterleavedChannelText",0);

	/* Errors panel */
	label_set_number(main_window,"LocFastFECText", 0);
	label_set_number(main_window,"LocFastCRCText", 0);
	label_set_number(main_window,"LocFastHECText", 0);
	label_set_number(main_window,"DisFastFECText", 0);
	label_set_number(main_window,"DisFastCRCText", 0);
	label_set_number(main_window,"DisFastHECText", 0);
	label_set_number(main_window,"LocItlvFECText", 0);
	label_set_number(main_window,"LocItlvCRCText", 0);
	label_set_number(main_window,"LocItlvHECText", 0);
	label_set_number(main_window,"DisItlvFECText", 0);
	label_set_number(main_window,"DisItlvCRCText", 0);
	label_set_number(main_window,"DisItlvHECText", 0);

	/* Defects panel */

	// Near End 
	// LoM
	hide_widget(main_window,"NearLoMRedLed");
	show_widget(main_window,"NearLoMLed");
	// LoP
	hide_widget(main_window,"NearLoPRedLed");
	show_widget(main_window,"NearLoPLed");
	// LoS
	hide_widget(main_window,"NearLoSRedLed");
	show_widget(main_window,"NearLoSLed");
	// LoF
	hide_widget(main_window,"NearLoFRedLed");
	show_widget(main_window,"NearLoFLed");
	// LoCD
	hide_widget(main_window,"NearLoCDRedLed");
	show_widget(main_window,"NearLoCDLed");

	// Far End
	// LoM
	hide_widget(main_window,"FarLoMRedLed");
	show_widget(main_window,"FarLoMLed");
	// LoP
	hide_widget(main_window,"FarLoPRedLed");
	show_widget(main_window,"FarLoPLed");
	// LoS
	hide_widget(main_window,"FarLoSRedLed");
	show_widget(main_window,"FarLoSLed");
	// LoF
	hide_widget(main_window,"FarLoFRedLed");
	show_widget(main_window,"FarLoFLed");
	// LoCD
	hide_widget(main_window,"FarLoCDRedLed");
	show_widget(main_window,"FarLoCDLed");
	// Dying Gasp
	hide_widget(main_window,"DyingGaspRedLed");
	show_widget(main_window,"DyingGaspLed");
	

	/* Test Panel */
	gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(main_window),"GenerateLBcellsButton"),FALSE);

	label_set_number(main_window,"CCText",last_oam_counters.rx_CC);
	label_set_number(main_window,"RDIText",last_oam_counters.rx_RDI);
	label_set_number(main_window,"AISText",last_oam_counters.rx_AIS);
	label_set_number(main_window,"FeLBText",last_oam_counters.rx_fe_LB);
	label_set_number(main_window,"NeLBText",last_oam_counters.rx_ne_LB);

	/* Config Panel */
	read_adsl_config("/etc/sysconfig/adsl-config");
	label_set_text(main_window,"ConfigModulationText",modulation_string(AdslConfig.modulation));
	label_set_number(main_window,"ConfigVpiText",AdslConfig.vpi);
	label_set_number(main_window,"ConfigVciText",AdslConfig.vci);
	label_set_text(main_window,"ConfigProtocolText",AdslConfig.protocol);
	label_set_text(main_window,"ConfigEncapsText",AdslConfig.encaps);
	

	/* info panel */	
	label_set_text(main_window,"PackageNameText","BeWAN ADSL");
	{
		unsigned long driver_version;
		char str[8];
		get_driver_version(&driver_version);
		g_snprintf(str,8,"%ld.%ld.%ld",
			   (driver_version>>8)&0xf,(driver_version>>4)&0xf,driver_version&0xf);
		label_set_text(main_window,"PackageVersionText",str);
	}
	{
	  get_version(&version);
	  version.versionA[sizeof(version.versionA)-1] = 0;
	  label_set_text(main_window,"FirmwareVersionText",version.versionA);
	}

	label_set_text(main_window,"ManufacturerText","BeWAN systems");
	label_set_text(main_window,"CopyrightText","BeWAN systems 2004");
}


static void update_panels(GtkWidget *main_window)
{
	T_StateInfo info;
	T_AMSW_def_counter_set dcs;
	T_AMSW_def_bitmap_set dbs;
	T_oam_stats oam_counters;

	/* Status panel */
	get_state_info(&info);
	if (info.State != last_info.State) {
		gtk_label_set_text(GTK_LABEL(lookup_widget(GTK_WIDGET(main_window),
							   "ModemStateText")),
				   modem_state_string(info.State));
		last_info.State = info.State;
		if (info.State == C_AMSW_IDLE) {
			gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(main_window),"StartButton"),TRUE);
			gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(main_window),"StopButton"),FALSE);
		} else {
			gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(main_window),"StartButton"),FALSE);
			gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(main_window),"StopButton"),TRUE);
		}
		if (info.State == C_AMSW_SHOWTIME_L0) {
			state_showtime(main_window);
		} else {
			state_idle(main_window);
		}
	}
	if (info.Report != last_info.Report) {
		label_set_text(main_window,"RemoteReportText",modem_event_string(info.Report));
		last_info.Report = info.Report;
	}
	if (info.Failure != last_info.Failure) {
		label_set_text(main_window,"LastFailureText",modem_failure_string(info.Failure));
		last_info.Failure = info.Failure;
	}
	if (info.TimeCnctd != last_info.TimeCnctd) {
		if (info.TimeCnctd == 0L) {
			label_set_text(main_window,"TimeConnectedText","--:--:--");
		} else {
			unsigned long t = info.TimeCnctd/1000UL;
			if (t != last_info.TimeCnctd/1000UL) {
				char time_str[16];
				unsigned long h = t/3600;
				unsigned long s = t%3600;
				unsigned long m = s/60;
				s = s%60;
				g_snprintf(time_str,16,"%2.2ld:%2.2ld:%2.2ld",h,m,s);
				label_set_text(main_window,"TimeConnectedText",time_str);
			}
		}
		last_info.TimeCnctd = info.TimeCnctd;		
	}
	
	/* Errors panel */
	if (info.State == C_AMSW_SHOWTIME_L0) {
		get_counters(&dcs);
	} else {
		memset(&dcs,0,sizeof(dcs));
	}
	if (last_dcs.near_end.FecNotInterleaved != dcs.near_end.FecNotInterleaved) {
		label_set_number(main_window,"LocFastFECText", dcs.near_end.FecNotInterleaved);
		last_dcs.near_end.FecNotInterleaved = dcs.near_end.FecNotInterleaved;
	}
	if (last_dcs.near_end.CrcNotInterleaved != dcs.near_end.CrcNotInterleaved) {
		label_set_number(main_window,"LocFastCRCText", dcs.near_end.CrcNotInterleaved);
		last_dcs.near_end.CrcNotInterleaved = dcs.near_end.CrcNotInterleaved;
	}
	if (last_dcs.near_end.HecNotInterleaved != dcs.near_end.HecNotInterleaved) {
		label_set_number(main_window,"LocFastHECText", dcs.near_end.HecNotInterleaved);
		last_dcs.near_end.HecNotInterleaved = dcs.near_end.HecNotInterleaved;
	}
	if (last_dcs.far_end.FecNotInterleaved != dcs.far_end.FecNotInterleaved) {
		label_set_number(main_window,"DisFastFECText", dcs.far_end.FecNotInterleaved);
		last_dcs.far_end.FecNotInterleaved = dcs.far_end.FecNotInterleaved;
	}
	if (last_dcs.far_end.CrcNotInterleaved != dcs.far_end.CrcNotInterleaved) {
		label_set_number(main_window,"DisFastCRCText", dcs.far_end.CrcNotInterleaved);
		last_dcs.far_end.CrcNotInterleaved = dcs.far_end.CrcNotInterleaved;
	}
	if (last_dcs.far_end.HecNotInterleaved != dcs.far_end.HecNotInterleaved) {
		label_set_number(main_window,"DisFastHECText", dcs.far_end.HecNotInterleaved);
		last_dcs.far_end.HecNotInterleaved = dcs.far_end.HecNotInterleaved;
	}
	if (last_dcs.near_end.FecInterleaved != dcs.near_end.FecInterleaved) {
		label_set_number(main_window,"LocItlvFECText", dcs.near_end.FecInterleaved);
		last_dcs.near_end.FecInterleaved = dcs.near_end.FecInterleaved;
	}
	if (last_dcs.near_end.CrcInterleaved != dcs.near_end.CrcInterleaved) {
		label_set_number(main_window,"LocItlvCRCText", dcs.near_end.CrcInterleaved);
		last_dcs.near_end.CrcInterleaved = dcs.near_end.CrcInterleaved;
	}
	if (last_dcs.near_end.HecInterleaved != dcs.near_end.HecInterleaved) {
		label_set_number(main_window,"LocItlvHECText", dcs.near_end.HecInterleaved);
		last_dcs.near_end.HecInterleaved = dcs.near_end.HecInterleaved;
	}
	if (last_dcs.far_end.FecInterleaved != dcs.far_end.FecInterleaved) {
		label_set_number(main_window,"DisItlvFECText", dcs.near_end.FecInterleaved);
		last_dcs.far_end.FecInterleaved = dcs.far_end.FecInterleaved;
	}
	if (last_dcs.far_end.CrcInterleaved != dcs.far_end.CrcInterleaved) {
		label_set_number(main_window,"DisItlvCRCText", dcs.near_end.CrcInterleaved);
		last_dcs.far_end.CrcInterleaved = dcs.far_end.CrcInterleaved;
	}
	if (last_dcs.far_end.HecInterleaved != dcs.far_end.HecInterleaved) {
		label_set_number(main_window,"DisItlvHECText", dcs.near_end.HecInterleaved);
		last_dcs.far_end.HecInterleaved = dcs.far_end.HecInterleaved;
	}

	/* Defects panel */
	get_bitmaps( &dbs );

	if (dbs.near_end.status != last_dbs.near_end.status) {
		// LoM
		if (dbs.near_end.status & LOM) {
			hide_widget(main_window,"NearLoMLed");
			show_widget(main_window,"NearLoMRedLed");
		} else {
			hide_widget(main_window,"NearLoMRedLed");
			show_widget(main_window,"NearLoMLed");
		}
		// LoP
		if (dbs.near_end.status & 0x20) {
			hide_widget(main_window,"NearLoPLed");
			show_widget(main_window,"NearLoPRedLed");
		} else {
			hide_widget(main_window,"NearLoPRedLed");
			show_widget(main_window,"NearLoPLed");
		}
		// LoS
		if (dbs.near_end.status & LOS) {
			hide_widget(main_window,"NearLoSLed");
			show_widget(main_window,"NearLoSRedLed");
		} else {
			hide_widget(main_window,"NearLoSRedLed");
			show_widget(main_window,"NearLoSLed");
		}
		// LoF
		if (dbs.near_end.status & LOF) {
			hide_widget(main_window,"NearLoFLed");
			show_widget(main_window,"NearLoFRedLed");
		} else {
			hide_widget(main_window,"NearLoFRedLed");
			show_widget(main_window,"NearLoFLed");
		}
		// LoCD
		if (dbs.near_end.status & (LCDI | LCDNI)) {
			hide_widget(main_window,"NearLoCDLed");
			show_widget(main_window,"NearLoCDRedLed");
		} else {
			hide_widget(main_window,"NearLoCDRedLed");
			show_widget(main_window,"NearLoCDLed");
		}

		last_dbs.near_end.status = dbs.near_end.status;
	}
	
	// Far End
	if (dbs.far_end.status != last_dbs.far_end.status) {
		// LoM
		if (dbs.far_end.status & LOM) {
			hide_widget(main_window,"FarLoMLed");
			show_widget(main_window,"FarLoMRedLed");
		} else {
			hide_widget(main_window,"FarLoMRedLed");
			show_widget(main_window,"FarLoMLed");
		}
		// LoP
		if (dbs.far_end.status & 0x20) {
			hide_widget(main_window,"FarLoPLed");
			show_widget(main_window,"FarLoPRedLed");
		} else {
			hide_widget(main_window,"FarLoPRedLed");
			show_widget(main_window,"FarLoPLed");
		}
		// LoS
		if (dbs.far_end.status & LOS) {
			hide_widget(main_window,"FarLoSLed");
			show_widget(main_window,"FarLoSRedLed");
		} else {
			hide_widget(main_window,"FarLoSRedLed");
			show_widget(main_window,"FarLoSLed");
		}
		// LoF
		if (dbs.far_end.status & LOF) {
			hide_widget(main_window,"FarLoFLed");
			show_widget(main_window,"FarLoFRedLed");
		} else {
			hide_widget(main_window,"FarLoFRedLed");
			show_widget(main_window,"FarLoFLed");
		}
		// LoCD
		if (dbs.far_end.status & (LCDI | LCDNI)) {
			hide_widget(main_window,"FarLoCDLed");
			show_widget(main_window,"FarLoCDRedLed");
		} else {
			hide_widget(main_window,"FarLoCDRedLed");
			show_widget(main_window,"FarLoCDLed");
		}
		// Dying Gasp
		if (dbs.far_end.status & 0x100) {
			hide_widget(main_window,"DyingGaspLed");
			show_widget(main_window,"DyingGaspRedLed");
		} else {
			hide_widget(main_window,"DyingGaspRedLed");
			show_widget(main_window,"DyingGaspLed");
		}

		last_dbs.far_end.status = dbs.far_end.status;
	}

	/* Test Panel */
	if (info.State == C_AMSW_SHOWTIME_L0) {
		get_oam_stats(&oam_counters);
	}
	if (oam_counters.rx_CC != last_oam_counters.rx_CC) {
		label_set_number(main_window,"CCText",oam_counters.rx_CC);
		last_oam_counters.rx_CC = oam_counters.rx_CC;
	}
	if (oam_counters.rx_RDI != last_oam_counters.rx_RDI) {
		label_set_number(main_window,"RDIText",oam_counters.rx_RDI);
		last_oam_counters.rx_RDI = oam_counters.rx_RDI;
	}
	if (oam_counters.rx_AIS != last_oam_counters.rx_AIS) {
		label_set_number(main_window,"AISText",oam_counters.rx_AIS);
		last_oam_counters.rx_AIS = oam_counters.rx_AIS;
	}
	if (oam_counters.rx_fe_LB != last_oam_counters.rx_fe_LB) {
		label_set_number(main_window,"FeLBText",oam_counters.rx_fe_LB);
		last_oam_counters.rx_fe_LB = oam_counters.rx_fe_LB;
	}
	if (oam_counters.rx_ne_LB != last_oam_counters.rx_ne_LB) {
		label_set_number(main_window,"NeLBText",oam_counters.rx_ne_LB);
		last_oam_counters.rx_ne_LB = oam_counters.rx_ne_LB;
	}
}


/* 
   PowerChart
*/ 
void create_chart(GtkWidget *drawing_area)
{
	float mXmin;			// Minimum X Value
	float mXmax;			// Maximum X value
	float mYmin;			// Minimum Y value
	float mYmax;			// Maximum Y value
	float yscale;
	int n;

	mXmin = 0;
	mXmax = 1104;	// kHz
	mYmin = 10;
	mYmax = 90;	// dBm

	if (pixmap)
		gdk_pixmap_unref(pixmap);
	pixmap = gdk_pixmap_new(drawing_area->window,
				drawing_area->allocation.width,
				drawing_area->allocation.height,
				-1);
  
	gdk_draw_rectangle (pixmap,
			    drawing_area->style->black_gc,
			    TRUE,
			    0, 0,
			    drawing_area->allocation.width,
			    drawing_area->allocation.height);

	if (mCurves == 0) return;

	yscale = mYmax - mYmin;
	for (n=0; n < mCurves; n++) {
		int i;
		int x,y;
		int last_x=0,last_y=0;
		set_color(drawing_area,&chart_colors[n]);
		for (i=0; i < MAXVAL; i++) {
			x = i*drawing_area->allocation.width/MAXVAL;
			y = drawing_area->allocation.height - 
				((mCurve[n][i]-mYmin)*drawing_area->allocation.height/yscale);
			if (y >= drawing_area->allocation.height) y = drawing_area->allocation.height;
			if (i==0) {
				gdk_draw_point(pixmap,
					       drawing_area->style->fg_gc[GTK_STATE_NORMAL],
					       x,y);
			} else {
				gdk_draw_line(pixmap,
					      drawing_area->style->fg_gc[GTK_STATE_NORMAL],
					      last_x,last_y,
					      x,y);
			}
			last_x=x;
			last_y=y;
		}
	}
}

void draw_chart(GtkWidget *drawing_area,GdkEventExpose *event)
{
	gdk_draw_pixmap(drawing_area->window,
			drawing_area->style->fg_gc[GTK_WIDGET_STATE (drawing_area)],
			pixmap,
			event->area.x, event->area.y,
			event->area.x, event->area.y,
			event->area.width, event->area.height);
}


void create_RxPathLabel(GtkWidget *label)
{
	set_color(label,&chart_colors[1]);
}

extern void create_SNRLabel(GtkWidget *label) 
{
	set_color(label,&chart_colors[2]);
}

extern void create_EchoNoiseLabel(GtkWidget *label)
{
	set_color(label,&chart_colors[0]);
}


extern int start_msw()
{
	T_MswCtrl ctrl = {MSW_CTL_START,0,0,NULL,0};
	return msw_ctrl(&ctrl);       
}

extern void stop_msw()
{
	T_MswCtrl ctrl = {MSW_CTL_STOP,0,0,NULL,0};
	msw_ctrl(&ctrl);
}

extern int start_send_oam_lb(void)
{
	g_vpi = label_get_number(g_main_window,"VPIEntry");
	g_vci = label_get_number(g_main_window,"VCIEntry");
	g_send_oam = 1;
	return 0;
}

extern void stop_send_oam_lb(void)
{
	g_send_oam = 0;
}

extern void oam_reset_counters(void)
{
	T_MswCtrl ctrl = {NET_CTL_RESET_OAM_STATS,0,0,0,0};
	msw_ctrl(&ctrl);
}

extern void status_exit(void)
{
	close_device(&g_device);
}

gint periodic_callback(gpointer data)
{
	static int counter=0;
	GtkWidget *main_window = (GtkWidget *)data;

	update_panels(main_window);
	if ((counter % 10) == 0) {
		// 1 second
		if (g_send_oam) {
			T_MswCtrl ctrl;
			T_atm_channel tx_oam;

			if ( g_vpi==0) {
				tx_oam.type = ATM_OAM_F4;
			} else {
				tx_oam.type = ATM_OAM_F5;
			}
			tx_oam.vpi = g_vpi;
			tx_oam.vci = g_vci;
			ctrl.code = NET_CTL_TX_OAM_CELL;
			ctrl.subcode = 0;
			ctrl.buffer = &tx_oam;
			ctrl.length = sizeof(tx_oam);

			msw_ctrl(&ctrl);
		}
		
	}
	counter++;
	return TRUE;
}

int status_init(GtkWidget *main_window)
{
	GtkWidget *label;
	
	g_main_window = main_window;
	
	if (open_device(&g_device) < 0) {
		label = lookup_widget(GTK_WIDGET(main_window),"LastFailureText");
		gtk_label_set_text(GTK_LABEL(label),_("No hardware detected"));
		return -errno;
	} 

	state_idle(main_window);

	periodic_callback(main_window);
	gtk_timeout_add(100,periodic_callback,main_window);
	return 0;
}
