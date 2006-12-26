#ifndef __unicorn_status_h__
#define __unicorn_status_h__

#include "unicorn_device.h"

#ifdef __cplusplus
extern "C" {
#endif
	const char *get_modem_state_string(ADSL_DEVICE *device);
	const char *get_modem_event_string(ADSL_DEVICE *device);
	const char *get_modem_failure_string(ADSL_DEVICE *device);
	const char *get_time_connected_string(ADSL_DEVICE *device);
	unsigned long long get_cells_rcvd(ADSL_DEVICE *device);
	unsigned long long get_cells_sent(ADSL_DEVICE *device);
	const char *get_modulation_string(ADSL_DEVICE *device);
	int get_us_rate(ADSL_DEVICE *device);
	int get_ds_rate(ADSL_DEVICE *device);
	int get_us_cap_occ(ADSL_DEVICE *device);
	int get_ds_cap_occ(ADSL_DEVICE *device);
	int get_us_noise_margin(ADSL_DEVICE *device);
	int get_ds_noise_margin(ADSL_DEVICE *device);
	int get_us_attenuation(ADSL_DEVICE *device);
	int get_ds_attenuation(ADSL_DEVICE *device);
	int get_us_output_power(ADSL_DEVICE *device);
	int get_ds_output_power(ADSL_DEVICE *device);
	int get_us_fec_errors(ADSL_DEVICE *device);
	int get_ds_fec_errors(ADSL_DEVICE *device);
	int get_us_crc_errors(ADSL_DEVICE *device);
	int get_ds_crc_errors(ADSL_DEVICE *device);
	int get_us_hec_errors(ADSL_DEVICE *device);
	int get_ds_hec_errors(ADSL_DEVICE *device);
	const char *get_version_string(ADSL_DEVICE *device);
	const char *get_driver_version_string(ADSL_DEVICE *device);
#ifdef __cplusplus
}
#endif

#endif
