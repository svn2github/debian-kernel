//#define USB_KERNEL_DEBUG

#include "types.h"
#include "tracetool.h"
#include "hal.h"
#include "hard.h"
#include "rapi.h"
#include "bsp.h"		// AMAS API's and typedefs

#ifndef _USB_DRIVER
#error not_USB_DRIVER
#endif

unsigned short initializeBoard(void)
{

	ST_STATUS status;
	unsigned short iaddr_val, idata_val, irmw_val;

	// TX_DSP_Register configured in ATU-R Mode

	status = USB_controlWrite(UR_IADR,SACHEM_TX_DSP);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_IDATA,TX_DSP_MODE_ATUR);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_IRMW,MASK_TX_DSP_MODE);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_STATUS,0x2823);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");

	PRINT_INFO("USB> TX_DSP_Register configuration DONE!\n");

	// AFE in Reset

	iaddr_val = DMT_GPIO_OFFSET;
	idata_val = DMT_OUT_PIN_LOW;
	irmw_val  = MASK_DMT_OUT_PIN_LOW;

	status = USB_controlWrite(UR_IADR,iaddr_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_IDATA,idata_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_IRMW,irmw_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_STATUS,0x2823);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");

	PRINT_INFO("USB> AFE_RESET: DONE!\n");

	// AFE in PowerDown

	iaddr_val = SACHEM_TX_DSP;
	idata_val = AFE_POWER_UP;
	irmw_val  = MASK_AFE_POWER_DOWN;

	status = USB_controlWrite(UR_IADR,iaddr_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_IDATA,idata_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_IRMW,irmw_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_STATUS,0x2823);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");

	PRINT_INFO("USB> AFE_POWER_DOWN: DONE!\n");

	// AFE out of Reset

	iaddr_val = DMT_GPIO_OFFSET;
	idata_val = DMT_OUT_PIN_HIGH;
	irmw_val  = MASK_DMT_OUT_PIN_LOW;

	status = USB_controlWrite(UR_IADR,iaddr_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_IDATA,idata_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_IRMW,irmw_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_STATUS,0x2823);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");

	PRINT_INFO("USB> AFE_OUT_OF_RESET: DONE!\n");

		// AFE Power Up

	iaddr_val = SACHEM_TX_DSP;
	idata_val = AFE_POWER_DOWN;
	irmw_val  = MASK_AFE_POWER_DOWN;

	status = USB_controlWrite(UR_IADR,iaddr_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_IDATA,idata_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_IRMW,irmw_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");
	status = USB_controlWrite(UR_STATUS,0x2823);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");

	PRINT_INFO("USB> AFE_POWER_UP: DONE!\n");

#ifdef USB_KERNEL_DEBUG
  if ( (status = USB_controlWrite(0x61,0x00)) == FAILURE )
			PRINT_ERROR("Configuration of GPIO_DIR register failed!\n");

	PRINT_INFO("USB> GPIO_DIR: INITIALIZED!\n");
#endif

	return 0;
}

unsigned long powerUp_Modem_Chipset (unsigned long Chipset)
{
	ST_STATUS				status;
	unsigned short  iaddr_val, idata_val, irmw_val;

	iaddr_val = DMT_GPIO_OFFSET;
	idata_val = DMT_OUT_PIN_HIGH;
	irmw_val	= MASK_DMT_OUT_PIN_LOW;

	status = USB_controlWrite(UR_IADR,iaddr_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");

	status = USB_controlWrite(UR_IDATA,idata_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");

	status = USB_controlWrite(UR_IRMW,irmw_val);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");

	status = USB_controlWrite(UR_STATUS,0x2823);
	if (status == FAILURE) PRINT_ERROR("USB_controlWrite() failed\n");

	PRINT_INFO("USB powerUp_Modem_Chipset completed\n");

	return 0;
}


unsigned long powerDown_Modem_Chipset (unsigned long Chipset)
{
	return 0;
}

unsigned long pull_Modem_Chipset_out_of_reset(unsigned long Chipset)
{
	return 0;
}

unsigned long put_Modem_Chipset_in_reset(unsigned long Chipset)
{
	return 0;
}

