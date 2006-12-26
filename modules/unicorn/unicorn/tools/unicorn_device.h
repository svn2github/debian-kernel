#ifndef __unicorn_device_h__
#define __unicorn_device_h__

#include <stdint.h>
#include <linux/atm.h>
#include <net/if.h>
#include "unicorn.h"

struct ADSL_DEVICE {
	int sd;
	enum {
		ATM_DRIVER=1,
		ETH_DRIVER=2
	} type;
	union {
		struct atmif_sioc atm;
		struct ifreq eth;
	} req;
};
typedef struct ADSL_DEVICE ADSL_DEVICE;

#ifdef __cplusplus
extern "C" {
#endif
	int open_device(ADSL_DEVICE *device);
	void close_device(ADSL_DEVICE *device);
	int msw_ctrl(ADSL_DEVICE *device,T_MswCtrl *ctrl);
#ifdef __cplusplus
}
#endif

#endif
