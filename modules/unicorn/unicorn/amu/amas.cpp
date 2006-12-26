#include "types.h"
#include "tracetool.h"
#include "amas.h"				/* AMAS API's and typedefs */
#include "rapi.h"

extern unsigned long	g_ModemState, g_ShowtimeCounter;
extern unsigned int 	g_WaitForShowtime, g_WaitForInit;
extern bool				L3_flag;
									
extern T_AMSW_ANT_StaticConfiguration	g_StaticCfg;
extern T_AMSW_NT_NearEndLineOperData 	g_NearEndLineOperData;
extern T_AMSW_NT_FarEndLineOperData 	g_FarEndLineOperData;
extern T_AMSW_def_counter_set 			g_def_counter_set;
extern T_AMSW_def_bitmap_set 			g_def_bitmap_set;
extern T_AMSW_def_counters				g_def_counters;
extern T_AMSW_NT_ChannelOperData 		g_ChannelOperData;
extern T_AMSW_ANT_CustomerConfiguration	g_CustomerCfg;
extern T_AMSW_PowerStateConfiguration 	g_PowerStateCfg;
extern T_AMSW_Teq						g_Teq;
extern T_AMSW_Ber						g_Ber;
extern T_AMSW_VersionMS					g_VersionMS;

// ADSL Modem Software calls this function to report any state changes
void AMSW_ANT_reportModemStateChange(AMSW_ModemState p_ModemState)
{
	g_ModemState = p_ModemState;

	char *s;
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
	PRINT_ERROR("Current Modem State (%d): %s\n", g_ModemState, s);

	msw_report_event(MSW_EVENT_STATE,p_ModemState);
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
	case C_AMSW_L3_EXECUTED            : s = "L3 Executed"; 
		g_ModemState = C_AMSW_IDLE;
		L3_flag = TRUE;
		break;
	case C_AMSW_L3_REJECTED            : s = "L3 Rejected";       
		L3_flag = TRUE;
		break;
	case C_AMSW_L1_EXECUTED            : s = "L1 Executed";            break;
	case C_AMSW_L1_REJECTED            : s = "L1 Rejected";            break;
	case C_AMSW_L0_REJECTED            : s = "L0 Rejected";            break;
	case C_AMSW_RESTART_ACCEPTABLE     : s = "Restart Acceptable";     break;
	case C_AMSW_RESTART_NOT_ACCEPTABLE : s = "Restart Not Acceptable"; break;

	case C_AMSW_TO_INITIALIZING        : s = "Initializing";
		g_WaitForInit = 0;
		g_WaitForShowtime = 0;
		break;

	case C_AMSW_SHOWTIME               : s = "Showtime";
		g_WaitForShowtime = 0;
		g_ShowtimeCounter = 0;	
		break;

	case C_AMSW_SUICIDE_REQUEST        : s = "Suicide Request"; 
		// orderly shutdown request from peer ATU
		g_ModemState = C_AMSW_DISORDERLY;
		break;

	}
	PRINT_ERROR("Event Reported (%d): %s\n", (int)p_Event, s);

	msw_report_event(MSW_EVENT_REPORT,p_Event);
}

// ADSL Modem Software calls this function to report the reason for failure
void AMSW_ANT_reportModemFailure(AMSW_ModemFailure p_FailureCause)
{
	char *s;
	switch(p_FailureCause)
	{
	case C_AMSW_UNCOMPATIBLE_LINECONDITIONS :
		s = "Uncompatible Line Conditions";
		break;

	case C_AMSW_NO_LOCK_POSSIBLE :
		s = "No Lock Possible";
		break;

	case C_AMSW_PROTOCOL_ERROR :
		s = "Protocol Error";
		break;

	case C_AMSW_MESSAGE_ERROR :
		s = "Message Error";
		break;

	case C_AMSW_SPURIOUS_ATU_DETECTED :
		s = "Spurious ATU Detected";
		break;

	case C_AMSW_FORCED_SILENCE :
		s = "Forced Silence";
		break;

	case C_AMSW_DS_REQ_BITRATE_TOO_HIGH_FOR_LITE :
		s = "Requested Bit Rate Too High";
		break;

	case C_AMSW_INTERLEAVED_PROFILE_REQUIRED_FOR_LITE :
		s = "Interleaved Profile Required for LITE";
		break;

	case C_AMSW_UNSELECTABLE_OPERATION_MODE :
		s = "Unselectable Operation Mode";
		break;

	case C_AMSW_STATE_REFUSED_BY_GOLDEN :
		s = "State Refused By Golden";
		break;

	default : 
		s = "Unknown Cause";
		break;
	}

	// Disorderly shutdown
	PRINT_ERROR("Modem Init Failure: %s (%d)\n", s, p_FailureCause); 
	g_ModemState = C_AMSW_DISORDERLY;

	msw_report_event(MSW_EVENT_FAILURE,p_FailureCause);
}

AMSW_ResultCode AMSW_ANT_wait_event(unsigned long *event)
{
  *event= last_report;
  return 0;
}

