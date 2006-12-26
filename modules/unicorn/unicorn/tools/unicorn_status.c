#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "amu.h"
#include "unicorn.h"
#include "unicorn_status.h"


static const char *modem_state_string(AMSW_ModemState modemState)
{
	switch(modemState) {
	case C_AMSW_IDLE: return ("IDLE");
	case C_AMSW_L3:  return ("L3");
	case C_AMSW_LISTENING:  return ("LISTENING");
	case C_AMSW_ACTIVATING:  return ("ACTIVATING");
	case C_AMSW_Ghs_HANDSHAKING:  return ("Ghs_HANDSHAKING");
	case C_AMSW_ANSI_HANDSHAKING:  return ("ANSI_HANDSHAKING");
	case C_AMSW_INITIALIZING:  return ("INITIALIZING");
	case C_AMSW_RESTARTING:  return ("RESTARTING");
	case C_AMSW_FAST_RETRAIN:  return ("FAST_RETRAIN");
	case C_AMSW_SHOWTIME_L0:  return ("SHOWTIME_L0");
	case C_AMSW_SHOWTIME_LQ:  return ("SHOWTIME_LQ");
	case C_AMSW_SHOWTIME_L1:  return ("SHOWTIME_L1");
	case C_AMSW_EXCHANGE:  return ("EXCHANGE");
	case C_AMSW_TRUNCATE:  return ("TRUNCATE");
	case C_AMSW_ESCAPE:  return ("ESCAPE");
	case C_AMSW_RETRY:  return ("RETRY");
	case C_AMSW_DISORDERLY:  return ("DISORDERLY");
	default: return (" ");
	}
}

static const char *modem_event_string(AMSW_ModemEvent event)
{
	switch(event) {
	case C_AMSW_PEER_ATU_FOUND: return ("Peer ATU Found");
	case C_AMSW_RESTART_REQUEST: return ("Restart Request");
	case C_AMSW_ACTIVATION_REQUEST: return ("Activation Request");
	case C_AMSW_TO_INITIALIZING: return ("Initializing");
	case C_AMSW_SHOWTIME: return ("Showtime");
	case C_AMSW_L3_EXECUTED: return ("L3 Executed");
	case C_AMSW_L3_REJECTED: return ("L3 Rejected");
	case C_AMSW_L1_EXECUTED: return ("L1 Executed");
	case C_AMSW_L1_REJECTED: return ("L1 Rejected");
	case C_AMSW_L0_REJECTED: return ("L0 Rejected");
	case C_AMSW_RESTART_ACCEPTABLE: return ("Restart Acceptable");
	case C_AMSW_SUICIDE_REQUEST: return ("Suicide Request");
	case C_AMSW_RESTART_NOT_ACCEPTABLE: return ("Restart Not Acceptable");
	default: return (" ");
	}
}

static const char *modem_failure_string(AMSW_ModemFailure failure)
{
	switch(failure) {
	case C_AMSW_UNCOMPATIBLE_LINECONDITIONS: return ("Uncompatible Line Conditions");
	case C_AMSW_NO_LOCK_POSSIBLE: return ("No Lock Possible");
	case C_AMSW_PROTOCOL_ERROR: return ("Protocol Error");
	case C_AMSW_MESSAGE_ERROR: return ("Message Error");
	case C_AMSW_SPURIOUS_ATU_DETECTED: return ("Spurious ATU Detected");
	case C_AMSW_DS_REQ_BITRATE_TOO_HIGH_FOR_LITE: return ("Requested Bit Rate Too High");
	case C_AMSW_INTERLEAVED_PROFILE_REQUIRED_FOR_LITE: return ("Interleaved Profile Required for LITE");
	case C_AMSW_FORCED_SILENCE: return ("Forced Silence");
	case C_AMSW_UNSELECTABLE_OPERATION_MODE: return ("Unselectable Operation Mode");
	case C_AMSW_STATE_REFUSED_BY_GOLDEN: return ("State Refused By Golden");
	case C_AMSW_AMU_EVENT_ACT_TIMEOUT: return ("Activation Timeout");
	case C_AMSW_AMU_EVENT_INI_TIMEOUT: return ("Initialization Timeout");
	case C_AMSW_AMU_EVENT_SHUTDOWN: return ("Bringing Down Line due to persistent faults");
	case C_AMSW_EVENT_RETRY: return ("Retry");
	case C_AMSW_UNKNOWN: return ("Unknown");
	case C_AMSW_UNKNOWN_FAILURE: return ("Unknown failure");
	case C_AMSW_NO_HARDWARE: return ("No hardware detected");
	case C_AMSW_NO_USB_BANDWIDTH: return ("USB bandwidth unavailable");
	default: return (" ");
	}
}

static const char *modulation_string(unsigned long operationalMode)
{
	switch(operationalMode) {
	case AMSW_ANSI: return ("ANSI");
	case AMSW_G_LITE: return ("G.lite");
	case AMSW_G_DMT: return ("G.dmt");
	default: return (" ");
	}
}

static const char *time_connected_string(unsigned long TimeCnctd)
{
		if (TimeCnctd == 0L) {
			return ("--:--:--");
		} else {
			unsigned long t = TimeCnctd/1000UL;
			static char time_str[16];
			unsigned long h = t/3600;
			unsigned long s = t%3600;
			unsigned long m = s/60;
			s = s%60;
			snprintf(time_str,16,"%2.2ld:%2.2ld:%2.2ld",h,m,s);
			return time_str;
		}
}


AMSW_ModemState get_modem_state(ADSL_DEVICE *device)
{
	AMSW_ModemState state=C_AMSW_IDLE;
	T_MswCtrl ctrl = {MSW_CTL_GET_STATE,0,0,&state,sizeof(state)};
	msw_ctrl(device,&ctrl);
	return state;
}


static void get_state_info(ADSL_DEVICE *device,T_StateInfo *info)
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

	msw_ctrl(device,&ctrl);
}

static void get_bit_rates(ADSL_DEVICE *device,unsigned short *bit_rates)
{
	int i;

	for (i=0; i < 4; i++) {
		T_AMSW_NT_ChannelOperData cod;
		T_MswCtrl ctrl = 
			{MSW_CTL_GET_DATA,C_AMSW_NEAR_END_CHANNEL_DATA_FAST+i,
			 0,&cod,sizeof(T_AMSW_NT_ChannelOperData)};
		bit_rates[i] = 0;
		msw_ctrl(device,&ctrl);
		if (ctrl.retcode == 0) {
			bit_rates[i] = cod.actualBitrate;
		} else {
			bit_rates[i] = 0;
		}
	}	
}

static void get_near_end_line_oper_data(ADSL_DEVICE *device,T_AMSW_NT_NearEndLineOperData *nelod)
{	
	T_MswCtrl ctrl = {MSW_CTL_GET_DATA,C_AMSW_NEAR_END_LINE_DATA,0,
			  nelod,sizeof(T_AMSW_NT_NearEndLineOperData)};
	memset(nelod,0,sizeof(T_AMSW_NT_NearEndLineOperData));
	msw_ctrl(device,&ctrl);
}

static void get_far_end_line_oper_data(ADSL_DEVICE *device,T_AMSW_NT_FarEndLineOperData *felod)
{	
	T_MswCtrl ctrl = {MSW_CTL_GET_DATA,C_AMSW_FAR_END_LINE_DATA,0,
			  felod,sizeof(T_AMSW_NT_FarEndLineOperData)};
	memset(felod,0,sizeof(T_AMSW_NT_FarEndLineOperData));
	msw_ctrl(device,&ctrl);
}

static void get_counters(ADSL_DEVICE *device,T_AMSW_def_counter_set *dcs)
{	
	T_MswCtrl ctrl = {MSW_CTL_GET_DATA,C_AMSW_PM_DATA,0,dcs,sizeof(T_AMSW_def_counter_set)};
	memset(dcs,0,sizeof(T_AMSW_def_counters));
	msw_ctrl(device,&ctrl);
}


const char *get_modem_state_string(ADSL_DEVICE *device)
{
	return modem_state_string(get_modem_state(device));	
}

const char *get_modem_event_string(ADSL_DEVICE *device)
{
	T_StateInfo info;

	get_state_info(device,&info);
	return modem_event_string(info.Report);	
}

const char *get_modem_failure_string(ADSL_DEVICE *device)
{
	T_StateInfo info;

	get_state_info(device,&info);
	return modem_failure_string(info.Failure);	
}

const char *get_time_connected_string(ADSL_DEVICE *device)
{
	T_StateInfo info;

	get_state_info(device,&info);
	return time_connected_string(info.TimeCnctd);	
}

unsigned long long get_cells_rcvd(ADSL_DEVICE *device)
{	
	T_StateInfo info;

	get_state_info(device,&info);
	return info.RcvdCells;
}

unsigned long long get_cells_sent(ADSL_DEVICE *device)
{	
	T_StateInfo info;

	get_state_info(device,&info);
	return info.SentCells;
}

const char *get_modulation_string(ADSL_DEVICE *device)
{
	T_AMSW_NT_NearEndLineOperData nelod;
	
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_near_end_line_oper_data(device,&nelod);
		return modulation_string(nelod.operationalMode);
	} else {
		return "";
	}
}

int get_us_rate(ADSL_DEVICE *device)
{
	short bit_rates[4];
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_bit_rates(device,bit_rates);
		return bit_rates[1]+bit_rates[3];
	} else {
		return 0;
	}
}

int get_ds_rate(ADSL_DEVICE *device)
{
	short bit_rates[4];
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_bit_rates(device,bit_rates);
		return bit_rates[0]+bit_rates[2];
	} else {
		return 0;
	}
}

int get_us_cap_occ(ADSL_DEVICE *device)
{
	T_AMSW_NT_FarEndLineOperData felod;
	
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_far_end_line_oper_data(device,&felod);
		return felod.relCapacityOccupationUpstr;
	} else {
		return 0;
	}
}

int get_ds_cap_occ(ADSL_DEVICE *device)
{
	T_AMSW_NT_NearEndLineOperData nelod;
	
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_near_end_line_oper_data(device,&nelod);
		return nelod.relCapacityOccupationDnstr;
	} else {
		return 0;
	}
}

int get_us_noise_margin(ADSL_DEVICE *device)
{
	T_AMSW_NT_FarEndLineOperData felod;
	
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_far_end_line_oper_data(device,&felod);
		return felod.noiseMarginUpstr/2;
	} else {
		return 0;
	}
}

int get_ds_noise_margin(ADSL_DEVICE *device)
{
	T_AMSW_NT_NearEndLineOperData nelod;
	
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_near_end_line_oper_data(device,&nelod);
		return nelod.noiseMarginDnstr/2;
	} else {
		return 0;
	}
}

int get_us_attenuation(ADSL_DEVICE *device)
{
	T_AMSW_NT_FarEndLineOperData felod;
	
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_far_end_line_oper_data(device,&felod);
		return felod.attenuationUpstr/2;
	} else {
		return 0;
	}
}

int get_ds_attenuation(ADSL_DEVICE *device)
{
	T_AMSW_NT_NearEndLineOperData nelod;
	
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_near_end_line_oper_data(device,&nelod);
		return nelod.attenuationDnstr/2;
	} else {
		return 0;
	}
}

int get_us_output_power(ADSL_DEVICE *device)
{
	T_AMSW_NT_NearEndLineOperData nelod;
	
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_near_end_line_oper_data(device,&nelod);
		return  nelod.outputPowerUpstr/2;
	} else {
		return 0;
	}
}

int get_ds_output_power(ADSL_DEVICE *device)
{
	T_AMSW_NT_FarEndLineOperData felod;
	
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_far_end_line_oper_data(device,&felod);
		return felod.outputPowerDnstr/2;
	} else {
		return 0;
	}
}

int get_us_fec_errors(ADSL_DEVICE *device)
{
	T_AMSW_def_counter_set dcs;
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_counters(device,&dcs);
		return dcs.near_end.FecNotInterleaved+dcs.near_end.FecInterleaved;
	} else {
		return 0;
	}
}

int get_ds_fec_errors(ADSL_DEVICE *device)
{
	T_AMSW_def_counter_set dcs;
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_counters(device,&dcs);
		return dcs.far_end.FecNotInterleaved+dcs.far_end.FecInterleaved;
	} else {
		return 0;
	}
}

int get_us_crc_errors(ADSL_DEVICE *device)
{
	T_AMSW_def_counter_set dcs;
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_counters(device,&dcs);
		return dcs.near_end.CrcNotInterleaved+dcs.near_end.CrcInterleaved;
	} else {
		return 0;
	}
}

int get_ds_crc_errors(ADSL_DEVICE *device)
{
	T_AMSW_def_counter_set dcs;
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_counters(device,&dcs);
		return dcs.far_end.CrcNotInterleaved+dcs.far_end.CrcInterleaved;
	} else {
		return 0;
	}
}

int get_us_hec_errors(ADSL_DEVICE *device)
{
	T_AMSW_def_counter_set dcs;
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_counters(device,&dcs);
		return dcs.near_end.HecNotInterleaved+dcs.near_end.HecInterleaved;
	} else {
		return 0;
	}
}

int get_ds_hec_errors(ADSL_DEVICE *device)
{
	T_AMSW_def_counter_set dcs;
	if (get_modem_state(device) == C_AMSW_SHOWTIME_L0) {
		get_counters(device,&dcs);
		return dcs.far_end.HecNotInterleaved+dcs.far_end.HecInterleaved;
	} else {
		return 0;
	}
}

const char *get_version_string(ADSL_DEVICE *device)
{
	static T_AMSW_VersionMS version;
	T_MswCtrl ctrl = {MSW_CTL_GET_DATA,C_AMSW_VERSIONMS,0,&version,sizeof(T_AMSW_VersionMS)};
	
	version.versionA[0] = 0;
	msw_ctrl(device,&ctrl);
	return version.versionA;
}

const char *get_driver_version_string(ADSL_DEVICE *device)
{
	unsigned long driver_version=0;
	static char s[16];
	T_MswCtrl ctrl = {MSW_CTL_VERSION_INFO,0,0,&driver_version,sizeof(driver_version)};
	msw_ctrl(device,&ctrl);
	sprintf(s,"%ld.%ld.%ld",(driver_version>>8)&0xf,(driver_version>>4)&0xf,driver_version&0xf);
	return s;
}

