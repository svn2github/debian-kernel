#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "unicorn_status.h"

int do_status(void) 
{
	int err;
	ADSL_DEVICE device;

	if ((err = open_device(&device)) != 0) {
		printf("Open Device failed : %s\n",strerror(err));
		return err;
	}

	printf("Modem State               : %s\n",get_modem_state_string(&device));
        printf("Remote Report             : %s\n",get_modem_event_string(&device));
        printf("Last Failure              : %s\n",get_modem_failure_string(&device));
	printf("Time Connected            : %s\n",get_time_connected_string(&device));
	printf("Modulation                : %s\n",get_modulation_string(&device));
	printf("Rate Us/Ds (Kbps)         : %4d %4d\n",get_us_rate(&device),get_ds_rate(&device));
	printf("Cap. Occupation Us/Ds (%%) : %4d %4d\n", get_us_cap_occ(&device),get_ds_cap_occ(&device));
	printf("Noise Margin Us/Ds (dB)   : %4d %4d\n",get_us_noise_margin(&device),get_ds_noise_margin(&device));
	printf("Attenuation Us/Ds (dB)    : %4d %4d\n",get_us_attenuation(&device),get_ds_attenuation(&device));
	printf("Output Power Us/Ds (dBm)  : %4d %4d\n",get_us_output_power(&device),get_ds_output_power(&device));
	printf("FEC Errors Us/Ds          : %4d %4d\n",get_us_fec_errors(&device),get_ds_fec_errors(&device));
	printf("CRC Errors Us/Ds          : %4d %4d\n",get_us_crc_errors(&device),get_ds_crc_errors(&device));
	printf("HEC Errors Us/Ds          : %4d %4d\n",get_us_hec_errors(&device),get_ds_hec_errors(&device));
	printf("Driver Version            : %s\n",get_driver_version_string(&device));
	printf("Firmware Version          : %s\n",get_version_string(&device));

	close_device(&device);

	return 0;
}

int main(void)
{
	do_status(); 
	return 0;
}
