#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "unicorn_device.h"
 
static void get_oam_stats(ADSL_DEVICE *device,T_oam_stats *oam_stats)
{	
	T_MswCtrl ctrl = {NET_CTL_GET_OAM_STATS,0,0,oam_stats,sizeof(T_oam_stats)};
	memset(oam_stats,0,sizeof(T_oam_stats));
	msw_ctrl(device,&ctrl);
}

static void send_oam(ADSL_DEVICE *device,int vpi,int vci) 
{
	T_MswCtrl ctrl;
	T_atm_channel tx_oam;
	
	if ( vpi==0) {
		tx_oam.type = ATM_OAM_F4;
	} else {
		tx_oam.type = ATM_OAM_F5;
	}
	tx_oam.vpi = vpi;
	tx_oam.vci = vci;
	ctrl.code = NET_CTL_TX_OAM_CELL;
	ctrl.subcode = 0;
	ctrl.buffer = &tx_oam;
	ctrl.length = sizeof(tx_oam);

	msw_ctrl(device,&ctrl);
}

static void usage(char *exec)
{
	fprintf(stderr,"usage: %s <vpi> <vci>\n",exec);
}

int main(int argc, char *argv[])
{
	int vpi,vci;
	int err;
	ADSL_DEVICE device;
	T_oam_stats oam_stats;
	int rx_fe_LB;
	long ms;
	int cells_received;

	if (argc < 3) {
		usage(argv[0]);
		return 2;
	}

	vpi = atoi(argv[1]);
	vci = atoi(argv[2]);
	

	if ((err = open_device(&device)) != 0) {
		printf("Open Device failed : %s\n",strerror(err));
		return err;
	}
	get_oam_stats(&device,&oam_stats);
	rx_fe_LB = oam_stats.rx_fe_LB;
	
	printf("Send OAM loopback cell to %d.%d\n",vpi,vci);
	send_oam(&device,vpi,vci);

	/* listen for replies */
	cells_received = 0;
	ms = 0;
	while (ms < 1000) {
		get_oam_stats(&device,&oam_stats);
		if (oam_stats.rx_fe_LB > rx_fe_LB) {
			printf("Received OAM loopback cell from %d.%d time=%ldms\n",vpi,vci,ms);
			cells_received += oam_stats.rx_fe_LB - rx_fe_LB;
			rx_fe_LB = oam_stats.rx_fe_LB;
			break;
		}
		usleep(1000);
		ms += 1;
	}
	if (!cells_received) {
		printf("No response from %d.%d\n",vpi,vci);
	}
	return 0;
}
