#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "unicorn_status.h"


#define TITLE "Unicorn Status"
#define HEADER "Unicorn Status"

int do_status(void) 
{
	int err;
	ADSL_DEVICE device;

	if ((err = open_device(&device)) != 0) {
		printf("<li>Open Device failed : %s\n",strerror(err));
		return err;
	}

	printf("<li>Modem State : %s\n",get_modem_state_string(&device));
        printf("<li>Remote Report : %s\n",get_modem_event_string(&device));
        printf("<li>Last Failure : %s\n",get_modem_failure_string(&device));
	printf("<li>Time Connected : %s\n",get_time_connected_string(&device));
	printf("<li>ATM cells sent: %ull\n",get_cells_sent(&device));
	printf("<li>ATM cells rcvd: %ull\n",get_cells_rcvd(&device));
	printf("<li>Modulation : %s\n",get_modulation_string(&device));
	printf("<li>Rate Us/Ds (Kbps) : %4d/%4d\n",get_us_rate(&device),get_ds_rate(&device));
	printf("<li>Cap. Occupation Us/Ds (%%) : %4d/%4d\n", get_us_cap_occ(&device),get_ds_cap_occ(&device));
	printf("<li>Noise Margin Us/Ds (dB) : %4d/%4d\n",get_us_noise_margin(&device),get_ds_noise_margin(&device));
	printf("<li>Attenuation Us/Ds (dB) : %4d/%4d\n",get_us_attenuation(&device),get_ds_attenuation(&device));
	printf("<li>Output Power Us/Ds (dBm) : %4d/%4d\n",get_us_output_power(&device),get_ds_output_power(&device));
	printf("<li>FEC Errors Us/Ds : %4d/%4d\n",get_us_fec_errors(&device),get_ds_fec_errors(&device));
	printf("<li>CRC Errors Us/Ds : %4d/%4d\n",get_us_crc_errors(&device),get_ds_crc_errors(&device));
	printf("<li>HEC Errors Us/Ds : %4d/%4d\n",get_us_hec_errors(&device),get_ds_hec_errors(&device));
	printf("<li>Driver Version : %s\n",get_driver_version_string(&device));
	printf("<li>Firmware Version : %s\n",get_version_string(&device));

	close_device(&device);

	return 0;
}

int main(void)
{
	printf("Content-type: text/html\n\n");
	printf("<html> <head>\n");
	printf("<title>%s</title>\n",TITLE);
	printf("</head>\n\n");
	printf("<body>\n"); 
	printf("<h1>%s</h1>\n",HEADER);
	printf("<hr>\n");

	do_status();
	printf("</body> </html>\n");
	return 0;
}

