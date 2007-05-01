#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "usb-driver.h"
#include "parport.h"
#ifdef JTAGKEY
#include "jtagkey.h"
#endif
#include "config.h"

#define LINELEN 1024

#define PARSEERROR fprintf(stderr,"LIBUSB-DRIVER WARNING: Invalid config statement at line %d\n", line)

static struct parport_config pp_config[4];

static void read_config() {
	int i;
	static int config_read = 0;
	FILE *cfg;
	char buf[LINELEN];
#ifdef JTAGKEY
	char *pbuf;
	unsigned short vid, pid;
	int line, len, num;
#endif

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

	snprintf(buf, sizeof(buf), "%s/.libusb-driverrc", getenv("HOME"));

	cfg = fopen(buf, "r");
	if (cfg) {
#ifdef JTAGKEY
		line = 0;
		do {
			pbuf = fgets(buf, sizeof(buf), cfg);
			if (!pbuf)
				break;

			line++;

			len = strlen(buf);

			if (len > 0 && buf[len-1] == '\n') {
				buf[len-1] = '\0';
				len--;
			}
			if (len > 0 && buf[len-1] == '\r') {
				buf[len-1] = '\0';
				len--;
			}
			
			for (i = 0; i < len; i++) {
				if (buf[i] != ' ' && buf[i] != '\t')
					break;
			}

			if (buf[i] == '#' || buf[i] == ';' || buf[i] == '\0')
				continue;

			if (!strncasecmp(buf+i, "LPT", 3)) {
				unsigned char equal_seen = 0;

				i += 3;
				pbuf = buf+i;
				for (; i < len; i++) {
					if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '=') {
						if (buf[i] == '=')
							equal_seen = 1;

						buf[i] = '\0';
						i++;
						break;
					}
				}

				if (*pbuf == '\0') {
					PARSEERROR;
					continue;
				}

				num = 0;
				num = strtol(pbuf, NULL, 10);
				if (num < 1) {
					PARSEERROR;
					continue;
				}
				num--;

				for (; (i < len) && (!equal_seen) ; i++) {
					if (buf[i] == '=') {
						equal_seen = 1;
						i++;
						break;
					} else if (buf[i] != ' ' && buf[i] != '\t') {
						break;
					}
				}

				if (!equal_seen) {
					PARSEERROR;
					continue;
				}

				for (; i < len; i++) {
					if (buf[i] != ' ' && buf[i] != '\t')
						break;
				}

				if (strncasecmp(buf+i, "FTDI:", 5)) {
					PARSEERROR;
					continue;
				}

				i += 5;
				pbuf = buf + i;

				for (; i < len; i++) {
					if (buf[i] == ':')
						break;
				}

				if (buf[i] != ':') {
					PARSEERROR;
					continue;
				}

				buf[i] = '\0';

				vid = 0;
				vid = strtol(pbuf, NULL, 16);
				if (!num) {
					PARSEERROR;
					continue;
				}

				i++;
				pbuf = buf + i;

				for (; i < len; i++) {
					if (buf[i] == ' ' || buf[i] == '\t')
						break;
				}

				pid = 0;
				pid = strtol(pbuf, NULL, 16);
				if (!num) {
					PARSEERROR;
					continue;
				}

				pp_config[num].real = 0;
				pp_config[num].usb_vid = vid;
				pp_config[num].usb_pid = pid;
				pp_config[num].open = jtagkey_open;
				pp_config[num].close = jtagkey_close;
				pp_config[num].transfer = jtagkey_transfer;
			} else {
				PARSEERROR;
			}
		} while (pbuf);
#else
		fprintf(stderr,"libusb-driver not compiled with FTDI2232-support, config file ignored!\n");
#endif
		fclose(cfg);
	}
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
