#ifndef _AMU_H_
#define _AMU_H_

//
// amu.h : Defines the global variables for manager.
//


#include "types.h"
#include "amas.h"				/* AMAS API's and typedefs */


/***********************************************************************
 *            AMU Global Variables                                     *
 ***********************************************************************/

extern unsigned long FmPollingRate;
extern unsigned long InitTimeout;
extern unsigned long ActTimeout;
extern unsigned long RetryTime;

extern unsigned long LCD_Trig;
extern unsigned long LOS_LOF_Trig;

unsigned long g_AMUQid = 0;
unsigned long g_ModemState = C_AMSW_IDLE;
unsigned int  g_WaitForInit =  0, g_WaitForShowtime = 0, g_WaitForDisorderly = 0; 
unsigned int  g_WaitForRetry = 0;  
unsigned char g_NEAR_LOS = 0, g_NEAR_LOF = 0, g_NEAR_LCDNI = 0, g_NEAR_LCDI = 0;
unsigned char g_FAR_LOS = 0, g_FAR_LOF = 0, g_FAR_LCDNI = 0, g_FAR_LCDI = 0;
unsigned char g_Mode = AMSW_ANSI | AMSW_UAWG | AMSW_G_DMT | AMSW_G_LITE;
unsigned long g_ShowtimeCounter = 0;				  
bool L3_flag = FALSE;	// bool var to acknowledge answer to orderly shutdown request

T_AMSW_Identification               g_Identification;
T_AMSW_NT_NearEndLineOperData       g_NearEndLineOperData;
T_AMSW_NT_FarEndLineOperData        g_FarEndLineOperData;
T_AMSW_def_counter_set              g_def_counter_set;
T_AMSW_def_bitmap_set               g_def_bitmap_set;
T_AMSW_def_counters                 g_def_counters;
T_AMSW_NT_ChannelOperData           g_ChannelOperData;
T_AMSW_ANT_CustomerConfiguration    g_CustomerCfg;
T_AMSW_ANT_StaticConfiguration      g_StaticCfg;
T_AMSW_PowerStateConfiguration      g_PowerStateCfg;
T_AMSW_Teq g_Teq; // STM G
T_AMSW_Ber g_Ber;
T_AMSW_VersionMS g_VersionMS;

void AMUTask(unsigned long Arg1, unsigned long , unsigned long , unsigned long );
unsigned long FM_Polling(bool pm_poll);
unsigned long PM_Polling(void);

#endif   // _AMU_H_
