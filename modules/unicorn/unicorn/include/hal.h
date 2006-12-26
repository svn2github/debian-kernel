//----------------------------------------------------------------------
// Test driver for the STMicroelectronics ADSL Chip Taurus
//----------------------------------------------------------------------
// File: hal.h
// Author: Christophe Piel
// Copyright F.H.L.P. 2000
// Copyright ST Microelectronics 2000
//----------------------------------------------------------------------
// provides definitions for hardware access API to the Taurus chip
//----------------------------------------------------------------------
#ifndef __hal__h_
#define __hal__h_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SUCCESS,FAILURE } ST_STATUS;
//----------------------------------------------------------------------
//	PCI hardware access interface
//----------------------------------------------------------------------
//	structure definitions
//----------------------------------------------------------------------
typedef struct {
	DWORD *CMDptrW1;
	DWORD *CMDptrW2;
	DWORD *CMDptrRd;
	WORD *ITABLEptr;
	WORD *IMASKptr;
} T_PCIinit;

typedef struct {
	WORD addr;
	DWORD data;
} T_DirData;

typedef struct {
	DWORD idata;
	DWORD iaddr;
	DWORD status;
} T_SlaveDirData;

typedef struct {
	DWORD idata[3];
	DWORD icntrl;
	DWORD iaddr;
	DWORD status;
} T_SlaveIndData;

typedef struct {
	DWORD idata;
	DWORD iaddr;
	DWORD mask;
	DWORD status;
} T_SlaveMaskData;

typedef struct {
	DWORD *cmdBuff;
	DWORD *resBuff;
	DWORD wrSize;
	DWORD rdSize;
	DWORD status;
} T_MasterRBlock;

typedef struct {
	DWORD *cmdBuff;
	DWORD wrSize;
	DWORD status;
} T_MasterWBlock;

typedef struct {
	DWORD *resBuff;
	DWORD baseAddr;
	DWORD wrSize;
	DWORD rdSize;
	DWORD status;
} T_MasterRBurst;

typedef struct {
	DWORD *cmdBuff;
	DWORD baseAddr;
	DWORD wrSize;
	DWORD status;
} T_MasterWBurst;

typedef struct {
	DWORD *cmdBuff;
	DWORD SocAddr;
	DWORD SocCntrl;
	DWORD mask;
	DWORD reqSize;
	DWORD exeSize;
	DWORD status;
} T_Msg;

typedef struct {
	PBYTE buffer;
	DWORD length;
} T_WriteAtmData;
//----------------------------------------------------------------------
//	Function prototypes
//----------------------------------------------------------------------
ST_STATUS
PCI_init(
	DWORD **CMDptrW1,
	DWORD **CMDptrW2,
	DWORD **CMDptrRd,
	WORD **ITABLEptr,
	WORD **IMASKptr
	);

ST_STATUS
PCI_directRead(
	WORD addr,
	DWORD *data
	);

ST_STATUS
PCI_directWrite(
	WORD addr,
	DWORD data
	);

ST_STATUS
PCI_SlaveRMWrite(
	T_SlaveMaskData *dataPtr
	);

ST_STATUS
PCI_SlaveReadDirect(
	T_SlaveDirData *dataPtr
	);

ST_STATUS
PCI_SlaveWriteDirect(
	T_SlaveDirData *dataPtr
	);

ST_STATUS
PCI_SlaveReadIndirect(
	T_SlaveIndData *dataPtr
	);

ST_STATUS
PCI_SlaveWriteIndirect(
	T_SlaveIndData *dataPtr
	);

ST_STATUS
PCI_MasterReadDirBlock(
	T_MasterRBlock *dataPtr
	);

ST_STATUS
PCI_MasterWriteDirBlock(
	T_MasterWBlock *dataPtr
	);

ST_STATUS
PCI_MasterReadIndBlock(
	T_MasterRBlock *dataPtr
	);

ST_STATUS
PCI_MasterWriteIndBlock(
	T_MasterWBlock *dataPtr
	);

ST_STATUS
PCI_MasterReadDirBurst(
	T_MasterRBurst *dataPtr
	);

ST_STATUS
PCI_MasterWriteDirBurst(
	T_MasterWBurst *dataPtr
	);

ST_STATUS
PCI_MasterReadIndBurst(
	T_MasterRBurst *dataPtr
	);

ST_STATUS
PCI_MasterWriteIndBurst(
	T_MasterWBurst *dataPtr
	);

ST_STATUS
PCI_MessageRead(
	T_Msg *dataPtr
	);

ST_STATUS
PCI_MessageWrite(
	T_Msg *dataPtr
	);

ST_STATUS
PCI_WaitForObc(
	void
	);

ST_STATUS
PCI_WriteAtm(
	PBYTE buffer,
	DWORD length
	);

ST_STATUS
PCI_ReadAtm(
	PBYTE buffer,
	DWORD size,
	DWORD *length
	);

ST_STATUS
PCI_ReadConfigHeader (
	PVOID Buffer,
	DWORD Offset,
	DWORD Count
	);

ST_STATUS
PCI_WriteConfigHeader (
	PVOID Buffer,
	DWORD Offset,
	DWORD Count
	);

ST_STATUS
PCI_ReadDeviceSpecificConfig (
	PVOID Buffer,
	DWORD Offset,
	DWORD Count
	);

ST_STATUS
PCI_WriteDeviceSpecificConfig (
	PVOID Buffer,
	DWORD Offset,
	DWORD Count
	);
//----------------------------------------------------------------------
//	USB hardware access interface
//----------------------------------------------------------------------
//	structure definitions
//----------------------------------------------------------------------
//enum T_Bool {FALSE=0,TRUE=1};
typedef enum {EP2,EP6} T_EpOut ;
typedef enum {EP3,EP5,EP7} T_EpIn ;

typedef struct {
	BYTE addr;
	short data;
} T_RegData;

typedef struct {
	WORD *cmdBuff;
	WORD wrSize;
	WORD rdSize;
} T_ReadData;

typedef struct {
	WORD *cmdBuff;
	WORD frameSize;
} T_ShortWrite;

typedef struct {
	WORD *cmdBuff;
	WORD nFrames;
	WORD frameSize;
	WORD lastFrameSize;
} T_LongWrite;

typedef struct {
	WORD ep0_size;
	WORD ep1_size;
	WORD ep2_size;
	WORD ep3_size;
	WORD ep4_size;
	WORD ep5_size;
	WORD ep6_size;
	WORD ep7_size;
} T_EpSettings;

typedef struct {
	WORD *CMDptrW1;
	WORD *CMDptrW2;
	WORD *CMDptrRd;
	WORD *CMDptrW_I1;
	WORD *CMDptrW_I2;
	WORD *CMDptrRd_I;
	WORD *ITABLEptr;
	WORD *IMASKptr;
	T_EpSettings ep_setting;
} T_USBinit;

typedef struct {
	WORD *cmdBuff;
	WORD wrSize;
	WORD rdSize;
	T_EpOut ep_out;
	T_EpIn ep_in;
} T_EpReadData;

typedef struct {
	T_ShortWrite wr;
	T_EpOut ep_out;
} T_EpShortWrite;

typedef struct {
	T_LongWrite wr;
	T_EpOut ep_out;
} T_EpLongWrite;

//----------------------------------------------------------------------
//	Function prototypes
//----------------------------------------------------------------------
ST_STATUS
USB_init(
	WORD **CMDptrW1,
	WORD **CMDptrW2,
	WORD **CMDptrRd,
	WORD **CMDptrW_I1,
	WORD **CMDptrW_I2,
	WORD **CMDptrRd_I,
	WORD **ITABLEptr,
	WORD **IMASKptr,
	T_EpSettings *ep_setting
	);

ST_STATUS
USB_controlRead(
	BYTE addr,
	WORD *data
	);

ST_STATUS
USB_controlWrite(
	BYTE addr,
	WORD data
	);

ST_STATUS
USB_S_Write(
	T_ShortWrite *dataPtr,
	T_EpOut ep_out
	);

ST_STATUS
USB_L_Write(
	T_LongWrite *dataPtr,
	T_EpOut ep_out
	);

ST_STATUS
USB_Read(
	T_ReadData *dataPtr,
	T_EpOut ep_out,
	T_EpIn ep_in
	);

ST_STATUS
USB_WriteAtm(
	PBYTE buffer,
	DWORD length
	);

ST_STATUS
USB_ReadAtm(
	PBYTE buffer,
	DWORD size,
	DWORD *length
	);

BOOL USB_checkIntContext(void);

VOID WritePort(WORD ad,BYTE b);
//----------------------------------------------------------------------
//	EOF
//----------------------------------------------------------------------
#ifdef __cplusplus
} // extern "C"
#endif
	

#endif
