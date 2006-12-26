#include "types.h"
#include "tracetool.h"
#include "hal.h"
#include "hard.h"
#include "rapi.h"
#include "bsp.h"		// AMAS API's and typedefs

#ifndef _PCI_DRIVER
#error not_PCI_DRIVER
#endif

unsigned short initializeBoard(void)
{
	T_SlaveMaskData p;
	ST_STATUS status;

	// TX_DSP_Register configured in ATU-R Mode
	p.iaddr  = SACHEM_TX_DSP;
	p.idata  = TX_DSP_MODE_ATUR;
	p.mask   = MASK_TX_DSP_MODE;
	p.status = 0x2823;
	status = PCI_SlaveRMWrite(&p);
	if (status == FAILURE) PRINT_ERROR("PCI_SlaveRMWrite() failed\n");

	xtm_wkafter(1);

	// AFE in Reset (UNICORN)
	p.iaddr  = DMT_GPIO_OFFSET;
	p.idata  = DMT_OUT_PIN_LOW;
	p.mask   = MASK_DMT_OUT_PIN_LOW;
	p.status = 0x2823;
	status = PCI_SlaveRMWrite(&p);
	if (status == FAILURE) PRINT_ERROR("PCI_SlaveRMWrite() failed\n");

	xtm_wkafter(1);

	// AFE in Power Down (UNICORN)
	p.iaddr  = SACHEM_TX_DSP;
	p.idata  = AFE_POWER_DOWN; // fisaksen
	p.mask   = MASK_AFE_POWER_DOWN;
	p.status = 0x2823;
	status = PCI_SlaveRMWrite(&p);
	if (status == FAILURE) PRINT_ERROR("PCI_SlaveRMWrite() failed\n");

	xtm_wkafter(1);

	// AFE out of Reset (UNICORN)
	p.iaddr  = DMT_GPIO_OFFSET;
	p.idata  = DMT_OUT_PIN_HIGH;
	p.mask   = MASK_DMT_OUT_PIN_LOW;
	p.status = 0x2823;
	status = PCI_SlaveRMWrite(&p);
	if (status == FAILURE) PRINT_ERROR("PCI_SlaveRMWrite() failed\n");

	xtm_wkafter(1);

	// AFE in PowerUp (UNICORN)
	p.iaddr  = SACHEM_TX_DSP;
	p.idata  = AFE_POWER_UP; // fisaksen
	p.mask   = MASK_AFE_POWER_DOWN;
	p.status = 0x2823;
	status = PCI_SlaveRMWrite(&p);
	if (status == FAILURE) PRINT_ERROR("PCI_SlaveRMWrite() failed\n");

	xtm_wkafter(1);
	return 0;
}

unsigned long powerUp_Modem_Chipset (unsigned long Chipset)
{
	T_SlaveMaskData p;
	ST_STATUS status;

	// AFE out of Reset (UNICORN)
	p.iaddr  = DMT_GPIO_OFFSET;
	p.idata  = DMT_OUT_PIN_HIGH;
	p.mask   = MASK_DMT_OUT_PIN_LOW;
	p.status = 0x2823;
	status = PCI_SlaveRMWrite(&p);
	if (status == FAILURE) PRINT_ERROR("PCI_SlaveRMWrite() failed\n");
	xtm_wkafter(1);
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

extern "C" void HandleAtmError(void)
{
}

extern "C" void HandleLeds(void)
{
}
