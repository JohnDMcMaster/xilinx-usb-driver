#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "usb-driver.h"
#include "parport.h"
#ifdef JTAGKEY
#include "jtagkey.h"
#endif
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
		pp_config[i].open = parport_open;
		pp_config[i].close = parport_close;
		pp_config[i].transfer = parport_transfer;
	}

#ifdef JTAGKEY
	pp_config[3].real = 0;
	pp_config[3].usb_vid = 0x0403;
	pp_config[3].usb_pid = 0xcff8;
	pp_config[3].open = jtagkey_open;
	pp_config[3].close = jtagkey_close;
	pp_config[3].transfer = jtagkey_transfer;
#endif
}

struct parport_config *config_get(int num) {
	struct parport_config *ret = NULL;
	int i;

	read_config();
	
	for (i=0; i<sizeof(pp_config)/sizeof(struct parport_config); i++) {
		if (pp_config[i].num == num) {
			ret = &(pp_config[i]);
			break;
		}
	}

	return ret;
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

/* TODO:
int config_init_port(int num) {}
config_get_transfer_fn(int num) {}
*/
