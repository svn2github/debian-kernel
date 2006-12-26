//----------------------------------------------------------------------
// Test driver for the STMicroelectronics ADSL Chip Taurus
//----------------------------------------------------------------------
// File: hard.h
// Author: Christophe Piel
// Copyright F.H.L.P. 2000
// Copyright ST Microelectronics 2000
//----------------------------------------------------------------------
// Contains definitions of the hardware registers and configuration
//----------------------------------------------------------------------
#ifndef	__HARD__H_
#define __HARD__H_

#define	OBC_CMD_TIMEOUT	1000	// ms
#define	TOSCA_INTR_WDOG	200	// ms

#define	ATM_CELL_LENGTH	53


//	Interrupt Status Register bit description
//	-----------------------------------------
#define	ISR_TOIF		0x0001	// TOSCA Direct Interrupt Flag
#define	ISR_TOIE		0x0002	// TOSCA Direct Interrupt Enable
#define	ISR_TOIFS		0x0004	// TOSCA Delayed Interrupt
#define	ISR_TOIES		0x0008	// TOSCA Delayed Interrupt Enable
#define	ISR_TIMIF		0x0010	// Timer Interrupt Flag
#define	ISR_TIMIE		0x0020	// Timer Interrupt Enable
#define	ISR_OSIF		0x0040	// OBC Slave Interrupt Flag
#define	ISR_OSIE		0x0080	// OBC Slave Interrupt Enable
#define	ISR_OMIF		0x0100	// OBC Master Interrupt Flag
#define	ISR_OMIE		0x0200	// OBC Master Interrupt Enable
#define	ISR_GPIFA		0x0400	// GPIN[0] Interrupt Flag
#define	ISR_GPIEA		0x0800	// GPIN[0] Interrupt Enable
#define	ISR_GPIFB		0x1000	// GPIN[4] Interrupt Flag
#define	ISR_GPIEB		0x2000	// GPIN[4] Interrupt Enable

#define	ISR_IF			0x1555	// Mask for all interrupts flags


#ifdef _PCI_DRIVER
//----------------------------------------------------------------------
//	PCI interface hardware definitions
//----------------------------------------------------------------------
#define	PCI_CELL_LENGTH	64

#define	PCI_US_BUF_SIZE	(30*(PCI_CELL_LENGTH/sizeof(DWORD)))
#define	PCI_DS_BUF_SIZE	(200*(PCI_CELL_LENGTH/sizeof(DWORD)))

typedef struct {
	DWORD CmdBufW1[512];
	DWORD CmdBufW2[512];
	DWORD CmdBufRd[1312];
	DWORD AtmUsBuf[2*PCI_US_BUF_SIZE];
	DWORD AtmDsBuf[4*PCI_DS_BUF_SIZE];
} DMA_MEMORY, *PDMA_MEMORY;

//	PCI_BRIDGE Register Map
//	=======================
#define	PR_LACFW		0x0000	// Local Arbitrer Configuration Word
#define	PR_LACFW_SET	0x0004	// SET Local Arbitrer Configuration Word bits
#define	PR_LACFW_CLR	0x0008	// CLR Local Arbitrer Configuration Word bits
#define	PR_MWC			0x000C	// Memory Write Control
#define	PR_MRC			0x0010	// Memory Read Control
#define	PR_PCFW			0x0014	// PCI Configuration Word Data Register
#define	PR_ISDR			0x0018	// PCI Interrupt Status & Data Register
#define	PR_RUNNING		0x001C	// Reports status of PCI bus request

//	Down-Stream DMA

#define	PR_DS_PTR_1		0x0800	// Host Memory DS Start Pointer Buffer 1
#define	PR_DS_PTR_2		0x0804	// Host Memory DS Start Pointer Buffer 2
#define	PR_DS_SIZE_1	0x0808	// Set the Number of DMA data transfer 1
#define	PR_DS_SIZE_2	0x080C	// Set the Number of DMA data transfer 1
#define	PR_DS_MOVED		0x0810	// Current number of double words transfered

//	Up-Stream DMA

#define	PR_US_PTR_1		0x1000	// Host Memory US Start Pointer Buffer 1
#define	PR_US_PTR_2		0x1004	// Host Memory US Start Pointer Buffer 2
#define	PR_US_SIZE_1	0x1008	// Set the Number of DMA data transfer 1
#define	PR_US_SIZE_2	0x100C	// Set the Number of DMA data transfer 1
#define	PR_US_MOVED		0x1010	// Current number of double words transfered

//	 OBC Write DMA

#define	PR_WR_PTR		0x1800	// Host Memory WR Start Pointer Buffer
#define	PR_WR_SIZE		0x1808	// Set the Status of DMA channel
#define	PR_WR_MOVED		0x1810	// Current number of double words transferred

//	OBC Read DMA

#define	PR_RD_PTR		0x2000	// Host Memory RD Start Pointer Buffer
#define	PR_RD_SIZE		0x2008	// Set the Status of DMA channel
#define	PR_RD_MOVED		0x2010	// Current number of double words transferred

//	Handshaking

#define	PR_DS_PTR_HS	0x2800	// Host Memory DS Handshaking 1 Location
#define	PR_US_PTR_HS	0x2804	// Host Memory US Handshaking 1 Location
#define	PR_OBC_PTR_HS	0x2808	// Host Memory OBC Handshaking Location
#define	PR_PATTERN		0x280C	// Pattern used by handshake pointers

//	CFG_MEM Register Map
//	--------------------
#define	PR_C_ADR		0x4000	// SPI ADR & CONTROL Register
#define	PR_C_DATA		0x4004	// SPI DATA Register

//	ADSL uP Register Map: REGS
//	--------------------------
#define	PR_STATUS		0x8000	// Status Control Register
#define	PR_IDATA_1		0x8004	// Indirect Data 1 Register (LSB)
#define	PR_IDATA_2		0x8008	// Indirect Data 2 Register
#define	PR_IDATA_3		0x800C	// Indirect Data 3 Register (MSB)
#define	PR_IDATA		0x8010	// Indirect Data 3 Register
#define	PR_IADR			0x8014	// Indirect Address Register
#define	PR_IMASKS		0x8018	// Mask to apply to Slave Indirect R/W
#define	PR_IRMW			0x801C	// Mask to apply to Slave R/M/W access
#define	PR_IADR_TX		0x8020	// Indirect TX Reference Address Register
#define	PR_IADR_RX		0x8024	// Indirect RX Reference Address Register
#define	PR_IADR_MSG		0x8028	// Indirect MSG Reference Address Register
#define	PR_IADR_CHK		0x802C	// Indirect Address Check Register
#define	PR_IADR_BURST	0x8030	// Burst Base Address Register
#define	PR_SIZE_BURST	0x8034	// Number of Burst accesses to be executed
#define	PR_IMASKM		0x8038	// Mask to apply to the Master Ind or Msg R/W
#define	PR_IADR_IRQ		0x803C	// Indirect Address of Tosca IRQ Table
#define	PR_ITABLE		0x8040	// Interrupt Table Data Register
#define	PR_ISR			0x807C	// Interrupt Status Register

//	ADSL uP Register Map: PERIPHERALs
//	---------------------------------
#define	PR_GPIO_DATA	0x8080	// Status of the GPIO pins
#define	PR_GPIO_DIR		0x8084	// Direction of the GPIO pins
#define	PR_GPIO_PER		0x8088	// Persistency values of the GPIO pins
#define	PR_TIM_REG_A	0x808C	// Timer A mode and preset value
#define	PR_TIM_REG_B	0x8090	// Timer B mode and preset value
#define	PR_AFE_TEST		0x8094	// AFE Feedback Control Register



//	PCI Interrupt Status & Data Register bit description
//	----------------------------------------------------
#define	PISDR_DMA_DS1_IF	0x00000001	// DMA DS PTR1 Interrupt Flag
#define	PISDR_DMA_DS2_IF	0x00000002	// DMA DS PTR2 Interrupt Flag
#define	PISDR_DMA_DS_IF		0x00000003	// DMA DS PTRS Interrupt Flag
#define	PISDR_DMA_US1_IF	0x00000004	// DMA US PTR1 Interrupt Flag
#define	PISDR_DMA_US2_IF	0x00000008	// DMA US PTR2 Interrupt Flag
#define	PISDR_DMA_US_IF		0x0000000C	// DMA US PTRS Interrupt Flag
#define	PISDR_DMA_OBC_IF	0x00000010	// DMA OBC Interrupt Flag
#define	PISDR_AIF1			0x00000020	// ADSL Interrupt 1 Flag
#define	PISDR_AIF2			0x00000040	// ADSL Interrupt 2 Flag
#define	PISDR_PEIF			0x00000080	// Parity Error Interrupt Flag
#define	PISDR_BEIF			0x00000100	// Bus Error Interrupt Flag
#define	PISDR_ACTDIF		0x00000200	// ACTD interrupt flag
#define	PISDR_DMA_DS1_IE	0x00000400	// DMA DS PTR1 Interrupt Enable
#define	PISDR_DMA_DS2_IE	0x00000800	// DMA DS PTR2 Interrupt Enable
#define	PISDR_DMA_DS_IE		0x00000C00	// DMA DS PTRS Interrupt Enable
#define	PISDR_DMA_US1_IE	0x00001000	// DMA US PTR1 Interrupt Enable
#define	PISDR_DMA_US2_IE	0x00002000	// DMA US PTR2 Interrupt Enable
#define	PISDR_DMA_US_IE		0x00003000	// DMA US PTRS Interrupt Enable
#define	PISDR_DMA_OBC_IE	0x00004000	// DMA OBC Interrupt Enable
#define	PISDR_AIE1			0x00008000	// ADSL Interrupt 1 Enable
#define	PISDR_AIE2			0x00010000	// ADSL Interrupt 2 Enable
#define	PISDR_PEIE			0x00020000	// Parity Error Interrupt Enable
#define	PISDR_BEIE			0x00040000	// Bus Error Interrupt Enable
#define	PISDR_ACTDIE		0x00080000	// ACTD Interrupt Flag Enable

#define	PISDR_IF			0x000003FF	// Mask for all interrupts flags

//	Local Arbitrer Configuration Word bit description
//	-------------------------------------------------
#define	LACFW_W_DS_SIZE		0x0001	// DS ATM transfer length
#define	LACFW_W_US_SIZE		0x0002	// US ATM transfer length
#define	LACFW_W_WR_SIZE		0x000C	// WR OBC transfer length
#define	LACFW_W_RD_SIZE		0x0030	// RD OBC transfer length
#define	LACFW_EDMA_DS1		0x0040	// Enable DS DMA pointer 1
#define	LACFW_EDMA_DS2		0x0080	// Enable DS DMA pointer 2
#define	LACFW_EDMA_DS		0x00C0	// Enable DS DMA pointers
#define	LACFW_EDMA_US1		0x0100	// Enable US DMA pointer 1
#define	LACFW_EDMA_US2		0x0200	// Enable US DMA pointer 2
#define	LACFW_EDMA_US		0x0300	// Enable US DMA pointers
#define	LACFW_EDMA_WROBC	0x0400	// Enable WR OBC DMA channel
#define	LACFW_EDMA_RDOBC	0x0800	// Enable RD OBC DMA channel
#define	LACFW_EHS_DS		0x1000	// Enable DS DMA Handshaking
#define	LACFW_EHS_US		0x2000	// Enable US DMS Handshaking
#define	LACFW_EHS_OBC		0x4000	// Enable OBC DMA Handshaking

//	Offset of Power Management Control/Status -PMCSR int PCI config space

#define	PCI_PMCSR	224

#endif


#ifdef _USB_DRIVER
//----------------------------------------------------------------------
//	USB interface hardware definitions
//----------------------------------------------------------------------
#define	USB_CELL_LENGTH	56

#define	ATM_DS_CELLS_PER_PKT	8	// ATM downstream cells per ISO packet (max alt 1)
#define ATM_READS               8       // max ATM downstream URB outstanding
#define	ATM_DS_ISO_PACKETS	5	// ATM downstream ISO packets per URB
#define	USB_DS_BUF_SIZE	(ATM_DS_CELLS_PER_PKT*ATM_READS*ATM_DS_ISO_PACKETS*(USB_CELL_LENGTH/sizeof(WORD)))
// with bandwidth reduced to 1 cell per ISO packet we need more ISO packet
#define	MAX_ISO_PACKETS	(ATM_DS_CELLS_PER_PKT*ATM_DS_ISO_PACKETS)

#define	ATM_US_CELLS_PER_PKT	3	// ATM upstream cells per ISO packet (max alt 1)
#define	ATM_WRITES	        2	// max ATM upstream URB outstanding
#define	ATM_US_ISO_PACKETS	5	// ATM upstream ISO packets per URB
#define	USB_US_BUF_SIZE	(ATM_US_CELLS_PER_PKT*ATM_WRITES*ATM_US_ISO_PACKETS*(USB_CELL_LENGTH/sizeof(WORD)))

typedef struct {
	WORD CmdBufW1[1024];
	WORD CmdBufW2[1024];
	WORD CmdBufRd[1024];

	WORD CmdBufW_I1[1024];
	WORD CmdBufW_I2[1024];
	WORD CmdBufRd_I[1024];

	WORD AtmUsBuf[USB_US_BUF_SIZE];
	WORD AtmDsBuf[USB_DS_BUF_SIZE];
	WORD IntBuf[2][17];
} USB_MEMORY, *PUSB_MEMORY;

//	USB Endpoints
//	-------------
enum {
	USB_EP0,
	EP_INTERRUPT,
	EP_OBC_ISO_OUT,
	EP_OBC_ISO_IN,
	EP_ATM_ISO_OUT,
	EP_ATM_ISO_IN,
	EP_OBC_INT_OUT,
	EP_OBC_INT_IN,
	EP_MAX,
};

//	USB_BRIDGE Register Map
//	=======================
#define	UR_CFW			0x00	// USB Configuration Word
#define	UR_ISDR			0x01	// USB Interrupt Status & Data Register
#define	UR_STAT			0x02	// USB Internal Status Register

//	CFG_MEM Register Map
//	--------------------
#define	UR_C_ADR		0x20	// SPI ADR & CONTROL Register
#define	UR_C_DATA		0x21	// SPI DATA Register

//	ADSL uP Register Map: REGS
//	--------------------------
#define	UR_STATUS		0x40	// Status Control Register
#define	UR_IDATA_1		0x41	// Indirect Data 1 Register (LSB)
#define	UR_IDATA_2		0x42	// Indirect Data 2 Register
#define	UR_IDATA_3		0x43	// Indirect Data 3 Register (MSB)
#define	UR_IDATA		0x44	// Indirect Data 3 Register
#define	UR_IADR			0x45	// Indirect Address Register
#define	UR_IMASKS		0x46	// Mask to apply to Slave Indirect R/W
#define	UR_IRMW			0x47	// Mask to apply to Slave R/M/W access
#define	UR_IADR_TX		0x48	// Indirect TX Reference Address Register
#define	UR_IADR_RX		0x49	// Indirect RX Reference Address Register
#define	UR_IADR_MSG		0x4A	// Indirect MSG Reference Address Register
#define	UR_IADR_CHK		0x4B	// Indirect Address Check Register
#define	UR_IADR_BURST	0x4C	// Burst Base Address Register
#define	UR_SIZE_BURST	0x4D	// Number of Burst accesses to be executed
#define	UR_IMASKM		0x4E	// Mask to apply to the Master Ind or Msg R/W
#define	UR_IADR_IRQ		0x4F	// Indirect Address of Tosca IRQ Table
#define	UR_ITABLE		0x50	// Interrupt Table Data Register
#define	UR_ISR			0x5F	// Interrupt Status Register

//	ADSL uP Register Map: PERIPHERALs
//	---------------------------------
#define	UR_GPIO_DATA	0x60	// Status of the GPIO pins
#define	UR_GPIO_DIR		0x61	// Direction of the GPIO pins
#define	UR_GPIO_PER		0x62	// Persistency values of the GPIO pins
#define	UR_TIM_REG_A	0x63	// Timer A mode and preset value
#define	UR_TIM_REG_B	0x64	// Timer B mode and preset value
#define	UR_AFE_TEST		0x65	// AFE Feedback Control Register

//	USB insterrupt status register bitfields
//	----------------------------------------
#define	UISDR_TIRQ1		0x0001	// ADSL uP interrupt flag 1
#define	UISDR_TIRQ2		0x0002	// ADSL uP interrupt flag 2
#define	UISDR_UTIRQ1	0x0004	// UTOPIA rising edge FIFO interrupt flag
#define	UISDR_UTIRQ2	0x0008	// UTOPIA falling edge FIFO interrupt flag
#define	UISDR_ERR_ATM	0x0010	// ATM operation Error flag
#define	UISDR_ERR_OBC	0x0020	// WR OBC operation Error flag
#define	UISDR_ERR_PIPE	0x0040	// WR OBC Access Error flag
#define	UISDR_ACTDIF	0x0080	// ACTD interrupt flag
#define	UISDR_INT_LO	0x0100	// INT_LO flag
#define	UISDR_TIE1		0x0200	// Enable ADSL uP interrupt 1
#define	UISDR_TIE2		0x0400	// Enable ADSL uP interrupt 2
#define	UISDR_IE		0x0800	// Enable Error interrupts
#define	UISDR_LOE		0x1000	// Enable INT_LO interrupts
#define	UISDR_UTIE1		0x2000	// Enable UTOPIA FIFO interrupts
#define	UISDR_UTIE2		0x4000	// Enable UTOPIA FIFO interrupts
#define	UISDR_ACTDIE	0x8000	// Enable ACTD interrupts

#define	UISDR_IF		0x018F	// Mask for all interrupts flags
#define	UISDR_ERF		0x0070	// Mask for all Error flags
#define	UISDR_IEF		0xFE00	// Mask for all Enable flags


//	Control COMMAND bitfields
//	-------------------------
#define	CTRL_FIRST		0x0001	// First control command in pipe
#define	CTRL_LASTI		0x0002	// Last Frame of control cmds in pipe
#define	CTRL_SET_OBCI	0x0004	// Connect WR OBC FIFO to EP6
#define	CTRL_RST_OBCI	0x0008	// Connect WR OBC FIFO to EP2
#define	CTRL_SET_OBCO	0x0010	// Connect RD OBC FIFO to EP7
#define	CTRL_RST_OBCO	0x0020	// Connect RD OBC FIFO to EP3
#define	CTRL_SET_ATM	0x0040	// Connect RD OBC FIFO to EP5
#define	CTRL_RST_ATM	0x0080	// Connect RD OBC FIFO to EP3/EP7
#define	CTRL_EWRITE		0x0100	// Enable Write Command
#define	CTRL_LINK		0x0200	// Next DWORD is another control command
#define	CTRL_ADR		0xFC00	// Mask for ADR field

//	LED bitfields
//	-------------
#define LED_POWER       0x0800
#define LED_SHOWTIME    0x0200
#define LED_INIT        0x0400

#endif

#endif
