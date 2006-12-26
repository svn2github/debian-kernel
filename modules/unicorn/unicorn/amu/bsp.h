#ifndef _BSP_H_
#define _BSP_H_

#define SACHEM_TX_DSP			0x0B62	// TX DSP Register
#define AFE_POWER_UP			0x0008	// power up AFE  bit 3
#define AFE_POWER_DOWN			0xFFF7	// power down AFE  bit 3
#define MASK_AFE_POWER_DOWN		0x0008	// Mask for power down AFE  bit 3
#define TX_DSP_MODE_ATUR		0x0001	// ATU-R Mode set
#define MASK_TX_DSP_MODE		0x0001	// Mask for ATU-R Mode setting

#define CONFIG_MODE_BIG			0x0004	// BIG Endian Mode set
#define MASK_BIGEND_MODE		0x0004	// Mask for BIG Endian Mode setting

#define DMT_GPIO_OFFSET			0x40	// General purpose Register
#define DMT_OUT_PIN_HIGH		0x0004	// High level  bit 2 (GP-OUT)
#define DMT_OUT_PIN_LOW			0xFFFB	// Low level  bit 2 (GP-OUT)
#define MASK_DMT_OUT_PIN_LOW	0x0004	// Mask Low level  bit 2 (GP-OUT)
#define DMT_IN_PIN0_MASK		0x0001	// GP_IN0  bit 0
#define DMT_IN_PIN1_MASK		0x0002	// GP_IN1  bit 1

#ifdef __cplusplus
extern "C" {
#endif

unsigned short initializeBoard(void);
unsigned long powerUp_Modem_Chipset (unsigned long Chipset);
unsigned long powerDown_Modem_Chipset (unsigned long Chipset);
unsigned long pull_Modem_Chipset_out_of_reset(unsigned long Chipset);
unsigned long put_Modem_Chipset_in_reset(unsigned long Chipset); 

#ifdef __cplusplus
}	// extern "C"
#endif

#endif
