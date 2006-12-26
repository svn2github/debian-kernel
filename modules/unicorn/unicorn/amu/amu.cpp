#include "types.h"
#include "tracetool.h"
#include "rapi.h"  
#include "hard.h"
#include "hal.h"
#include "amas.h"
#include "amu.h"
#define _PICAP_CODE_
#ifdef _PICAP_CODE_
extern long Vendor_Id_code_ECI;
#endif



//#define PM_FM_POLLING_RATE          1000// milliseconds
//#define INIT_POLLING_TIME           2    // (INIT_POLLING_TIME* PM_FM_POLLING_RATE) milliseconds
//#define WAITFOR_SHOWTIME_COUNT      20   // (WAITFOR_SHOWTIME_COUNT * PM_FM_POLLING_RATE) millisecs 
#define RETRY_WAIT_TIME_MIN_MSEC      5000 // 5 seconds (between line disable and reenable)

//#define WAITFOR_DISORDERLY_COUNT    2000   // (WAITFOR_DISORDERLY_COUNT * PM_FM_POLLING_RATE) msecs 

//#define NEAR_LCDNI_COUNT            15  // 15 seconds timeout of near end LCD persistency
//#define NEAR_LCDI_COUNT             15  // 15 seconds timeout of near end LCDI persistency
//#define NEAR_LOS_COUNT              5   // 5 seconds timeout of near end LOS persistency
//#define NEAR_LOF_COUNT              5   // 5 seconds timeout of near end LOF persistency
//#define FAR_LCDNI_COUNT             17  // 17 seconds timeout of far end LCD persistency
//#define FAR_LCDI_COUNT							17  // 17 seconds timeout of far end LCD persistency
//#define FAR_LOS_COUNT               5   // 6 seconds timeout of far end LOS persistency
//#define FAR_LOF_COUNT               5   // 6 seconds timeout of far end LOF persistency
//#define FAR_LOS_SHORT_COUNT         4   // 4 seconds timeout of far end LOS persistency
//#define FAR_LOF_SHORT_COUNT         4   // 4 seconds timeout of far end LOF persistency


//#define CRC_FAST_COUNT              5   // (CRC_FAST_COUNT * AMUTASK_MSG_WAIT_TIME) millisecs
//#define CRC_INTERLEAVED_COUNT       5   // (CRC_INTERLEAVED_COUNT * AMUTASK_MSG_WAIT_TIME) milli
//#define WATCHDOG_COUNTER_VALUE      500	// Time to reset = (WATCHDOG_COUNTER_VALUE * 2) * 8.192 
																					// milli sec

unsigned long NEAR_LCDNI_COUNT;         
unsigned long NEAR_LCDI_COUNT;          
unsigned long NEAR_LOS_COUNT;           
unsigned long NEAR_LOF_COUNT;           
unsigned long FAR_LCDNI_COUNT;          
unsigned long FAR_LCDI_COUNT;						
unsigned long FAR_LOS_COUNT;            
unsigned long FAR_LOF_COUNT;            
unsigned long FAR_LOS_SHORT_COUNT;      
unsigned long FAR_LOF_SHORT_COUNT;      

#if 0
//ModemSW variables
extern T_AMSW_Identification                g_Identification;
extern T_AMSW_NT_NearEndLineOperData        g_NearEndLineOperData;
extern T_AMSW_NT_FarEndLineOperData         g_FarEndLineOperData;
extern T_AMSW_def_counter_set               g_def_counter_set;
extern T_AMSW_def_bitmap_set                g_def_bitmap_set;
extern T_AMSW_def_counters                  g_def_counters;
extern T_AMSW_NT_ChannelOperData            g_ChannelOperData;
extern T_AMSW_ANT_CustomerConfiguration     g_CustomerCfg;
extern T_AMSW_ANT_StaticConfiguration       g_StaticCfg;
extern T_AMSW_PowerStateConfiguration       g_PowerStateCfg;   
extern T_AMSW_Teq g_Teq; //STM G
extern T_AMSW_Ber g_Ber;
extern T_AMSW_VersionMS g_VersionMS;
#endif

extern unsigned long DownstreamRate;	// In Kbits/sec	
unsigned long FmPollingRate = 1000;
unsigned long InitTimeout = 20000;
//unsigned long ActTimeout = 10000;
unsigned long ActTimeout = 300000;	// Fix for Alcatel 4.2.13
unsigned long RetryTime = 5000;

unsigned long LCD_Trig = 15000;
unsigned long LOS_LOF_Trig = 5000;

extern unsigned long Vendor_Id_code_Globspan;
unsigned long Vendor_Id_code_Globspan=0;

extern "C" void HandleAtmError(void);
extern "C" void HandleLeds(void);
	
unsigned long amu_init_modem(unsigned short MODE);
unsigned long amu_init_modem(unsigned short MODE)
{
	unsigned long l_RetCode;

	//
	// Static Configuration
	//
	g_StaticCfg.utopiaMode = C_AMSW_UTOPIA_LEVEL1;
	g_StaticCfg.utopiaFast = 0;
	g_StaticCfg.utopiaSlow = 0;
	for (l_RetCode=0; l_RetCode<32; l_RetCode++)
		g_StaticCfg.serialNumber[l_RetCode] = '9';
//	g_StaticCfg.maximumDownstreamLineRate = 5000;				
	g_StaticCfg.maximumDownstreamLineRate = DownstreamRate;
	g_StaticCfg.managementVersion = 1;
	g_StaticCfg.goldenMode = AMSW_GOLDEN_OFF;
	g_StaticCfg.vendorIdentif.countryCode = 0x0f;
	g_StaticCfg.vendorIdentif.reserved = 0x00;
	g_StaticCfg.vendorIdentif.vendorCode = (0x41 << 24) | (0x4c << 16) | (0x43 << 8) | (0x42); /*"ALCB"*/ //RFC016
//	g_StaticCfg.vendorIdentif.vendorCode = 0x3A; //STMicroelectronics
	g_StaticCfg.vendorIdentif.vendorSpecific = 0x0451;

	//
	//	Customer Configuration
	//

	switch(MODE)
	{
	case MSW_MODE_ANSI:
		g_CustomerCfg.POTSoverlayOperationModes = AMSW_ANSI;
		break;
	case MSW_MODE_GLITE:
		g_CustomerCfg.POTSoverlayOperationModes = AMSW_G_LITE;
		break;
	case MSW_MODE_GDMT:
		g_CustomerCfg.POTSoverlayOperationModes = AMSW_G_DMT;
		break;		
	case MSW_MODE_MULTI:	default:
		g_CustomerCfg.POTSoverlayOperationModes = AMSW_G_DMT | AMSW_ANSI | AMSW_G_LITE;
		break;
	}

	g_CustomerCfg.POTSoverlayPermissions[0] = AMSW_TRELLIS | AMSW_BITSWAP;   // ANSI
	
#ifdef SUICIDE_FIX	
	g_CustomerCfg.POTSoverlayPermissions[1] = AMSW_TRELLIS | AMSW_DS_PILOT_MODULATED | AMSW_POWER_MANAGEMENT;	    //DMT
#else
	g_CustomerCfg.POTSoverlayPermissions[1] = AMSW_TRELLIS | AMSW_DS_PILOT_MODULATED;							// DMT													
#endif
														
	g_CustomerCfg.POTSoverlayPermissions[2] = AMSW_LQ;																														// UAWG
	g_CustomerCfg.POTSoverlayPermissions[3] = AMSW_BITSWAP | AMSW_RS16 | AMSW_TRELLIS | AMSW_DS_PILOT_MODULATED ;	// G.LITE
	g_CustomerCfg.POTSoverlayPermissions[4] = 0;
	g_CustomerCfg.POTSoverlayPermissions[5] = 0;
	g_CustomerCfg.POTSoverlayPermissions[6] = 0;
	g_CustomerCfg.POTSoverlayPermissions[7] = 0;
	
	//
	// ISDN
	//
	g_CustomerCfg.ISDNoverlayOperationModes = 0;
	for (l_RetCode = 0; l_RetCode < 8; l_RetCode++)
		g_CustomerCfg.ISDNoverlayPermissions[l_RetCode] = 0;
	for (l_RetCode = 0; l_RetCode < 8; l_RetCode++)
		g_CustomerCfg.ISDNoverlayPermissions[l_RetCode] = 0;
		
	//
	// Power State Configuration
	//
	g_PowerStateCfg.powerStateControl = AMSW_L3 | AMSW_L1;

	// Setup modem configuration
	l_RetCode = AMSW_ANT_setModemConfiguration(C_AMSW_STATIC_CONFIGURATION,&g_StaticCfg);
	if (l_RetCode != C_AMSW_ACK)
		PRINT_ERROR("Error in AMSW_ANT_setModemConfiguration-1 (%d)\n", l_RetCode);
	l_RetCode = AMSW_ANT_setModemConfiguration(C_AMSW_CUSTOMER_CONFIGURATION,&g_CustomerCfg);
	if (l_RetCode != C_AMSW_ACK)
		PRINT_ERROR("Error in AMSW_ANT_setModemConfiguration-2 (%d)\n", l_RetCode);
	l_RetCode = AMSW_ANT_setModemConfiguration(C_AMSW_POWER_STATE_CONTROL,&g_PowerStateCfg);
	if (l_RetCode != C_AMSW_ACK)
		PRINT_ERROR("Error in AMSW_ANT_setModemConfiguration-3 (%d)\n", l_RetCode);
	return l_RetCode;
}

void AMUTask(unsigned long,unsigned long , unsigned long , unsigned long )
{
	unsigned long   l_RetCode = C_AMSW_REJ;
	//static int      l_Lit = 0;
	static bool     PM_toggle = TRUE;

	PRINT_ERROR("FmPollingRate=%ldms,InitTimeout=%ldms,ActTimeout=%ld\n",
		   FmPollingRate,InitTimeout,ActTimeout);

	unsigned long PM_FM_POLLING_RATE = FmPollingRate;
	unsigned long WAITFOR_SHOWTIME_COUNT = InitTimeout / PM_FM_POLLING_RATE;
	unsigned long WAITFOR_INIT_COUNT = ActTimeout / PM_FM_POLLING_RATE;
	unsigned long RETRY_WAIT_TIME = RETRY_WAIT_TIME_MIN_MSEC / PM_FM_POLLING_RATE;
	if (RetryTime > RETRY_WAIT_TIME_MIN_MSEC)	// STM Gian Set RetryTime Only if is Bigger then minimum value
		RETRY_WAIT_TIME = RetryTime / PM_FM_POLLING_RATE;
	unsigned long INIT_POLLING_TIME = 5; //(??????) 
	unsigned long WAITFOR_DISORDERLY_COUNT = 3; 

	NEAR_LCDNI_COUNT = LCD_Trig / PM_FM_POLLING_RATE;         
	NEAR_LCDI_COUNT = NEAR_LCDNI_COUNT;          
	NEAR_LOS_COUNT = LOS_LOF_Trig / PM_FM_POLLING_RATE;           
	NEAR_LOF_COUNT = NEAR_LOS_COUNT;           
	FAR_LCDNI_COUNT = NEAR_LCDNI_COUNT;          
	FAR_LCDI_COUNT = NEAR_LCDNI_COUNT;						
	FAR_LOS_COUNT = NEAR_LOS_COUNT;            
	FAR_LOF_COUNT = NEAR_LOS_COUNT;            
	FAR_LOS_SHORT_COUNT = NEAR_LOS_COUNT;      
	FAR_LOF_SHORT_COUNT = NEAR_LOS_COUNT;      

	UINT delay = 0;
		
	while (amu_go)
	{
		xtm_wkafter(500);
		delay += 500;
           
		
		// Exit immediately on surprise removal
		// ------------------------------------
		if (GlobalRemove) return;

		HandleAtmError();
		HandleLeds();
		
		// Modem Software polling
		// ----------------------
		if (delay < PM_FM_POLLING_RATE) continue;
		delay = 0;
           
		// Modem Software polling
		// ----------------------
		switch(g_ModemState)
		{
		case C_AMSW_IDLE       : 
			g_WaitForRetry = 0; // STM Gian clear
			g_WaitForInit = 0;  // STM Gian clear

			break;
		case C_AMSW_ACTIVATING : 
			g_WaitForInit++;
			if(g_WaitForInit >= WAITFOR_INIT_COUNT)
			{
				msw_report_event(AMU_EVENT_ACT_TIMEOUT,0);
				PRINT_ERROR("Timeout in activation!!!!\n");
				g_ModemState = C_AMSW_DISORDERLY;
				g_WaitForInit = 0;
			}
			break;
		case C_AMSW_L3         : break;
		case C_AMSW_SHOWTIME_L0:
		case C_AMSW_SHOWTIME_LQ:
		case C_AMSW_SHOWTIME_L1:
			// Poll line for defects and performance
			g_ShowtimeCounter++;
			if (g_ShowtimeCounter >= INIT_POLLING_TIME)
			{
//				PRINT_ERROR("Polling Defects after %d ms\n", g_ShowtimeCounter*PM_FM_POLLING_RATE);
				if ( (FM_Polling(PM_toggle) != C_AMSW_ACK ))
				{
					PRINT_ERROR("Error in AMSW_get_Data during Fm Polling....\n");
					PRINT_ERROR("Fm Polling will be stopped for %d sec!!!\n",(INIT_POLLING_TIME*PM_FM_POLLING_RATE)/1000);
					g_ShowtimeCounter = 0;
				}
				else
					PM_toggle = !PM_toggle;
			}
			break;
		case C_AMSW_INITIALIZING :
		case C_AMSW_Ghs_HANDSHAKING :
		case C_AMSW_ANSI_HANDSHAKING :

			// WAIT to reach SHOWTIME
			g_WaitForShowtime++;
			if(g_WaitForShowtime >= WAITFOR_SHOWTIME_COUNT)
			{
				msw_report_event(AMU_EVENT_INI_TIMEOUT,0);
				PRINT_ERROR("Timeout in initialization!!!!\n");
				// Reset count
				g_WaitForShowtime = 0;
				// Disorderly shutdown 
				g_ModemState = C_AMSW_DISORDERLY;
			}
			break;

		case C_AMSW_DISORDERLY :

			// Special state for disorderly shutdown
			// Wait out the period of heavy burst of interrupts
			// from CO side when line length is changed while in
			// showtime
		
			g_WaitForDisorderly++;
			if(g_WaitForDisorderly >= WAITFOR_DISORDERLY_COUNT)
			{


//				if (RetryTime != 0)
//						msw_report_event(AMU_EVENT_RETRY,0);

				board_disable_intrs();

//				xtm_wkafter(WAITFOR_DISORDERLY_COUNT);

				
				g_ShowtimeCounter = 0;							
				g_WaitForDisorderly = 0;

				// Disorderly shutdown 
				l_RetCode = AMSW_ANT_requestModemStateChange(C_AMSW_IDLE);
				if(l_RetCode != C_AMSW_ACK) {
					PRINT_ERROR("Error in AMSW_ANT_requestModemStateChange(C_AMSW_IDLE) = %d\n", l_RetCode);
				}
				else for (int i=0; i<6; i++)
				{
					if (g_ModemState == C_AMSW_IDLE || GlobalRemove) break;
					xtm_wkafter(500);
				}
				if (RetryTime != 0)
				{
					g_ModemState = C_AMSW_RETRY;
					g_WaitForRetry = 0;
				}
        
			}
			break;
			
		case C_AMSW_RETRY:
			if (RetryTime != 0)
			{
				g_WaitForRetry++;
				//xtm_wkafter(RETRY_WAIT_TIME);
#ifdef _PICAP_CODE_
				if(Vendor_Id_code_ECI==TRUE)
					xtm_wkafter(1000);  //modif PICAP ECI CK
#endif
				if (g_WaitForRetry >= RETRY_WAIT_TIME)
                {
					msw_report_event(AMU_EVENT_RETRY,0);
					g_WaitForRetry = 0;
					msw_start();
				}
			}
			else
			{
				PRINT_ERROR("Error in AMUTask (C_AMSW_RETRY) with RetryTime != 0 (%ld)\n",RetryTime);
				g_ModemState = C_AMSW_IDLE;
			}
			break;
		default:
			PRINT_ERROR("Error in AMUTask g_ModemState =%d not catched\n",g_ModemState);
			g_ModemState = C_AMSW_IDLE;
	        break;
		}						//end switch
	}
}

// This function  monitors the line for LOS (Loss Of Signal), LOF (Loss Of Frame),
// LCDI (Loss Of Cell Delineation Interleaved), 
// LCDNI (Loss Of Cell Delineation Fast)defects

unsigned long FM_Polling(bool pm_poll)
{
	unsigned long l_RetCode = C_AMSW_REJ;

	if ((g_ModemState == C_AMSW_SHOWTIME_L0) ||
			(g_ModemState == C_AMSW_SHOWTIME_LQ) ||
			(g_ModemState == C_AMSW_SHOWTIME_L1))
	{
		// Get defect bit map
		l_RetCode = AMSW_ANT_getData(C_AMSW_FM_DATA, &g_def_bitmap_set);
		
		if(l_RetCode == C_AMSW_ACK)
		{
//#######################  Loss Of Signal  ##############################   
	
			if( ((g_def_bitmap_set.near_end.status & LOS) == LOS) ||
				((g_def_bitmap_set.near_end.status & LOS) == 0) && 
				((g_def_bitmap_set.near_end.change & LOS) == LOS) )
			{
				g_NEAR_LOS++;
			}
			else
			{
				g_NEAR_LOS = 0;
			}
			if( ((g_def_bitmap_set.far_end.status & LOS) == LOS) ||
				((g_def_bitmap_set.far_end.status & LOS) == 0) && 
				((g_def_bitmap_set.far_end.change & LOS) == LOS) )
			{
				g_FAR_LOS++;
			}
			else
			{
				g_FAR_LOS = 0;
			}

//######################  Loss Of Cell Delineation Fast  ######################	

			if((g_def_bitmap_set.near_end.status & LCDNI) == LCDNI)
			{
				g_NEAR_LCDNI++;
			}
			else
			{
				g_NEAR_LCDNI = 0;
			}
			if((g_def_bitmap_set.far_end.status & LCDNI) == LCDNI)
			{
				g_FAR_LCDNI++;
			}
			else
			{
				g_FAR_LCDNI = 0;
			}

//######################  Loss Of Cell Delineation Interleaved ######################

			if((g_def_bitmap_set.near_end.status & LCDI) == LCDI)
			{
				g_NEAR_LCDI++;
			}
			else
			{
				g_NEAR_LCDI = 0;
			}
			if((g_def_bitmap_set.far_end.status & LCDI) == LCDI)
			{
				g_FAR_LCDI++;
			}
			else
			{
				g_FAR_LCDI = 0;
			}

		// WAIT : If over a period of time defect persists, do a disorderly shutdown

			if( (g_NEAR_LOS >= NEAR_LOS_COUNT) || (g_NEAR_LCDNI >= NEAR_LCDNI_COUNT) ||
				(g_NEAR_LCDI >= NEAR_LCDI_COUNT) )
			{
				g_FAR_LOS = 0;
				g_FAR_LCDNI = 0;
				g_FAR_LCDI = 0;
				g_ModemState = C_AMSW_DISORDERLY;
			}
			else if ( (g_FAR_LOS >= FAR_LOS_COUNT) || (g_FAR_LCDNI >= FAR_LCDNI_COUNT) ||
					(g_FAR_LCDI >= FAR_LCDI_COUNT) ||
					((g_FAR_LOS >= FAR_LOS_SHORT_COUNT) && g_NEAR_LOS ) )
				g_ModemState = C_AMSW_DISORDERLY;
								
			if(g_ModemState == C_AMSW_DISORDERLY)
			{

				PRINT_ERROR("Bringing down line due to persistent:\n");
				PRINT_ERROR("NEAR_LOS = %d/NEAR_LCD = %d/NEAR_LCDI = %d\nFAR_LOS = %d  FAR_LCDI = %d  FAR_LCDNI = %d\n",
					g_NEAR_LOS,  g_NEAR_LCDNI, g_NEAR_LCDI,g_FAR_LOS, g_FAR_LCDNI, g_FAR_LCDI);
				
				msw_report_event(AMU_EVENT_SHUTDOWN,0);
	/*			
				if (RetryTime != 0)
				{
					msw_report_event(AMU_EVENT_RETRY,0);
					PRINT_ERROR("Auto-Retry will take place\n");
				}
	*/			

				g_NEAR_LOS = 0; g_NEAR_LCDNI = 0; g_NEAR_LCDI = 0;
				g_FAR_LOS = 0; g_FAR_LCDNI = 0; g_FAR_LCDI = 0;
			}
		/*		
			if (pm_poll)
			{
				PM_Polling();
			}
		*/
		}
	}

	return l_RetCode;
}

unsigned long PM_Polling(void)
{
   
	  unsigned long l_RetCode = C_AMSW_REJ;

		if   ((g_ModemState == C_AMSW_SHOWTIME_L0) ||
          (g_ModemState == C_AMSW_SHOWTIME_LQ) ||
					(g_ModemState == C_AMSW_SHOWTIME_L1)
         )
    {

				l_RetCode = AMSW_ANT_getData(C_AMSW_PM_DATA, &g_def_counter_set);
				if(l_RetCode != C_AMSW_ACK)
				{
						PRINT_ERROR("AMSW_ANT_getData error\n");
				}
				else
				{
						PRINT_INFO("\nFast Path Performance Counters:\n\n");	

						PRINT_INFO("Near-end Fec-F = %5u\n", g_def_counter_set.near_end.FecNotInterleaved);
						PRINT_INFO("Far-end Fec-F  = %5u\n", g_def_counter_set.far_end.FecNotInterleaved);
						
						PRINT_INFO("Near-end Crc-F = %5u\n", g_def_counter_set.near_end.CrcNotInterleaved);
						PRINT_INFO("Far-end Crc-F  = %5u\n", g_def_counter_set.far_end.CrcNotInterleaved);

						PRINT_INFO("Near-end Hec-F = %5u\n", g_def_counter_set.near_end.HecNotInterleaved);
						PRINT_INFO("Far-end Hec-F  = %5u\n", g_def_counter_set.far_end.HecNotInterleaved);
						
						PRINT_INFO("Near-end Total Cell-F  = %5u\n", g_def_counter_set.near_end.TotalCellCountNotInterleaved);								
						PRINT_INFO("Near-end Active Cell-F = %5u\n", g_def_counter_set.near_end.ActiveCellCountNotInterleaved);
						
						PRINT_INFO("\nInterleave Path Performance Counters:\n\n");	

						PRINT_INFO("Near-end Fec-I = %5u\n", g_def_counter_set.near_end.FecInterleaved);
						PRINT_INFO("Far-end Fec-I  = %5u\n", g_def_counter_set.far_end.FecInterleaved);
												
						PRINT_INFO("Near-end Crc-I = %5u\n", g_def_counter_set.near_end.CrcInterleaved);
						PRINT_INFO("Far-end Crc-I  = %5u\n", g_def_counter_set.far_end.CrcInterleaved);
						
						PRINT_INFO("Near-end Hec-I = %5u\n", g_def_counter_set.near_end.HecInterleaved);
						PRINT_INFO("Far-end Hec-I  = %5u\n", g_def_counter_set.far_end.HecInterleaved);
						
						PRINT_INFO("Near-end Total Cell-I  = %5u\n", g_def_counter_set.near_end.TotalCellCountInterleaved);
						PRINT_INFO("Near-end Active Cell-I = %5u\n", g_def_counter_set.near_end.ActiveCellCountInterleaved);
				}
		}

		return l_RetCode;
}
