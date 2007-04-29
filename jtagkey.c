#include <stdio.h>
#include <ftdi.h>
#include <unistd.h>
#include "usb-driver.h"
#include "jtagkey.h"

#define USBBUFSIZE 4096

static struct ftdi_context ftdic;
static unsigned int usb_maxlen = 0;
static unsigned char bitbang_mode;

int jtagkey_init(unsigned short vid, unsigned short pid) {
	int ret = 0;
	unsigned char c;

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


	if ((ret = ftdi_write_data_get_chunksize(&ftdic, &usb_maxlen))  != 0) {
		fprintf(stderr, "unable to get write chunksize: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	if ((ret = ftdi_set_latency_timer(&ftdic, 1))  != 0) {
		fprintf(stderr, "unable to set latency timer: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	if ((ret = ftdi_set_baudrate(&ftdic, 500000))  != 0) {
		fprintf(stderr, "unable to set baudrate: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	c = 0x00;
	ftdi_write_data(&ftdic, &c, 1);

	if ((ret = ftdi_set_bitmode(&ftdic, JTAGKEY_TCK|JTAGKEY_TDI|JTAGKEY_TMS|JTAGKEY_OEn, BITMODE_BITBANG))  != 0) {
		fprintf(stderr, "unable to enable bitbang mode: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	bitbang_mode = BITMODE_BITBANG;

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

static int jtagkey_set_bbmode(unsigned char mode) {
	int ret = 0;

	if (bitbang_mode != mode) {
		DPRINTF("switching bitbang-mode!\n");

		/* Wait for the latency-timer to kick in */
		usleep(2);
		if ((ret = ftdi_set_bitmode(&ftdic, JTAGKEY_TCK|JTAGKEY_TDI|JTAGKEY_TMS|JTAGKEY_OEn, mode))  != 0) {
			fprintf(stderr, "unable to enable bitbang mode: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
			return ret;
		}
		if ((ret = ftdi_usb_purge_buffers(&ftdic))  != 0) {
			fprintf(stderr, "unable to purge buffers: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
			return ret;
		}
		/* Wait for the FTDI2232 to settle */
		usleep(2);

		bitbang_mode = mode;
	}

	return ret;
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
	int nread = 0;
	unsigned long port;
	unsigned char val;
	static unsigned char last_data = 0;
	static unsigned char last_write = 0x00;
	static unsigned char writebuf[USBBUFSIZE], *writepos = writebuf;
	static unsigned char readbuf[USBBUFSIZE], *readpos;
	unsigned char data, prev_data;

	/* Count reads */
	for (i = 0; i < num; i++)
		if (tr[i].cmdTrans == PP_READ)
			nread++;

	/* Write combining */
	if ((writepos-writebuf > sizeof(writebuf)-num) || (nread && writepos-writebuf)) {
		unsigned char *pos = writebuf;
		int len;
		DPRINTF("writing %d bytes due to %d following reads in %d chunks or full buffer\n", writepos-writebuf, nread, num);

		jtagkey_set_bbmode(BITMODE_BITBANG);
		while (pos < writepos) {
			len = writepos-pos;

			if (len > usb_maxlen)
				len = usb_maxlen;

			DPRINTF("combined write of %d/%d\n",len,writepos-pos);
			ftdi_write_data(&ftdic, pos, len);
			pos += len;
		}

		DPRINTF("read %d/%d bytes\n", i, writepos-writebuf);
		writepos = writebuf;
	}

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
		*writepos = last_data;
		prev_data = last_data;

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
						data = JTAGKEY_OEn;
						DPRINTF("CTRL\n");
					} else {
						DPRINTF("!CTRL\n");
					}

					if (val & PP_PROG) {
						DPRINTF("PROG\n");
					} else {
						DPRINTF("!PROG\n");
					}

					*writepos = data;

					last_data = data;
					last_write = val;
					break;

				default:
					fprintf(stderr,"!!!Unsupported TRANSFER command: %lu!!!\n", tr[i].cmdTrans);
					ret = -1;
					break;
			}
		}

		if (nread || (*writepos != prev_data))
			writepos++;
	}

	if (nread)
	{
		DPRINTF("writing %d bytes\n", writepos-writebuf);

		*writepos = last_data;
		writepos++;

		jtagkey_set_bbmode(BITMODE_SYNCBB);
		ftdi_write_data(&ftdic, writebuf, writepos-writebuf);

		i = 0;
		while (i < writepos-writebuf) {
			i += ftdi_read_data(&ftdic, readbuf, sizeof(readbuf));
		}

#ifdef DEBUG
		DPRINTF("write: ");
		hexdump(writebuf, writepos-writebuf);
		DPRINTF("read: ");
		hexdump(readbuf, i);
#endif

		writepos = writebuf;
	} else {
		return ret;
	}

	readpos = readbuf;

	for (i = 0; i < num; i++) {
		DPRINTF("dwPort: 0x%lx, cmdTrans: %lu, dwbytes: %ld, fautoinc: %ld, dwoptions: %ld\n",
				(unsigned long)tr[i].dwPort, tr[i].cmdTrans, tr[i].dwBytes,
				tr[i].fAutoinc, tr[i].dwOptions);

		port = (unsigned long)tr[i].dwPort;
		val = tr[i].Data.Byte;
		readpos++;

		if (port == ppbase + PP_DATA) {
			if (tr[i].cmdTrans == PP_WRITE) {
				last_write = val;
			}
		} else if (port == ppbase + PP_STATUS) {
			DPRINTF("status port (last write: 0x%x)\n", last_write);
			switch(tr[i].cmdTrans) {
				case PP_READ:
					data = *readpos;

#ifdef DEBUG
					DPRINTF("READ: 0x%x\n", data);
					jtagkey_state(data);
#endif

					val = 0x00;
					if ((data & JTAGKEY_TDO) && (last_write & PP_PROG))
						val |= PP_TDO;

					if (!(last_write & PP_PROG))
						val |= 0x08;

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
