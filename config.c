#include <stdio.h>
#include <stdlib.h>
#include "config.h"

static struct parport_config pp_config[4];

static void read_config() {
	int i;
	static int config_read = 0;

	if (config_read)
		return;
	
	config_read = 1;

	for (i=0; i<sizeof(pp_config)/sizeof(struct parport_config); i++) {
		pp_config[i].num = i;
		pp_config[i].ppbase = i*0x10;
		pp_config[i].real = 1;
	}

#ifdef JTAGKEY
	pp_config[3].real = 0;
	pp_config[3].usb_vid = 0x0403;
	pp_config[3].usb_pid = 0xcff8;
#endif
}

unsigned char config_is_real_pport(int num) {
	int ret = 1;
	int i;

	read_config();
	
	for (i=0; i<sizeof(pp_config)/sizeof(struct parport_config); i++) {
		if (pp_config[i].num == num) {
			ret = pp_config[i].real;
			break;
		}
	}

	return ret;
}

unsigned short config_usb_vid(int num) {
	unsigned short ret = 0x00;
	int i;
	
	read_config();
	
	for (i=0; i<sizeof(pp_config)/sizeof(struct parport_config); i++) {
		if (pp_config[i].num == num) {
			ret = pp_config[i].usb_vid;
			break;
		}
	}

	return ret;
}

unsigned short config_usb_pid(int num) {
	unsigned short ret = 0x00;
	int i;
	
	read_config();
	
	for (i=0; i<sizeof(pp_config)/sizeof(struct parport_config); i++) {
		if (pp_config[i].num == num) {
			ret = pp_config[i].usb_pid;
			break;
		}
	}

	return ret;
}
