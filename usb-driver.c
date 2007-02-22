#if defined(__GNUC__) && !defined(__STRICT_ANSI__)

#define _GNU_SOURCE 1

#if defined(RTLD_NEXT)
#define REAL_LIBC RTLD_NEXT
#else
#define REAL_LIBC ((void *) -1L)
#endif

#include <dlfcn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include "xilinx.h"

#define SNIFFLEN 4096
static unsigned char lastbuf[4096];

void hexdump(unsigned char *buf, int len);
void diff(unsigned char *buf1, unsigned char *buf2, int len);

void parse_wdioctlreq(unsigned char *wdioctl, unsigned int request) {
	struct header_struct* wdheader = (struct header_struct*)wdioctl;

	if (wdheader->magic != MAGIC) {
		fprintf(stderr,"!!!ERROR: Header does not match!!!\n");
		return;
	}

	fprintf(stderr, "Request: ");
	switch(request) {
		case VERSION:
			fprintf(stderr,"VERSION");
			break;
		case LICENSE:
			fprintf(stderr,"LICENSE");
			fprintf(stderr," \"%s\" (XX,XX)", ((struct license_struct*)(wdheader->data))->cLicense);
			break;
		case TRANSFER:
			fprintf(stderr,"TRANSFER");
			break;
		case USB_TRANSFER:
			fprintf(stderr,"USB_TRANSFER");
		#if 0
			{
				struct usb_transfer *ut = (struct usb_transfer*)(wdheader->data);

				fprintf(stderr," unique: %d, pipe: %d, read: %d, options: %x, size: %d, timeout: %x\n", ut->dwUniqueID, ut->dwPipeNum, ut->fRead, ut->dwOptions, ut->dwBufferSize, ut->dwTimeout);
				memcpy(lastbuf, ut->pBuffer, ut->dwBufferSize);
				hexdump(ut->pBuffer, ut->dwBufferSize);
				fprintf(stderr,"\n");
			}
			fprintf(stderr,"\n");
#endif
			break;
		case EVENT_UNREGISTER:
			fprintf(stderr,"EVENT_UNREGISTER");
			break;
		case INT_DISABLE:
			fprintf(stderr,"INT_DISABLE");
			break;
		case INT_WAIT:
			fprintf(stderr,"INT_WAIT");
			break;
		case CARD_UNREGISTER:
			fprintf(stderr,"CARD_UNREGISTER");
			break;
		case USB_GET_DEVICE_DATA:
			fprintf(stderr,"USB_GET_DEVICE_DATA");
			break;
		case INT_ENABLE:
			fprintf(stderr,"INT_ENABLE");
			break;
		case EVENT_PULL:
			fprintf(stderr,"EVENT_PULL");
			break;
		case USB_SET_INTERFACE:
			fprintf(stderr,"USB_SET_INTERFACE");
			break;
		case EVENT_REGISTER:
			{
				struct event *e = (struct event*)(wdheader->data);
				fprintf(stderr,"%x:%x ", e->u.Usb.deviceId.dwVendorId, e->u.Usb.deviceId.dwProductId);
				fprintf(stderr,"match: %04x:%04x\n", e->matchTables[0].VendorId, e->matchTables[0].ProductId);
			}
			memcpy(lastbuf, wdheader->data, wdheader->size);
			fprintf(stderr,"EVENT_REGISTER");
			break;
		case CARD_REGISTER:
			break;
		default:
			memcpy(lastbuf, wdheader->data, wdheader->size);
			fprintf(stderr,"\n");
			hexdump(wdheader->data, wdheader->size);
			fprintf(stderr,"\n");
			fprintf(stderr,"Unknown(%x)",request);
	}

	fprintf(stderr, ", size: %d\n", wdheader->size);
}

void parse_wdioctlans(unsigned char *wdioctl, unsigned int request, int result) {
	struct header_struct* wdheader = (struct header_struct*)wdioctl;

	if (wdheader->magic != MAGIC) {
		fprintf(stderr,"!!!ERROR: Header does not match!!!\n");
		return;
	}

	fprintf(stderr, "Answer: %d ", result);
	switch(request) {
		case VERSION:
			fprintf(stderr,"\"%s\" (%d)", ((struct version_struct*)(wdheader->data))->version, ((struct version_struct*)(wdheader->data))->versionul);
			break;
		case LICENSE:
			fprintf(stderr,"\"%s\" (XX,XX)", ((struct license_struct*)(wdheader->data))->cLicense);
			break;
		case TRANSFER:
		case EVENT_UNREGISTER:
		case INT_DISABLE:
		case INT_WAIT:
		case CARD_UNREGISTER:
		case USB_GET_DEVICE_DATA:
		case INT_ENABLE:
		case EVENT_PULL:
		case USB_SET_INTERFACE:
		case CARD_REGISTER:
			break;
		case EVENT_REGISTER:
			fprintf(stderr,"\n");
			diff(lastbuf, wdheader->data, wdheader->size);
			break;
		case USB_TRANSFER:
		#if 0
			{
				struct usb_transfer *ut = (struct usb_transfer*)(wdheader->data);

				fprintf(stderr,"\n");
				hexdump(ut->pBuffer, ut->dwBufferSize);
				fprintf(stderr,"\n");
				diff(lastbuf, ut->pBuffer, ut->dwBufferSize);
			}
		#endif
			break;
		default:
			fprintf(stderr,"\n");
			hexdump(wdheader->data, wdheader->size);
			fprintf(stderr,"\n");
			diff(lastbuf, wdheader->data, wdheader->size);
			break;
	}
	fprintf(stderr, ", size: %d\n", wdheader->size);
}

int do_wdioctl(unsigned int request, unsigned char *wdioctl) {
	struct header_struct* wdheader = (struct header_struct*)wdioctl;
	struct version_struct *version;

	if (wdheader->magic != MAGIC) {
		fprintf(stderr,"!!!ERROR: Header does not match!!!\n");
		return;
	}

	switch(request) {
		case VERSION:
			version = (struct version_struct*)(wdheader->data);
			strcpy(version->version, "WinDriver no more");
			version->versionul = 999;
			fprintf(stderr,"faking VERSION\n");
			break;
		case CARD_REGISTER:
			{
				struct card_register* cr = (struct card_register*)(wdheader->data);
				/* Todo: LPT-Port already in use */
			}
			fprintf(stderr,"faking CARD_REGISTER\n");
			break;
		case USB_TRANSFER:
			fprintf(stderr,"in USB_TRANSFER");
			{
				struct usb_transfer *ut = (struct usb_transfer*)(wdheader->data);

				fprintf(stderr," unique: %d, pipe: %d, read: %d, options: %x, size: %d, timeout: %x\n", ut->dwUniqueID, ut->dwPipeNum, ut->fRead, ut->dwOptions, ut->dwBufferSize, ut->dwTimeout);
				fprintf(stderr,"setup packet: ");
				hexdump(ut->SetupPacket, 8);
				if (!ut->fRead && ut->dwBufferSize)
				{
					hexdump(ut->pBuffer, ut->dwBufferSize);
					fprintf(stderr,"\n");
				}
			}
		case LICENSE:
		case TRANSFER:
		case EVENT_UNREGISTER:
		case INT_DISABLE:
		case INT_WAIT:
		case CARD_UNREGISTER:
		case USB_GET_DEVICE_DATA:
		case INT_ENABLE:
		case EVENT_PULL:
		case USB_SET_INTERFACE:
		case EVENT_REGISTER:
		default:
			return -1;
	}

	return 0;
}


typedef int (*open_funcptr_t) (const char *, int, mode_t);

static windrvrfd = 0;
static void* mmapped = NULL;
static size_t mmapplen = 0;

int open (const char *pathname, int flags, ...)
{
	static open_funcptr_t func = NULL;
	mode_t mode = 0;
	va_list args;
	int fd;

	if (!func)
		func = (open_funcptr_t) dlsym (REAL_LIBC, "open");

	if (flags & O_CREAT) {
		va_start(args, flags);
		mode = va_arg(args, mode_t);
		va_end(args);
	}

	fd = (*func) (pathname, flags, mode);

	if (!strcmp (pathname, "/dev/windrvr6")) {
		fprintf(stderr,"opening windrvr6\n");
		windrvrfd = fd;
	}

	return fd;
}

void diff(unsigned char *buf1, unsigned char *buf2, int len) {
	int i;

	for(i=0; i<len; i++) {
		if (buf1[i] != buf2[i]) {
			fprintf(stderr,"Diff at %d: %02x(%c)->%02x(%c)\n", i, buf1[i], ((buf1[i] >= 31 && buf1[i] <= 126)?buf1[i]:'.'), buf2[i], ((buf2[i] >= 31 && buf2[i] <= 126)?buf2[i]:'.'));
		}
	}
}

void hexdump(unsigned char *buf, int len) {
	int i;

	for(i=0; i<len; i++) {
		fprintf(stderr,"%02x ", buf[i]);
		if ((i % 16) == 15)
			fprintf(stderr,"\n");
	}
}

int ioctl(int fd, int request, ...)
{
	static int (*func) (int, int, void *) = NULL;
	va_list args;
	void *argp;
	int ret;

	if (!func)                                                                    
		func = (int (*) (int, int, void *)) dlsym (REAL_LIBC, "ioctl");             

	va_start (args, request);
	argp = va_arg (args, void *);
	va_end (args);

	if (fd == windrvrfd) {
		parse_wdioctlreq(argp, request);

		if ((ret = do_wdioctl(request, argp)))
			ret = (*func) (fd, request, argp);
	} else {
		ret = (*func) (fd, request, argp);
	}

	if (fd == windrvrfd) {
		parse_wdioctlans(argp, request, ret);
	}
	return ret;
}

#if 0
void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
{
	static void* (*func) (void *, size_t, int, int, int, off_t) = NULL;
	void *ret;

	if (!func)
		func = (void* (*) (void *, size_t, int, int, int, off_t)) dlsym (REAL_LIBC, "mmap");

	ret = (*func) (start, length, prot, flags, fd, offset);
	fprintf(stderr,"MMAP: %x, %d, %d, %d, %d, %d -> %x\n", (unsigned int)start, length, prot, flags, fd, offset, (unsigned int)ret);
	mmapped = ret;
	mmapplen = length;

	return ret;
}

void *mmap64(void *start, size_t length, int prot, int flags, int fd, off64_t offset)
{
	static void* (*func) (void *, size_t, int, int, int, off64_t) = NULL;
	void *ret;

	if (!func)
		func = (void* (*) (void *, size_t, int, int, int, off64_t)) dlsym (REAL_LIBC, "mmap64");

	ret = (*func) (start, length, prot, flags, fd, offset);
	fprintf(stderr,"MMAP64: %x, %d, %d, %d, %d, %lld -> %x\n", (unsigned int)start, length, prot, flags, fd, offset, (unsigned int)ret);
	mmapped = ret;
	mmapplen = length;

	return ret;
}

void *mmap2(void *start, size_t length, int prot, int flags, int fd, off_t pgoffset)
{
	static void* (*func) (void *, size_t, int, int, int, off_t) = NULL;
	void *ret;

	if (!func)
		func = (void* (*) (void *, size_t, int, int, int, off_t)) dlsym (REAL_LIBC, "mmap2");

	ret = (*func) (start, length, prot, flags, fd, pgoffset);
	fprintf(stderr,"MMAP2: %x, %d, %d, %d, %d, %d -> %x\n", (unsigned int)start, length, prot, flags, fd, pgoffset, (unsigned int)ret);
	mmapped = ret;
	mmapplen = length;

	return ret;
}

void *malloc(size_t size)
{
	static void* (*func) (size_t) = NULL;
	void *ret;

	if (!func)
		func = (void* (*) (size_t)) dlsym(REAL_LIBC, "malloc");
	
	ret = (*func) (size);
	
	//fprintf(stderr,"MALLOC: %d -> %x\n", size, (unsigned int) ret);

	return ret;
}
#endif


#endif
