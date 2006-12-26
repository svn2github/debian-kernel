//----------------------------------------------------------------------
// Driver for the STMicroelectronics ADSL Chip Taurus
//----------------------------------------------------------------------
// File: rapi.h
// Author: Christophe Piel
// Copyright F.H.L.P. 2000
// Copyright ST Microelectronics 2000
//----------------------------------------------------------------------
// provides definitions for Operating System API to the Taurus chip
//----------------------------------------------------------------------
#ifndef __rapi__h_
#define __rapi__h_

#ifdef __cplusplus
extern "C" {
#endif

	enum {
		_SM_WAIT=0,
		_SM_NOWAIT=1
	};

void setShowtime(void);			// Set showtime FLAG to TRUE
void resetShowtime(void);		// Set showtime FLAG to FALSE
void setAtmRate(				// reports ATM cell rates
	unsigned short upRate,
	unsigned short downRate
	);

//----------------------------------------------------------------------
//	INTERRUPT MANAGEMENT
//----------------------------------------------------------------------
typedef void (*ITHANDLER)(DWORD);

#define	ERR_BSP_ILL_INTR	8

DWORD
board_set_intr_handler(
	DWORD intr,
	ITHANDLER p_handler,
	DWORD devNumber
	);

DWORD
board_reset_intr_handler(
	DWORD intr
	);

DWORD
board_clear_intr_pending(
	DWORD intr
	);

DWORD
board_mask_intr(
	DWORD intr
	);

DWORD
board_unmask_intr(
	DWORD intr
	);

void
board_disable_intrs(
	void
	);

void
board_enable_intrs(
	void
	);

//----------------------------------------------------------------------
//	TASK MANAGEMENT
//----------------------------------------------------------------------
#define	XPRIO_EVENT_HDLR	200
#define	XPRIO_CRITICAL_APPL	150
#define	XPRIO_OAM_APLL		100
#define	XPRIO_BACKGRND_APPL	50

typedef void (*START_FUNC)(DWORD,DWORD,DWORD,DWORD);

//	Creates a task
//	--------------
DWORD xt_create(
	char	name[4],
	DWORD	prio,
	DWORD	sstack,
	DWORD	ustack,
	DWORD	flags,
	DWORD	*tid
	);

//	Starts a task
//	-------------
DWORD xt_start(
	DWORD		tid,
	DWORD		mode,
	START_FUNC	start_addr,
	DWORD		targs[4]
	);

//	Disables task scheduling
//	------------------------
void xt_entercritical(void);

//	Enables task scheduling
//	-----------------------
void xt_exitcritical(void);

//	Waits until all tasks terminate
//	-------------------------------
void xt_waitexit(void);
//----------------------------------------------------------------------
//	MEMORY MANAGEMENT
//----------------------------------------------------------------------

//	Allocates a memory buffer
//	-------------------------
DWORD xm_getmem(
	DWORD	size,
	PVOID	*bufaddr
	);

//	Release a memory buffer
//	-----------------------
DWORD xm_retmem(
	PVOID bufaddr
	);
//----------------------------------------------------------------------
//	INTER TASK COMMUNICATION AND SYNCHRONIZATION (SEMAPHORE)
//----------------------------------------------------------------------

//	Creates a semaphore
//	-------------------
DWORD xsm_create(
	char	name[4],
	DWORD	count,
	DWORD	flags,
	DWORD	*smid
	);

//	Gets a semaphore ident
//	----------------------
DWORD xsm_ident(
	char	name[4],
	DWORD	node,
	DWORD	*smid
	);

//	Allows a task to acquire a semaphore
//	------------------------------------
DWORD xsm_p(
	DWORD	smid,
	DWORD	flags,
	DWORD	timeout
	);

//	Releases a semaphore
//	--------------------
DWORD xsm_v(
	DWORD	smid
	);
//----------------------------------------------------------------------
//	INTER TASK COMMUNICATION AND SYNCHRONIZATION (QUEUE)
//----------------------------------------------------------------------

//	Creates a message queue
//	-----------------------
DWORD xq_create(
	char	name[4],
	DWORD	count,
	DWORD	flags,
	DWORD	*qid
	);

//	Receives a message from a queue
//	-------------------------------
DWORD xq_receive(
	DWORD	qid,
	DWORD	flags,
	DWORD	timeout,
	DWORD	msg_buf[4]
	);

//	Sends a message to a queue
//	--------------------------
DWORD xq_send(
	DWORD	qid,
	DWORD	msg_buf[4]
	);
//----------------------------------------------------------------------
//	TIMER MANAGEMENT
//----------------------------------------------------------------------
typedef enum {
	E_XTM_ONE_SHOT = 1,
	E_XTM_PERIODIC = 2,
}
T_timer_mode;

#define	XMID_TIMER_EX	1

//	Starts a timer
//	--------------
DWORD xtm_startmsgtimer(
	DWORD	qid,
	T_timer_mode mode,
	DWORD	interval,
	DWORD	userdata,
	BYTE	owner_ctrl,
	DWORD	*tmid
	);

//	Stops a timer
//	-------------
DWORD xtm_stopmsgtimer(
	DWORD	tmid
	);
//----------------------------------------------------------------------
//	TIME FUNCTIONS
//----------------------------------------------------------------------

//	Wake up after n milliseconds
//	----------------------------
DWORD xtm_wkafter(
	DWORD ms
	);

//	Returns system time in ms.
//	--------------------------
DWORD xtm_gettime(
	void
	);

//	Returns in ms the difference between two times
//	----------------------------------------------
DWORD xtm_timediff(
	DWORD time1,
	DWORD time2
	);

//	Returns the elapsed time since the given time
//	---------------------------------------------
DWORD xtm_elapse(
	DWORD time
	);

//	Gets a time stamp in ms and s.
//	-----------------------------
DWORD xtm_gettimestamp(
	DWORD *microseconds,
	DWORD *seconds
	);
//----------------------------------------------------------------------
//	MISCELLANEOUS
//----------------------------------------------------------------------
//#define	dbg_print	DbgPrint
//DWORD __cdecl DbgPrint(char *fmt,...);

extern DWORD tosca_hardITABLE[14];
extern WORD tosca_softITABLE[28]; // TOSCA Interrupt table visible to MSW
extern int rapi_init(void);
extern void rapi_exit(void);
#define rapi_lock() 
#define rapi_unlock()
  //#define rapi_lock() do_rapi_lock(__FUNCTION__)
  //#define rapi_unlock() do_rapi_unlock(__FUNCTION__)
extern void do_rapi_lock(const char *func);
extern void do_rapi_unlock(const char *func);
extern void tosca_interrupt(void);

//----------------------------------------------------------------------
//	END
//----------------------------------------------------------------------
#ifdef __cplusplus
}	//extern "C"
#endif

#endif	// __rapi__h_
