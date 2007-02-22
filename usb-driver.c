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
#include <usb.h>
#include "xilinx.h"

static int (*ioctl_func) (int, int, void *) = NULL;
static int windrvrfd = 0;
static struct usb_bus *busses = NULL;

void hexdump(unsigned char *buf, int len);
void diff(unsigned char *buf1, unsigned char *buf2, int len);

int do_wdioctl(int fd, unsigned int request, unsigned char *wdioctl) {
	struct header_struct* wdheader = (struct header_struct*)wdioctl;
	struct version_struct *version;
	int ret = 0;

	if (wdheader->magic != MAGIC) {
		fprintf(stderr,"!!!ERROR: magic header does not match!!!\n");
		return (*ioctl_func) (fd, request, wdioctl);
	}

	switch(request) {
		case VERSION:
			version = (struct version_struct*)(wdheader->data);
			strcpy(version->version, "WinDriver no more");
			version->versionul = 999;
			fprintf(stderr,"faking VERSION\n");
			break;

		case LICENSE:
			fprintf(stderr,"faking LICENSE\n");
			break;

		case CARD_REGISTER:
			{
				//struct card_register* cr = (struct card_register*)(wdheader->data);
				/* Todo: LPT-Port already in use */
			}
			fprintf(stderr,"faking CARD_REGISTER\n");
			break;

		case USB_TRANSFER:
			fprintf(stderr,"in USB_TRANSFER");
			{
				struct usb_transfer *ut = (struct usb_transfer*)(wdheader->data);

				fprintf(stderr," unique: %lu, pipe: %lu, read: %lu, options: %lx, size: %lu, timeout: %lx\n", ut->dwUniqueID, ut->dwPipeNum, ut->fRead, ut->dwOptions, ut->dwBufferSize, ut->dwTimeout);
				fprintf(stderr,"setup packet: ");
				hexdump(ut->SetupPacket, 8);
				fprintf(stderr,"\n");
				if (!ut->fRead && ut->dwBufferSize)
				{
					hexdump(ut->pBuffer, ut->dwBufferSize);
					fprintf(stderr,"\n");
				}

				ret = (*ioctl_func) (fd, request, wdioctl);

				fprintf(stderr,"Transferred: %lu (%s)\n",ut->dwBytesTransferred, (ut->fRead?"read":"write"));
				if (ut->fRead && ut->dwBytesTransferred)
				{
					fprintf(stderr,"Read: ");
					hexdump(ut->pBuffer, ut->dwBytesTransferred);
				}
				fprintf(stderr,"\n");
			}
			break;

		case INT_ENABLE:
			fprintf(stderr,"faking INT_ENABLE");
			{
				struct interrupt *it = (struct interrupt*)(wdheader->data);

				fprintf(stderr,"Handle: %lu, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n", it->hInterrupt, it->dwOptions, it->dwCmds, it->fEnableOk, it->dwCounter, it->dwLost, it->fStopped);

				it->fEnableOk = 1;
				//ret = (*ioctl_func) (fd, request, wdioctl);
			}

			break;
			
		case INT_DISABLE:
			fprintf(stderr,"INT_DISABLE\n");
			{
				struct interrupt *it = (struct interrupt*)(wdheader->data);

				fprintf(stderr,"Handle: %lu, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n", it->hInterrupt, it->dwOptions, it->dwCmds, it->fEnableOk, it->dwCounter, it->dwLost, it->fStopped);
				//it->dwCounter = 0;
				//it->fStopped = 1;
				ret = (*ioctl_func) (fd, request, wdioctl);
				fprintf(stderr,"Handle: %lu, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n", it->hInterrupt, it->dwOptions, it->dwCmds, it->fEnableOk, it->dwCounter, it->dwLost, it->fStopped);
			}
			break;

		case USB_SET_INTERFACE:
			fprintf(stderr,"USB_SET_INTERFACE\n");
			{
				struct usb_set_interface *usi = (struct usb_set_interface*)(wdheader->data);

				fprintf(stderr,"unique: %lu, interfacenum: %lu, alternatesetting: %lu, options: %lx\n", usi->dwUniqueID, usi->dwInterfaceNum, usi->dwAlternateSetting, usi->dwOptions);
				ret = (*ioctl_func) (fd, request, wdioctl);
			}
			break;

		case USB_GET_DEVICE_DATA:
			fprintf(stderr,"USB_GET_DEVICE_DATA\n");
			{
				struct usb_get_device_data *ugdd = (struct usb_get_device_data*)(wdheader->data);
				int pSize;

				fprintf(stderr, "uniqe: %lu, bytes: %lu, options: %lx\n", ugdd->dwUniqueID, ugdd->dwBytes, ugdd->dwOptions);
				pSize = ugdd->dwBytes;
				ret = (*ioctl_func) (fd, request, wdioctl);
				if (pSize) {
					hexdump(ugdd->pBuf, pSize);
					fprintf(stderr, "\n");
				}
			}
			break;

		case EVENT_REGISTER:
			fprintf(stderr,"EVENT_REGISTER\n");
			{
				struct event *e = (struct event*)(wdheader->data);
				int i;

				fprintf(stderr,"handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lu, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n", e->handle, e->dwAction, e->dwStatus, e->dwEventId, e->dwCardType, e->hKernelPlugIn, e->dwOptions, e->u.Usb.deviceId.dwVendorId, e->u.Usb.deviceId.dwProductId, e->u.Usb.dwUniqueID, e->dwEventVer, e->dwNumMatchTables);
				for (i = 0; i < e->dwNumMatchTables; i++)
					fprintf(stderr,"match: dev: %x:%x, class: %x, subclass: %x, intclass: %x, intsubclass: %x, intproto: %x\n", e->matchTables[i].VendorId, e->matchTables[i].ProductId, e->matchTables[i].bDeviceClass, e->matchTables[i].bDeviceSubClass, e->matchTables[i].bInterfaceClass, e->matchTables[i].bInterfaceSubClass, e->matchTables[i].bInterfaceProtocol);

				ret = (*ioctl_func) (fd, request, wdioctl);

				fprintf(stderr,"handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lu, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n", e->handle, e->dwAction, e->dwStatus, e->dwEventId, e->dwCardType, e->hKernelPlugIn, e->dwOptions, e->u.Usb.deviceId.dwVendorId, e->u.Usb.deviceId.dwProductId, e->u.Usb.dwUniqueID, e->dwEventVer, e->dwNumMatchTables);
				for (i = 0; i < e->dwNumMatchTables; i++)
					fprintf(stderr,"match: dev: %x:%x, class: %x, subclass: %x, intclass: %x, intsubclass: %x, intproto: %x\n", e->matchTables[i].VendorId, e->matchTables[i].ProductId, e->matchTables[i].bDeviceClass, e->matchTables[i].bDeviceSubClass, e->matchTables[i].bInterfaceClass, e->matchTables[i].bInterfaceSubClass, e->matchTables[i].bInterfaceProtocol);
			}
			break;

		case TRANSFER:
			fprintf(stderr,"TRANSFER\n");
			ret = (*ioctl_func) (fd, request, wdioctl);
			break;

		case EVENT_UNREGISTER:
			fprintf(stderr,"EVENT_UNREGISTER\n");
			ret = (*ioctl_func) (fd, request, wdioctl);
			break;

		case INT_WAIT:
			fprintf(stderr,"INT_WAIT\n");
			{
				struct interrupt *it = (struct interrupt*)(wdheader->data);

				fprintf(stderr,"Handle: %lu, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n", it->hInterrupt, it->dwOptions, it->dwCmds, it->fEnableOk, it->dwCounter, it->dwLost, it->fStopped);

				ret = (*ioctl_func) (fd, request, wdioctl);
				fprintf(stderr,"Handle: %lu, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n", it->hInterrupt, it->dwOptions, it->dwCmds, it->fEnableOk, it->dwCounter, it->dwLost, it->fStopped);
			}
			break;

		case CARD_UNREGISTER:
			fprintf(stderr,"CARD_UNREGISTER\n");
			ret = (*ioctl_func) (fd, request, wdioctl);
			break;

		case EVENT_PULL:
			fprintf(stderr,"EVENT_PULL\n");
			{
				struct event *e = (struct event*)(wdheader->data);
				int i;

				fprintf(stderr,"handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lu, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n", e->handle, e->dwAction, e->dwStatus, e->dwEventId, e->dwCardType, e->hKernelPlugIn, e->dwOptions, e->u.Usb.deviceId.dwVendorId, e->u.Usb.deviceId.dwProductId, e->u.Usb.dwUniqueID, e->dwEventVer, e->dwNumMatchTables);
				for (i = 0; i < e->dwNumMatchTables; i++)
					fprintf(stderr,"match: dev: %x:%x, class: %x, subclass: %x, intclass: %x, intsubclass: %x, intproto: %x\n", e->matchTables[i].VendorId, e->matchTables[i].ProductId, e->matchTables[i].bDeviceClass, e->matchTables[i].bDeviceSubClass, e->matchTables[i].bInterfaceClass, e->matchTables[i].bInterfaceSubClass, e->matchTables[i].bInterfaceProtocol);

				ret = (*ioctl_func) (fd, request, wdioctl);

				fprintf(stderr,"handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lu, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n", e->handle, e->dwAction, e->dwStatus, e->dwEventId, e->dwCardType, e->hKernelPlugIn, e->dwOptions, e->u.Usb.deviceId.dwVendorId, e->u.Usb.deviceId.dwProductId, e->u.Usb.dwUniqueID, e->dwEventVer, e->dwNumMatchTables);
				for (i = 0; i < e->dwNumMatchTables; i++)
					fprintf(stderr,"match: dev: %x:%x, class: %x, subclass: %x, intclass: %x, intsubclass: %x, intproto: %x\n", e->matchTables[i].VendorId, e->matchTables[i].ProductId, e->matchTables[i].bDeviceClass, e->matchTables[i].bDeviceSubClass, e->matchTables[i].bInterfaceClass, e->matchTables[i].bInterfaceSubClass, e->matchTables[i].bInterfaceProtocol);
			}
			break;

		default:
			ret = (*ioctl_func) (fd, request, wdioctl);
	}

	return ret;
}


typedef int (*open_funcptr_t) (const char *, int, mode_t);

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
		if (!busses) {
			usb_init();
			usb_find_busses();
			usb_find_devices();

			busses = usb_get_busses();
		}
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
	va_list args;
	void *argp;
	int ret;

	if (!ioctl_func)                                                                    
		ioctl_func = (int (*) (int, int, void *)) dlsym (REAL_LIBC, "ioctl");             

	va_start (args, request);
	argp = va_arg (args, void *);
	va_end (args);

	if (fd == windrvrfd)
		ret = do_wdioctl(fd, request, argp);
	else
		ret = (*ioctl_func) (fd, request, argp);

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
