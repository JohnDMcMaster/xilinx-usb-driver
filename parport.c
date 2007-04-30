#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/parport.h>
#include <linux/ppdev.h>
#include "usb-driver.h"
#include "parport.h"

static int parportfd = -1;

int parport_transfer(WD_TRANSFER *tr, int fd, unsigned int request, int ppbase, int ecpbase, int num) {
	int ret = 0;
	int i;
	unsigned long port;
	unsigned char val;
	static unsigned char last_pp_write = 0;

	for (i = 0; i < num; i++) {
		DPRINTF("dwPort: 0x%lx, cmdTrans: %lu, dwbytes: %ld, fautoinc: %ld, dwoptions: %ld\n",
				(unsigned long)tr[i].dwPort, tr[i].cmdTrans, tr[i].dwBytes,
				tr[i].fAutoinc, tr[i].dwOptions);

		port = (unsigned long)tr[i].dwPort;
		val = tr[i].Data.Byte;

#ifdef DEBUG
		if (tr[i].cmdTrans == 13)
			DPRINTF("write byte: %d\n", val);
#endif

		if (parportfd < 0)
			return ret;

		if (port == ppbase + PP_DATA) {
			DPRINTF("data port\n");
			switch(tr[i].cmdTrans) {
				case PP_READ:
					ret = 0; /* We don't support reading of the data port */
					break;

				case PP_WRITE:
					ret = ioctl(parportfd, PPWDATA, &val);
					last_pp_write = val;
					break;

				default:
					fprintf(stderr,"!!!Unsupported TRANSFER command: %lu!!!\n", tr[i].cmdTrans);
					ret = -1;
					break;
			}
		} else if (port == ppbase + PP_STATUS) {
			DPRINTF("status port (last write: %d)\n", last_pp_write);
			switch(tr[i].cmdTrans) {
				case PP_READ:
					ret = ioctl(parportfd, PPRSTATUS, &val);
#ifdef FORCE_PC3_IDENT
					val &= 0x5f;
					if (last_pp_write & 0x40)
						val |= 0x20;
					else
						val |= 0x80;
#endif
					break;

				case PP_WRITE:
					ret = 0; /* Status Port is readonly */
					break;

				default:
					fprintf(stderr,"!!!Unsupported TRANSFER command: %lu!!!\n", tr[i].cmdTrans);
					ret = -1;
					break;
			}
		} else if (port == ppbase + PP_CONTROL) {
			DPRINTF("control port\n");
			switch(tr[i].cmdTrans) {
				case PP_READ:
					ret = ioctl(parportfd, PPRCONTROL, &val);
					break;

				case PP_WRITE:
					ret = ioctl(parportfd, PPWCONTROL, &val);
					break;

				default:
					fprintf(stderr,"!!!Unsupported TRANSFER command: %lu!!!\n", tr[i].cmdTrans);
					ret = -1;
					break;
			}
		} else if ((port == ecpbase + PP_ECP_CFGA) && ecpbase) {
			DPRINTF("ECP_CFGA port\n");
		} else if ((port == ecpbase + PP_ECP_CFGB) && ecpbase) {
			DPRINTF("ECP_CFGB port\n");
		} else if ((port == ecpbase + PP_ECP_ECR) && ecpbase) {
			DPRINTF("ECP_ECR port\n");
		} else {
			DPRINTF("access to unsupported address range!\n");
			ret = 0;
		}

		tr[i].Data.Byte = val;

		DPRINTF("dwPortReturn: 0x%lx, cmdTrans: %lu, dwbytes: %ld, fautoinc: %ld, dwoptions: %ld\n",
				(unsigned long)tr[i].dwPort, tr[i].cmdTrans, tr[i].dwBytes,
				tr[i].fAutoinc, tr[i].dwOptions);
#ifdef DEBUG
		if (tr[i].cmdTrans == 10)
			DPRINTF("read byte: %d\n", tr[i].Data.Byte);
#endif
	}

	return ret;
}

int parport_open(int num) {
	char ppdev[32];

	if (parportfd < 0) {
		snprintf(ppdev, sizeof(ppdev), "/dev/parport%u", num);
		DPRINTF("opening %s\n", ppdev);
		parportfd = open(ppdev, O_RDWR|O_EXCL);

		if (parportfd < 0)
			fprintf(stderr,"Can't open %s: %s\n", ppdev, strerror(errno));
	}

	if (parportfd >= 0) {
		int pmode;

		if (ioctl(parportfd, PPCLAIM) == -1)
			return -1;

		pmode = IEEE1284_MODE_COMPAT;
		if (ioctl(parportfd, PPNEGOT, &pmode) == -1)
			return -1;

#if 0
		if (cr->Card.dwItems > 1 && cr->Card.Item[1].I.IO.dwAddr) {
			DPRINTF("ECP mode requested\n");
			ecpbase = (unsigned long)cr->Card.Item[1].I.IO.dwAddr;
			/* TODO: Implement ECP mode */
			pmode = IEEE1284_MODE_ECP;

			if (ioctl(parportfd, PPNEGOT, &pmode) == -1) {
				ecpbase = 0;
				pmode = IEEE1284_MODE_COMPAT;
				if (ioctl(parportfd, PPNEGOT, &pmode) == -1)
					return ret;
			}
		}
#endif
	}

	return parportfd;
}

void parport_close(int handle) {
	if (parportfd == handle && parportfd >= 0) {
		ioctl(parportfd, PPRELEASE);
		close(parportfd);
		parportfd = -1;
	}
}
