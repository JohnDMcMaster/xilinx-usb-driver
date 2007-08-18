#include <stdio.h>
#include <ftdi.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include "usb-driver.h"
#include "config.h"
#include "jtagkey.h"
#include "jtagmon.h"

#define USBBUFSIZE 1048576
#define JTAG_SPEED 100000
#define BULK_LATENCY 2
#define OTHER_LATENCY 1

static struct ftdi_context ftdic;

static int jtagkey_latency(int latency) {
	static int current = 0;
	int ret;

	if (current != latency) {
		DPRINTF("switching latency\n");
		if ((ret = ftdi_set_latency_timer(&ftdic, latency))  != 0) {
			fprintf(stderr, "unable to set latency timer: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
			return ret;
		}
		
		current = latency;
	}

	return ret;
}

static int jtagkey_init(unsigned short vid, unsigned short pid) {
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

	if ((ret = ftdi_write_data_set_chunksize(&ftdic, USBBUFSIZE))  != 0) {
		fprintf(stderr, "unable to set write chunksize: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	if ((ret = ftdi_read_data_set_chunksize(&ftdic, USBBUFSIZE))  != 0) {
		fprintf(stderr, "unable to set read chunksize: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	if ((ret = jtagkey_latency(OTHER_LATENCY)) != 0)
		return ret;

	c = 0x00;
	ftdi_write_data(&ftdic, &c, 1);

	if ((ret = ftdi_set_bitmode(&ftdic, JTAGKEY_TCK|JTAGKEY_TDI|JTAGKEY_TMS|JTAGKEY_OEn, BITMODE_SYNCBB))  != 0) {
		fprintf(stderr, "unable to enable bitbang mode: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	if ((ret = ftdi_set_baudrate(&ftdic, JTAG_SPEED))  != 0) {
		fprintf(stderr, "unable to set baudrate: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	if ((ret = ftdi_usb_purge_buffers(&ftdic))  != 0) {
		fprintf(stderr, "unable to purge buffers: %d (%s)\n", ret, ftdi_get_error_string(&ftdic));
		return ret;
	}

	return ret;
}

int jtagkey_open(int num) {
	int ret;

	ret = jtagkey_init(config_usb_vid(num), config_usb_pid(num));

	if (ret >= 0)
		ret = 0xff;

	return ret;
}

void jtagkey_close(int handle) {
	if (handle == 0xff) {
		ftdi_disable_bitbang(&ftdic);
		ftdi_usb_close(&ftdic);
		ftdi_deinit(&ftdic);
	}
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

struct jtagkey_reader_arg {
	int		num;
	unsigned char	*buf;
};

static void *jtagkey_reader(void *thread_arg) {
	struct jtagkey_reader_arg *arg = (struct jtagkey_reader_arg*)thread_arg;
	int i;

	i = 0;
	DPRINTF("reader for %d bytes\n", arg->num);
	while (i < arg->num) {
		i += ftdi_read_data(&ftdic, arg->buf + i, arg->num - i);
	}
	
	pthread_exit(NULL);
}

/* TODO: Interpret JTAG commands and transfer in MPSSE mode */
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
	unsigned char data, prev_data, last_cyc_write;
	struct jtagkey_reader_arg targ;
	pthread_t reader_thread;

	/* Count reads */
	for (i = 0; i < num; i++)
		if (tr[i].cmdTrans == PP_READ)
			nread++;

	/* Write combining */
	if ((writepos-writebuf > sizeof(writebuf)-num) || (nread && writepos-writebuf)) {
		unsigned char *pos = writebuf;
		int len;

		DPRINTF("writing %d bytes due to %d following reads in %d chunks or full buffer\n", writepos-writebuf, nread, num);
		jtagkey_latency(BULK_LATENCY);

		targ.num = writepos-pos;
		targ.buf = readbuf;
		pthread_create(&reader_thread, NULL, &jtagkey_reader, &targ);

		while (pos < writepos) {
			len = writepos-pos;

			if (len > USBBUFSIZE)
				len = USBBUFSIZE;

			DPRINTF("combined write of %d/%d\n",len,writepos-pos);
			ftdi_write_data(&ftdic, pos, len);
			pos += len;
		}
		pthread_join(reader_thread, NULL);

		writepos = writebuf;
	}

	last_cyc_write = last_write;

	for (i = 0; i < num; i++) {
		DPRINTF("dwPort: 0x%lx, cmdTrans: %lu, dwbytes: %ld, fautoinc: %ld, dwoptions: %ld\n",
				(unsigned long)tr[i].dwPort, tr[i].cmdTrans, tr[i].dwBytes,
				tr[i].fAutoinc, tr[i].dwOptions);

		port = (unsigned long)tr[i].dwPort;
		val = tr[i].Data.Byte;

#ifdef DEBUG
		if (tr[i].cmdTrans == 13)
			DPRINTF("write byte: %d\n", val);

		if (tr[i].cmdTrans == 13)
			jtagmon(val & PP_TCK, val & PP_TMS, val & PP_TDI);
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

		if ((tr[i].cmdTrans == PP_READ) || (*writepos != prev_data) || (i == num-1))
			writepos++;
	}

	if (nread)
	{
		DPRINTF("writing %d bytes\n", writepos-writebuf);

		*writepos = last_data;
		writepos++;

		jtagkey_latency(OTHER_LATENCY);

		targ.num = writepos-writebuf;
		targ.buf = readbuf;
		pthread_create(&reader_thread, NULL, &jtagkey_reader, &targ);
		ftdi_write_data(&ftdic, writebuf, writepos-writebuf);
		pthread_join(reader_thread, NULL);

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
	last_write = last_cyc_write;

	for (i = 0; i < num; i++) {
		DPRINTF("dwPort: 0x%lx, cmdTrans: %lu, dwbytes: %ld, fautoinc: %ld, dwoptions: %ld\n",
				(unsigned long)tr[i].dwPort, tr[i].cmdTrans, tr[i].dwBytes,
				tr[i].fAutoinc, tr[i].dwOptions);

		port = (unsigned long)tr[i].dwPort;
		val = tr[i].Data.Byte;

		if ((tr[i].cmdTrans != PP_READ) && (val == last_write) && (i != num-1))
			continue;

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

					if (~last_write & PP_PROG)
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
