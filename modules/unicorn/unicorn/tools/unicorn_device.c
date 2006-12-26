#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/atmdev.h>
#include "unicorn_device.h"


int open_device(ADSL_DEVICE *device)
{
	unsigned long state=0L;
	T_MswCtrl ctrl ={MSW_CTL_GET_STATE,0,0,&state,sizeof(state)};
	int sd;

	// try first ATM socket
	sd = socket(PF_ATMPVC,SOCK_DGRAM,0);
	if (sd > 0) {
		device->sd = sd;
		device->type = ATM_DRIVER;
		device->req.atm.number = 0;
#ifdef DEBUG
		fprintf(stderr,"PF_ATMPVC socket opened,itf=%d\n",device->req.atm.number);
#endif	
		device->req.atm.arg = &ctrl;
		device->req.atm.length = sizeof(T_MswCtrl);
		if (ioctl(device->sd,ATM_MSW_CTL,&device->req.atm) == 0) {
			return 0;
		} else {
#ifdef DEBUG
			fprintf(stderr,"PF_ATMPVC msw_ctrl failed,,err=%s(%d)\n",strerror(errno),errno);
			close (sd);
#endif	
		}
	}

	// try Ethernet socket
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd > 0) {
		device->sd = sd;
		device->type = ETH_DRIVER;
		strcpy(device->req.eth.ifr_name,UNICORN_ETH_NAME);
#ifdef DEBUG
		fprintf(stderr,"AF_INET socket opened,itf=%s\n",device->req.eth.ifr_name);
#endif	
		device->req.eth.ifr_data = (char *)&ctrl;
		if (ioctl(device->sd,ETH_MSW_CTL,&device->req.eth) == 0) {
			return 0;
		} else {
#ifdef DEBUG
			fprintf(stderr,"AF_INET msw_ctrl failed,err=%s(%d)\n",strerror(errno),errno);
			close (sd);
#endif	
		}
	}
	return errno;
}

void close_device(ADSL_DEVICE *device)
{
	close(device->sd);
	device->sd = -1;
	device->type = 0;
}

int msw_ctrl(ADSL_DEVICE *device,T_MswCtrl *ctrl)
{
	ctrl->retcode = 0;
	if (device->type == ATM_DRIVER) {
		device->req.atm.arg = ctrl;
		device->req.atm.length = sizeof(T_MswCtrl);
		if (ioctl(device->sd,ATM_MSW_CTL,&device->req.atm) < 0) {
			return errno;
		}
		return ctrl->retcode;
	} else if (device->type == ETH_DRIVER) {
		device->req.eth.ifr_data = (char *)ctrl;
		if (ioctl(device->sd,ETH_MSW_CTL,&device->req.eth) < 0) {
			return errno;
		}
		return ctrl->retcode;
	}
	return -1;
}
