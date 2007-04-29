#include <stdio.h>
#include <ftdi.h>
#include "usb-driver.h"
#include "jtagkey.h"

static struct ftdi_context ftdic;

int jtagkey_init(unsigned short vid, unsigned short pid) {
	int ret = 0;

	if ((ret = ftdi_init(&ftdic)) != 0) {
		fprintf(stderr, "unable to initialise libftdi: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}
	
	if ((ret = ftdi_usb_open(&ftdic, vid, pid)) != 0) {
		fprintf(stderr, "unable to open ftdi device: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	if ((ret = ftdi_usb_reset(&ftdic)) != 0) {
		fprintf(stderr, "unable reset device: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	if ((ret = ftdi_set_interface(&ftdic, INTERFACE_A)) != 0) {
		fprintf(stderr, "unable to set interface: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

#if 0
	if ((ret = ftdi_write_data_set_chunksize(&ftdic, 3))  != 0) {
		fprintf(stderr, "unable to set write chunksize: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}
#endif

	if ((ret = ftdi_set_latency_timer(&ftdic, 1))  != 0) {
		fprintf(stderr, "unable to set latency timer: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	if ((ret = ftdi_set_baudrate(&ftdic, 1000000))  != 0) {
		fprintf(stderr, "unable to set baudrate: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	if ((ret = ftdi_set_bitmode(&ftdic, JTAGKEY_TCK|JTAGKEY_TDI|JTAGKEY_TMS|JTAGKEY_OEn, BITMODE_SYNCBB))  != 0) {
		fprintf(stderr, "unable to enable bitbang mode: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	if ((ret = ftdi_usb_purge_buffers(&ftdic))  != 0) {
		fprintf(stderr, "unable to purge buffers: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	return ret;
}

void jtagkey_close() {
	ftdi_disable_bitbang(&ftdic);
	ftdi_usb_close(&ftdic);
	ftdi_deinit(&ftdic);
}

void jtagkey_state(unsigned char data) {
	fprintf(stderr,"Pins high: ");

	if (data & JTAGKEY_TCK)
		fprintf(stderr,"TCK ");

	if (data & JTAGKEY_TDI)
		fprintf(stderr,"TDI ");

	if (data & JTAGKEY_TDO)
		fprintf(stderr,"TDO ");

	if (data & JTAGKEY_TMS)
		fprintf(stderr,"TMS ");

	if (data & JTAGKEY_VREF)
		fprintf(stderr,"VREF ");
	
	fprintf(stderr,"\n");
}

int jtagkey_transfer(WD_TRANSFER *tr, int fd, unsigned int request, int ppbase, int ecpbase, int num) {
	int ret = 0;
	int i;
	unsigned long port;
	unsigned char val;
	static unsigned char last_write = 0, last_data = 0;
	unsigned char data;
	unsigned char writebuf[4096], readbuf[4096], *pos;

	pos = writebuf;
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

		/* Pad writebuf for read-commands in stream */
		if (tr[i].cmdTrans == 10) {
			*pos = last_data;
			pos++;
		}

		if (port == ppbase + PP_DATA) {
			DPRINTF("data port\n");

			data = 0x00;
			switch(tr[i].cmdTrans) {
				case PP_READ:
					ret = 0; /* We don't support reading of the data port */
					break;

				case PP_WRITE:
					if (val & PP_TDI) {
						data |= JTAGKEY_TDI;
						DPRINTF("TDI\n");
					} else {
						DPRINTF("!TDI\n");
					}
					if (val & PP_TCK) {
						data |= JTAGKEY_TCK;
						DPRINTF("TCK\n");
					} else {
						DPRINTF("!TCK\n");
					}
					if (val & PP_TMS) {
						data |= JTAGKEY_TMS;
						DPRINTF("TMS\n");
					} else {
						DPRINTF("!TMS\n");
					}
					if (val & PP_CTRL) {
						data |= JTAGKEY_OEn;
						DPRINTF("CTRL\n");
					} else {
						DPRINTF("!CTRL\n");
					}
					*pos = data;
					pos++;

					last_write = val;
					last_data = data;
					break;

				default:
					fprintf(stderr,"!!!Unsupported TRANSFER command: %lu!!!\n", tr[i].cmdTrans);
					ret = -1;
					break;
			}
		}
	}

	ftdi_usb_purge_buffers(&ftdic);
	ftdi_write_data(&ftdic, writebuf, pos-writebuf);

	i = 0;
	do {
#if 0
		ftdi_write_data(&ftdic, &last_data, 1);
#endif
		i += ftdi_read_data(&ftdic, readbuf, sizeof(readbuf));
	} while (i < pos-writebuf);

#ifdef DEBUG
	DPRINTF("write: ");
	hexdump(writebuf, pos-writebuf);
	DPRINTF("read: ");
	hexdump(readbuf, i);
#endif

	pos = readbuf;

	for (i = 0; i < num; i++) {
		DPRINTF("dwPort: 0x%lx, cmdTrans: %lu, dwbytes: %ld, fautoinc: %ld, dwoptions: %ld\n",
				(unsigned long)tr[i].dwPort, tr[i].cmdTrans, tr[i].dwBytes,
				tr[i].fAutoinc, tr[i].dwOptions);

		port = (unsigned long)tr[i].dwPort;
		val = tr[i].Data.Byte;
		pos++;

		if (port == ppbase + PP_STATUS) {
			DPRINTF("status port (last write: %d)\n", last_write);
			switch(tr[i].cmdTrans) {
				case PP_READ:
					data = *pos;
#ifdef DEBUG
					DPRINTF("READ: 0x%x\n", data);
					jtagkey_state(data);
#endif

					val = 0x00;
					if (data & JTAGKEY_TDO)
						val |= PP_TDO;

					if (last_write & 0x40)
						val |= 0x20;
					else
						val |= 0x80;
					break;

				case PP_WRITE:
					ret = 0; /* Status Port is readonly */
					break;

				default:
					fprintf(stderr,"!!!Unsupported TRANSFER command: %lu!!!\n", tr[i].cmdTrans);
					ret = -1;
					break;
			}
		} else {
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
