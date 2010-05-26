/* libusb/ppdev connector for XILINX impact
 *
 * Copyright (c) 2007 Michael Gernoth <michael@gernoth.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#define _GNU_SOURCE 1

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
#include <signal.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <bits/wordsize.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <syscall.h>
#include <linux/personality.h>
#include "usb-driver.h"
#include "config.h"
#include "xpcu.h"

static int (*ioctl_func) (int, int, void *) = NULL;
static int *windrvrfds = NULL;
static int windrvrfds_count = 0;
static unsigned long ppbase = 0;
static unsigned long ecpbase = 0;
static struct parport_config *pport = NULL;
static FILE *modulesfp = NULL;
static FILE *baseaddrfp = NULL;
static int baseaddrnum = 0;
static int modules_read = 0;

#define NO_WINDRVR 1

void hexdump(unsigned char *buf, int len, char *prefix) {
	int i = 0;

	fprintf(stderr, "%s %03x: ", prefix, i);
	for(i = 0; i<len; i++) {
		fprintf(stderr,"%02x ", buf[i]);
		if ((i % 16) == 7)
			fprintf(stderr," ");
		if (((i % 16) == 15) && ((i+1) < len))
			fprintf(stderr,"\n%s %03x: ", prefix, i+1);
	}
	fprintf(stderr,"\n");
}


static int do_wdioctl(int fd, unsigned int request, unsigned char *wdioctl) {
	struct header_struct* wdheader = (struct header_struct*)wdioctl;
	struct version_struct *version;
	int ret = 0;

	if (wdheader->magic != MAGIC) {
		fprintf(stderr,"!!!ERROR: magic header does not match!!!\n");
		return (*ioctl_func) (fd, request, wdioctl);
	}

	switch(request & ~(0xc0000000)) {
		case VERSION:
			version = (struct version_struct*)(wdheader->data);
			strcpy(version->version, "libusb-driver.so version: " USB_DRIVER_VERSION);
			version->versionul = 802;
			DPRINTF("VERSION\n");
			break;

		case LICENSE:
			DPRINTF("LICENSE\n");
			break;

		case CARD_REGISTER_OLD:
		case CARD_REGISTER:
			DPRINTF("CARD_REGISTER\n");
			{
				struct card_register* cr = (struct card_register*)(wdheader->data);

				DPRINTF("-> Items: %lu, Addr: 0x%lx, bytes: %lu, bar: %lu\n",
				cr->Card.dwItems,
				(unsigned long)cr->Card.Item[0].I.IO.dwAddr,
				cr->Card.Item[0].I.IO.dwBytes,
				cr->Card.Item[0].I.IO.dwBar);
				
				DPRINTF("-> Items: %lu, Addr: 0x%lx, bytes: %lu, bar: %lu\n",
				cr->Card.dwItems,
				(unsigned long)cr->Card.Item[1].I.IO.dwAddr,
				cr->Card.Item[1].I.IO.dwBytes,
				cr->Card.Item[1].I.IO.dwBar);
#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				cr->hCard = 0;

				pport = config_get((unsigned long)cr->Card.Item[0].I.IO.dwAddr / 0x10);
				if (!pport)
					break;

				ret = pport->open((unsigned long)cr->Card.Item[0].I.IO.dwAddr / 0x10);

				ppbase = (unsigned long)cr->Card.Item[0].I.IO.dwAddr;

				if (cr->Card.dwItems > 1 && cr->Card.Item[1].I.IO.dwAddr)
					ecpbase = (unsigned long)cr->Card.Item[1].I.IO.dwAddr;

				if (ret >= 0) {
					cr->hCard = ret;
				} else {
					cr->hCard = 0;
				}
#endif
				DPRINTF("<-hCard: %lu\n", cr->hCard);
			}
			break;

		case USB_TRANSFER:
			DPRINTF("USB_TRANSFER\n");
			{
				struct usb_transfer *ut = (struct usb_transfer*)(wdheader->data);

#ifdef DEBUG
				DPRINTF("-> unique: 0x%lx, pipe: %lu, read: %lu, options: %lx, size: %lu, timeout: %lx\n",
				ut->dwUniqueID, ut->dwPipeNum, ut->fRead,
				ut->dwOptions, ut->dwBufferSize, ut->dwTimeout);
				if (ut->dwPipeNum == 0) {
					DPRINTF("-> setup packet:");
					hexdump(ut->SetupPacket, 8, "");
				}

				if (!ut->fRead && ut->dwBufferSize)
				{
					hexdump(ut->pBuffer, ut->dwBufferSize, "->");
				}
#endif

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				ret = xpcu_transfer(ut);
#endif

#ifdef DEBUG
				DPRINTF("Transferred: %lu (%s)\n",ut->dwBytesTransferred, (ut->fRead?"read":"write"));
				if (ut->fRead && ut->dwBytesTransferred)
				{
					hexdump(ut->pBuffer, ut->dwBytesTransferred, "<-");
				}
#endif
			}
			break;

		case INT_ENABLE_OLD:
		case INT_ENABLE:
			DPRINTF("INT_ENABLE\n");
			{
				struct interrupt *it = (struct interrupt*)(wdheader->data);

				DPRINTF("-> Handle: 0x%lx, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n",
				it->hInterrupt, it->dwOptions,
				it->dwCmds, it->fEnableOk, it->dwCounter,
				it->dwLost, it->fStopped);

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				ret = xpcu_int_state(it, ENABLE_INTERRUPT);
#endif

				DPRINTF("<- Handle: 0x%lx, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n",
				it->hInterrupt, it->dwOptions,
				it->dwCmds, it->fEnableOk, it->dwCounter,
				it->dwLost, it->fStopped);
			}

			break;
			
		case INT_DISABLE:
			DPRINTF("INT_DISABLE\n");
			{
				struct interrupt *it = (struct interrupt*)(wdheader->data);

				DPRINTF("-> Handle: 0x%lx, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n",
				it->hInterrupt, it->dwOptions,
				it->dwCmds, it->fEnableOk, it->dwCounter,
				it->dwLost, it->fStopped);
#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				ret = xpcu_int_state(it, DISABLE_INTERRUPT);
#endif
				DPRINTF("<- Handle: 0x%lx, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n",
				it->hInterrupt, it->dwOptions,
				it->dwCmds, it->fEnableOk, it->dwCounter,
				it->dwLost, it->fStopped);
			}
			break;

		case USB_SET_INTERFACE:
			DPRINTF("USB_SET_INTERFACE\n");
			{
				struct usb_set_interface *usi = (struct usb_set_interface*)(wdheader->data);

				DPRINTF("-> unique: 0x%lx, interfacenum: %lu, alternatesetting: %lu, options: %lx\n",
				usi->dwUniqueID, usi->dwInterfaceNum,
				usi->dwAlternateSetting, usi->dwOptions);
#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				ret = xpcu_set_interface(usi);
#endif
				DPRINTF("<- unique: 0x%lx, interfacenum: %lu, alternatesetting: %lu, options: %lx\n",
				usi->dwUniqueID, usi->dwInterfaceNum,
				usi->dwAlternateSetting, usi->dwOptions);
			}
			break;

		case USB_GET_DEVICE_DATA_OLD:
		case USB_GET_DEVICE_DATA:
			DPRINTF("USB_GET_DEVICE_DATA\n");
			{
				struct usb_get_device_data *ugdd = (struct usb_get_device_data*)(wdheader->data);

				DPRINTF("-> unique: 0x%lx, bytes: %lu, options: %lx\n",
				ugdd->dwUniqueID, ugdd->dwBytes,
				ugdd->dwOptions);

				ret = xpcu_deviceinfo(ugdd);

			}
			break;

		case EVENT_REGISTER_OLD:
		case EVENT_REGISTER:
			DPRINTF("EVENT_REGISTER\n");
			{
				struct event *e = (struct event*)(wdheader->data);
#ifdef DEBUG
				int i;
#endif

				DPRINTF("-> handle: 0x%lx, action: %lu, status: %lu, eventid: %lu, cardtype: %lu, kplug: %lu, options: %lu, dev: %lx:%lx, unique: 0x%lx, ver: %lu, nummatch: %lu\n",
				e->handle, e->dwAction,
				e->dwStatus, e->dwEventId, e->dwCardType,
				e->hKernelPlugIn, e->dwOptions,
				e->u.Usb.deviceId.dwVendorId,
				e->u.Usb.deviceId.dwProductId,
				e->u.Usb.dwUniqueID, e->dwEventVer,
				e->dwNumMatchTables);

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				ret = xpcu_find(e);
#endif

#ifdef DEBUG
				DPRINTF("<- handle: 0x%lx, action: %lu, status: %lu, eventid: %lu, cardtype: %lu, kplug: %lu, options: %lu, dev: %lx:%lx, unique: 0x%lx, ver: %lu, nummatch: %lu\n",
				e->handle, e->dwAction,
				e->dwStatus, e->dwEventId, e->dwCardType,
				e->hKernelPlugIn, e->dwOptions,
				e->u.Usb.deviceId.dwVendorId,
				e->u.Usb.deviceId.dwProductId,
				e->u.Usb.dwUniqueID, e->dwEventVer,
				e->dwNumMatchTables);

				for (i = 0; i < e->dwNumMatchTables; i++)
					DPRINTF("match: dev: %04x:%04x, class: %x, subclass: %x, intclass: %x, intsubclass: %x, intproto: %x\n",
					e->matchTables[i].VendorId,
					e->matchTables[i].ProductId,
					e->matchTables[i].bDeviceClass,
					e->matchTables[i].bDeviceSubClass,
					e->matchTables[i].bInterfaceClass,
					e->matchTables[i].bInterfaceSubClass,
					e->matchTables[i].bInterfaceProtocol);
#endif
			}
			break;

		case TRANSFER_OLD:
		case TRANSFER:
			DPRINTF("TRANSFER\n");
			{
				WD_TRANSFER *tr = (WD_TRANSFER*)(wdheader->data);

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				ret = pport->transfer(tr, fd, request, ppbase, ecpbase, 1);
#endif
			}
			break;

		case MULTI_TRANSFER_OLD:
		case MULTI_TRANSFER:
			DPRINTF("MULTI_TRANSFER\n");
			{
				WD_TRANSFER *tr = (WD_TRANSFER*)(wdheader->data);
				unsigned long num = wdheader->size/sizeof(WD_TRANSFER);
#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				ret = pport->transfer(tr, fd, request, ppbase, ecpbase, num);
#endif
			}
			break;

		case EVENT_UNREGISTER:
			{
				struct event *e = (struct event*)(wdheader->data);

				DPRINTF("EVENT_UNREGISTER\n");
#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				ret = xpcu_close(e);
#endif
			}
			break;

		case INT_WAIT:
			DPRINTF("INT_WAIT\n");
			{
				struct interrupt *it = (struct interrupt*)(wdheader->data);

				DPRINTF("-> Handle: 0x%lx, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n",
				it->hInterrupt, it->dwOptions,
				it->dwCmds, it->fEnableOk, it->dwCounter,
				it->dwLost, it->fStopped);

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				ret = xpcu_int_wait(it);
#endif

				DPRINTF("<- INT_WAIT_RETURN: Handle: 0x%lx, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n",
				it->hInterrupt, it->dwOptions, it->dwCmds,
				it->fEnableOk, it->dwCounter, it->dwLost,
				it->fStopped);
			}
			break;

		case CARD_UNREGISTER:
			DPRINTF("CARD_UNREGISTER\n");
			{
				struct card_register* cr = (struct card_register*)(wdheader->data);

				DPRINTF("-> Addr: 0x%lx, bytes: %lu, bar: %lu\n",
				(unsigned long)cr->Card.Item[0].I.IO.dwAddr,
				cr->Card.Item[0].I.IO.dwBytes,
				cr->Card.Item[0].I.IO.dwBar);

				DPRINTF("-> hCard: %lu\n", cr->hCard);

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				if (pport)
					pport->close(cr->hCard);

				pport = NULL;
#endif
			}
			break;

		case EVENT_PULL:
			DPRINTF("EVENT_PULL\n");
			{
				struct event *e = (struct event*)(wdheader->data);
#ifdef DEBUG
				int i;

				DPRINTF("-> handle: 0x%lx, action: %lu, status: %lu, eventid: %lu, cardtype: %lx, kplug: %lu, options: %lu, dev: %lx:%lx, unique: 0x%lx, ver: %lu, nummatch: %lu\n",
				e->handle, e->dwAction, e->dwStatus,
				e->dwEventId, e->dwCardType, e->hKernelPlugIn,
				e->dwOptions, e->u.Usb.deviceId.dwVendorId,
				e->u.Usb.deviceId.dwProductId,
				e->u.Usb.dwUniqueID, e->dwEventVer,
				e->dwNumMatchTables);

				for (i = 0; i < e->dwNumMatchTables; i++)
					DPRINTF("-> match: dev: %04x:%04x, class: %x, subclass: %x, intclass: %x, intsubclass: %x, intproto: %x\n",
					e->matchTables[i].VendorId,
					e->matchTables[i].ProductId,
					e->matchTables[i].bDeviceClass,
					e->matchTables[i].bDeviceSubClass,
					e->matchTables[i].bInterfaceClass,
					e->matchTables[i].bInterfaceSubClass,
					e->matchTables[i].bInterfaceProtocol);
#endif

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				ret = xpcu_found(e);
#endif

#ifdef DEBUG
				DPRINTF("<- handle: 0x%lx, action: %lu, status: %lu, eventid: %lu, cardtype: %lx, kplug: %lu, options: %lu, dev: %lx:%lx, unique: 0x%lx, ver: %lu, nummatch: %lu\n",
				e->handle, e->dwAction, e->dwStatus,
				e->dwEventId, e->dwCardType, e->hKernelPlugIn,
				e->dwOptions, e->u.Usb.deviceId.dwVendorId,
				e->u.Usb.deviceId.dwProductId,
				e->u.Usb.dwUniqueID, e->dwEventVer,
				e->dwNumMatchTables);

				for (i = 0; i < e->dwNumMatchTables; i++)
					DPRINTF("<- match: dev: %04x:%04x, class: %x, subclass: %x, intclass: %x, intsubclass: %x, intproto: %x\n",
					e->matchTables[i].VendorId,
					e->matchTables[i].ProductId,
					e->matchTables[i].bDeviceClass,
					e->matchTables[i].bDeviceSubClass,
					e->matchTables[i].bInterfaceClass,
					e->matchTables[i].bInterfaceSubClass,
					e->matchTables[i].bInterfaceProtocol);
#endif

			}
			break;

		default:
			fprintf(stderr,"!!!Unsupported IOCTL: %x!!!\n", request);
#ifndef NO_WINDRVR
			ret = (*ioctl_func) (fd, request, wdioctl);
#endif
			break;
	}

	return ret;
}

int ioctl(int fd, unsigned long int request, ...) {
	va_list args;
	void *argp;
	int i;

	if (!ioctl_func)                                                                    
		ioctl_func = (int (*) (int, int, void *)) dlsym (RTLD_NEXT, "ioctl");             

	va_start (args, request);
	argp = va_arg (args, void *);
	va_end (args);

	for (i = 0; i < windrvrfds_count; i++) {
		if (fd == windrvrfds[i])
			return do_wdioctl(fd, request, argp);
	}

	return (*ioctl_func) (fd, request, argp);
}

int open (const char *pathname, int flags, ...) {
	static int (*func) (const char *, int, mode_t) = NULL;
	mode_t mode = 0;
	va_list args;
	int fd;

	if (!func)
		func = (int (*) (const char *, int, mode_t)) dlsym (RTLD_NEXT, "open");

	if (flags & O_CREAT) {
		va_start(args, flags);
		mode = va_arg(args, mode_t);
		va_end(args);
	}

	if (!strcmp (pathname, "/dev/windrvr6")) {
		DPRINTF("opening windrvr6 (%d)\n", windrvrfds_count);
		windrvrfds = realloc(windrvrfds, sizeof(int) * (++windrvrfds_count));
		if (!windrvrfds)
			return -ENOMEM;

#ifdef NO_WINDRVR
		windrvrfds[windrvrfds_count-1] = fd = (*func) ("/dev/null", flags, mode);
#else
		windrvrfds[windrvrfds_count-1] = fd = (*func) (pathname, flags, mode);
#endif

		return fd;
	}

	return (*func) (pathname, flags, mode);
}

int close(int fd) {
	static int (*func) (int) = NULL;
	int i;

	if (!func)
		func = (int (*) (int)) dlsym(RTLD_NEXT, "close");
	
	for (i = 0; i < windrvrfds_count; i++) {
		if (fd == windrvrfds[i] && windrvrfds[i] >= 0) {
			int remaining = windrvrfds_count - (i + 1);
			DPRINTF("close windrvr6 (%d)\n", i);
			if (remaining)
				memmove(&(windrvrfds[i]), &(windrvrfds[i+1]), remaining * sizeof(int));
			windrvrfds = realloc(windrvrfds, sizeof(int) * --windrvrfds_count);
			if (!windrvrfds_count)
				windrvrfds = NULL;
			break;
		}
	}

	return (*func) (fd);
}

FILE *fopen(const char *path, const char *mode) {
	FILE *ret;
	static FILE* (*func) (const char*, const char*) = NULL;
	char buf[256];
	int i;

	if (!func)
		func = (FILE* (*) (const char*, const char*)) dlsym(RTLD_NEXT, "fopen");

	for (i = 0; i < 4; i++) {
		snprintf(buf, sizeof(buf), "/proc/sys/dev/parport/parport%d/base-addr", i);
		if (!strcmp(path, buf)) {
			DPRINTF("open base-addr of parport%d\n", i);
			if (config_is_real_pport(i)) {
				ret = (*func) (path, mode);
			} else {
				ret = (*func) ("/dev/null", mode);
			}

			if (ret) {
				baseaddrfp = ret;
				baseaddrnum = i;
			}

			return ret;
		}
	}

	ret = (*func) (path, mode);

	if (!strcmp(path, "/proc/modules")) {
		DPRINTF("opening /proc/modules\n");
		if (!ret && errno == ENOENT) {
			/* Hmm.. there appears to be no /proc/modules file
			 * fake it then */
			ret = (*func)("/dev/null", mode);
			DPRINTF("No /proc/modules -- faking\n");
		}
#ifdef NO_WINDRVR
		modulesfp = ret;
		modules_read = 0;
#endif
	}

	return ret;
}

char *fgets(char *s, int size, FILE *stream) {
        static char* (*func) (char*, int, FILE*) = NULL;
	const char modules[][256] = {"windrvr6 1 0 - Live 0xdeadbeef\n", "parport_pc 1 0 - Live 0xdeadbeef\n"};
	char buf[256];
	char *ret = NULL;


	if (!func)
		func = (char* (*) (char*, int, FILE*)) dlsym(RTLD_NEXT, "fgets");
	
	if (modulesfp == stream) {
		if (modules_read < sizeof(modules) / sizeof(modules[0])) {
			strcpy(s, modules[modules_read]);
			ret = s;
			modules_read++;
		}
	} else if (baseaddrfp == stream) {
		snprintf(s, sizeof(buf), "%d\t%d\n",
			(baseaddrnum) * 0x10,
			((baseaddrnum) * 0x10) + 0x400);
		ret = s;
	} else {
		ret = (*func)(s,size,stream);
	}

	return ret;
}

int fclose(FILE *fp) {
	static int (*func) (FILE*) = NULL;

	if (!func)
		func = (int (*) (FILE*)) dlsym(RTLD_NEXT, "fclose");

	if (fp == modulesfp) {
		modulesfp = NULL;
	}

	if (fp == baseaddrfp) {
		baseaddrfp = NULL;
	}
	
	return (*func)(fp);
}

int access(const char *pathname, int mode) {
	static int (*func) (const char*, int);

	if (!func)
		func = (int (*) (const char*, int)) dlsym(RTLD_NEXT, "access");

	if (pathname && !strcmp(pathname, "/dev/windrvr6")) {
		return 0;
	} else {
		return (*func)(pathname, mode);
	}
}

#if 0
/* USB cable sharing needs to overload semop, TODO! */
int semop (int __semid, struct sembuf *__sops, size_t __nsops) {
	static int (*func) (int, struct sembuf*, size_t) = NULL;
	int i;

	if (!func)
		func = (int (*) (int, struct sembuf*, size_t)) dlsym(RTLD_NEXT, "semop");
	
	fprintf(stderr,"semop: semid: 0x%X, elements: %d\n", __semid, __nsops);
	for (i = 0; i < __nsops; i++) {
		fprintf(stderr, " num: %u, op: %d, flg: %d\n", __sops[i].sem_num, __sops[i].sem_op, __sops[i].sem_flg);
		if (__sops[i].sem_op < 0) {
			fprintf(stderr, "SEMAPHORE LOCK\n");
		} else {
			fprintf(stderr, "SEMAPHORE UNLOCK\n");
		}
	}

	return (*func)(__semid, __sops, __nsops);
}
#endif

/*
 * Ugly hack for ISE 12. Preload doesn't seem to work correctly for
 * libImpactComm.so. Even though the file is still read with fopen(),
 * the version from libc is used and not the one from this file.
 * Replace the function calling fopen() instead...
 * echo '_Z14isModuleLoadedPci' | c++filt
 */
long int _Z14isModuleLoadedPci(char *module_name, int i) {
	DPRINTF("_Z14isModuleLoadedPci: Checking for module %s (%d)\n", module_name, i);

	return 1;
}

void cpr_segv_handler(int sig, siginfo_t *info, void *context) {
	void *newmem;

	DPRINTF("SEGV at %p, mapping memory\n", info->si_addr);
	errno = 0;
	newmem = mmap(info->si_addr, 1, PROT_READ, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED, -1, 0);
	if (newmem != info->si_addr) {
		perror("libusb-driver.so: Can't map memory, crashing now");
		exit(EXIT_FAILURE);
	}
}

/* XilCommNS::CPortResources::Instance() */
void* _ZN9XilCommNS14CPortResources8InstanceEv() {
	static void* (*func) (void) = NULL;
	char *filename = NULL;
	void *ret;
	int i;

	if (!func) {
		struct sigaction act, oldact;
		int sighand_installed = 0;

		func = (void* (*) (void)) dlsym(RTLD_NEXT, "_ZN9XilCommNS14CPortResources8InstanceEv");

		DPRINTF("Installing signal-handler for SIGSEGV\n");
		bzero(&act, sizeof(struct sigaction));
		act.sa_sigaction = cpr_segv_handler;
		act.sa_flags = SA_SIGINFO;
		if (sigaction(SIGSEGV, &act, &oldact) == 0) {
			sighand_installed = 1;
		}

		DPRINTF("Searching for filename starting at %p\n", func);
		for(i = 0; i < 32768; i++) {
			if (!strcmp(((char*)func)+i, "/proc/sys/dev/parport/%s/base-addr")) {
				filename = ((char*)func)+i;
				DPRINTF("Filename found at offset %p\n", (void*)(filename - ((char*)func)));
				break;
			}
		}
		if (sighand_installed) {
			DPRINTF("Restoring signal-handler for SIGSEGV\n");
			sigaction(SIGSEGV, &oldact, NULL);
		}
		if (!filename)
			fprintf(stderr, "libusb-driver.so: Can't find memory to patch, parallel cables will probably not work!\n");
	}

	if (filename) {
		long pagesize;
		size_t protectlen;
		void *start;
		int len = strlen(filename) + 1;
		int ret;

		pagesize = sysconf(_SC_PAGE_SIZE);
		DPRINTF("You have %lu bytes sized pages!\n", pagesize);

		start = (void*)((long)filename & (~(pagesize-1)));

		protectlen = pagesize;
		if ((long)(filename + len) > (long)(start + protectlen))
			protectlen += pagesize;

		DPRINTF("Unprotecting %zd bytes starting at %p\n", protectlen, start);
		ret = mprotect(start, protectlen, PROT_READ|PROT_WRITE);
		if (ret == -1)
			perror("mprotect");

		DPRINTF("Replacing %s with /dev/zero\n", filename);
		strcpy(filename, "/dev/zero");

		DPRINTF("Reprotecting %zd bytes starting at %p\n", protectlen, start);
		ret = mprotect(start, protectlen, PROT_READ|PROT_EXEC);
		if (ret == -1)
			perror("mprotect");
	}

	DPRINTF("-> XilCommNS::CPortResources::Instance()\n");

	ret = func();

	DPRINTF("<- XilCommNS::CPortResources::Instance()\n");

	return ret;
}

static void __attribute__ ((constructor)) libusbdriver_init(void) {
	int i;
	char buf[256];
	char buf2[256];

	for (i = 0; i < 4; i++) {
		snprintf(buf, sizeof(buf), "XIL_IMPACT_ENV_LPT%d_BASE_ADDRESS", i+1);
		snprintf(buf2, sizeof(buf2), "%x", 0x10*i);
		setenv(buf, buf2, 1);
		snprintf(buf, sizeof(buf), "XIL_IMPACT_ENV_LPT%d_ECP_ADDRESS", i+1);
		snprintf(buf2, sizeof(buf2), "%x", (0x10*i)+0x400);
		setenv(buf, buf2, 1);
	}

	setenv("XIL_IMPACT_USE_LIBUSB", "0", 1);
	setenv("XIL_IMPACT_USE_WINDRIVER", "1", 1);

	#if __WORDSIZE == 32
	{
		struct utsname un;
		int ret;

		ret = uname(&un);

		if (ret == 0 && (!strcmp(un.machine, "x86_64"))) {
			DPRINTF("setting 32bit personality\n");
			(long)syscall(SYS_personality, PER_LINUX32);
		}
	}
	#endif
}
