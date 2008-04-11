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
#include <usb.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <bits/wordsize.h>
#include "usb-driver.h"
#include "config.h"
#include "xpcu.h"

static int (*ioctl_func) (int, int, void *) = NULL;
static int windrvrfd = -1;
static unsigned long ppbase = 0;
static unsigned long ecpbase = 0;
static struct parport_config *pport = NULL;
static struct xpcu_s *xpcu = NULL;
static FILE *modulesfp = NULL;
static FILE *baseaddrfp = NULL;
static int baseaddrnum = 0;
static int modules_read = 0;
static unsigned long card_type;
static int ints_enabled = 0;
static pthread_mutex_t int_wait = PTHREAD_MUTEX_INITIALIZER;

#define NO_WINDRVR 1

void hexdump(unsigned char *buf, int len) {
	int i;

	for(i=0; i<len; i++) {
		fprintf(stderr,"%02x ", buf[i]);
		if ((i % 16) == 15)
			fprintf(stderr,"\n");
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

				DPRINTF("Items: %lu, Addr: 0x%lx, bytes: %lu, bar: %lu\n",
				cr->Card.dwItems,
				(unsigned long)cr->Card.Item[0].I.IO.dwAddr,
				cr->Card.Item[0].I.IO.dwBytes,
				cr->Card.Item[0].I.IO.dwBar);
				
				DPRINTF("Items: %lu, Addr: 0x%lx, bytes: %lu, bar: %lu\n",
				cr->Card.dwItems,
				(unsigned long)cr->Card.Item[1].I.IO.dwAddr,
				cr->Card.Item[1].I.IO.dwBytes,
				cr->Card.Item[1].I.IO.dwBar);
#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else

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
				DPRINTF("hCard: %lu\n", cr->hCard);
			}
			break;

		case USB_TRANSFER:
			DPRINTF("in USB_TRANSFER");
			{
				struct usb_transfer *ut = (struct usb_transfer*)(wdheader->data);

#ifdef DEBUG
				DPRINTF(" unique: %lu, pipe: %lu, read: %lu, options: %lx, size: %lu, timeout: %lx\n",
				ut->dwUniqueID, ut->dwPipeNum, ut->fRead,
				ut->dwOptions, ut->dwBufferSize, ut->dwTimeout);
				if (ut->dwPipeNum == 0) {
					DPRINTF("setup packet: ");
					hexdump(ut->SetupPacket, 8);
				}

				if (!ut->fRead && ut->dwBufferSize)
				{
					hexdump(ut->pBuffer, ut->dwBufferSize);
				}
#endif

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				xpcu_claim(xpcu, XPCU_CLAIM);
				/* http://www.jungo.com/support/documentation/windriver/802/wdusb_man_mhtml/node55.html#SECTION001213000000000000000 */
				if (ut->dwPipeNum == 0) { /* control pipe */
					int requesttype, request, value, index, size;
					requesttype = ut->SetupPacket[0];
					request = ut->SetupPacket[1];
					value = ut->SetupPacket[2] | (ut->SetupPacket[3] << 8);
					index = ut->SetupPacket[4] | (ut->SetupPacket[5] << 8);
					size = ut->SetupPacket[6] | (ut->SetupPacket[7] << 8);
					DPRINTF("requesttype: %x, request: %x, value: %u, index: %u, size: %u\n", requesttype, request, value, index, size);
					ret = usb_control_msg(xpcu->handle, requesttype, request, value, index, ut->pBuffer, size, ut->dwTimeout);
				} else {
					if (ut->fRead) {
						ret = usb_bulk_read(xpcu->handle, ut->dwPipeNum, ut->pBuffer, ut->dwBufferSize, ut->dwTimeout);
					} else {
						ret = usb_bulk_write(xpcu->handle, ut->dwPipeNum, ut->pBuffer, ut->dwBufferSize, ut->dwTimeout);
					}
					xpcu_claim(xpcu, XPCU_RELEASE);
				}

				if (ret < 0) {
					fprintf(stderr, "usb_transfer: %d (%s)\n", ret, usb_strerror());
				} else {
					ut->dwBytesTransferred = ret;
					ret = 0;
				}
#endif

#ifdef DEBUG
				DPRINTF("Transferred: %lu (%s)\n",ut->dwBytesTransferred, (ut->fRead?"read":"write"));
				if (ut->fRead && ut->dwBytesTransferred)
				{
					DPRINTF("Read: ");
					hexdump(ut->pBuffer, ut->dwBytesTransferred);
				}
#endif
			}
			break;

		case INT_ENABLE_OLD:
		case INT_ENABLE:
			DPRINTF("INT_ENABLE\n");
			{
				struct interrupt *it = (struct interrupt*)(wdheader->data);

				DPRINTF("Handle: %lu, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n",
				it->hInterrupt, it->dwOptions,
				it->dwCmds, it->fEnableOk, it->dwCounter,
				it->dwLost, it->fStopped);

				it->fEnableOk = 1;
				it->fStopped = 0;
				ints_enabled = 1;
				pthread_mutex_trylock(&int_wait);
			}

			break;
			
		case INT_DISABLE:
			DPRINTF("INT_DISABLE\n");
			{
				struct interrupt *it = (struct interrupt*)(wdheader->data);

				DPRINTF("Handle: %lu, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n",
				it->hInterrupt, it->dwOptions,
				it->dwCmds, it->fEnableOk, it->dwCounter,
				it->dwLost, it->fStopped);
#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				it->dwCounter = 0;
				it->fStopped = 1;
				ints_enabled = 0;
				if (pthread_mutex_trylock(&int_wait) == EBUSY)
					pthread_mutex_unlock(&int_wait);
#endif
				DPRINTF("Handle: %lu, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n",
				it->hInterrupt, it->dwOptions,
				it->dwCmds, it->fEnableOk, it->dwCounter,
				it->dwLost, it->fStopped);
			}
			break;

		case USB_SET_INTERFACE:
			DPRINTF("USB_SET_INTERFACE\n");
			{
				struct usb_set_interface *usi = (struct usb_set_interface*)(wdheader->data);

				DPRINTF("unique: %lu, interfacenum: %lu, alternatesetting: %lu, options: %lx\n",
				usi->dwUniqueID, usi->dwInterfaceNum,
				usi->dwAlternateSetting, usi->dwOptions);
#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				if (xpcu->dev) {
					if (!xpcu->handle) {
						xpcu->handle = usb_open(xpcu->dev);
#ifndef NO_USB_RESET
						if (xpcu->handle) {
							usb_reset(xpcu->handle);
							xpcu->handle = usb_open(xpcu->dev);
						}
#endif
					}

					xpcu->interface = xpcu->dev->config[0].interface[usi->dwInterfaceNum].altsetting[usi->dwAlternateSetting].bInterfaceNumber;
					xpcu->alternate = usi->dwAlternateSetting;
				}
#endif
				DPRINTF("unique: %lu, interfacenum: %lu, alternatesetting: %lu, options: %lx\n",
				usi->dwUniqueID, usi->dwInterfaceNum,
				usi->dwAlternateSetting, usi->dwOptions);
			}
			break;

		case USB_GET_DEVICE_DATA_OLD:
		case USB_GET_DEVICE_DATA:
			DPRINTF("USB_GET_DEVICE_DATA\n");
			{
				struct usb_get_device_data *ugdd = (struct usb_get_device_data*)(wdheader->data);
				int pSize;

				DPRINTF("unique: %lu, bytes: %lu, options: %lx\n",
				ugdd->dwUniqueID, ugdd->dwBytes,
				ugdd->dwOptions);

				pSize = ugdd->dwBytes;
				if (!ugdd->dwBytes) {
					if (xpcu->dev) {
						ugdd->dwBytes = xpcu_deviceinfo(xpcu, NULL);
					}
				} else {
					xpcu_deviceinfo(xpcu, (unsigned char*)ugdd->pBuf);
				}
			}
			break;

		case EVENT_REGISTER_OLD:
		case EVENT_REGISTER:
			DPRINTF("EVENT_REGISTER\n");
			{
				struct event *e = (struct event*)(wdheader->data);
				struct usb_bus *bus;
				char* devpos;
				int busnum = -1, devnum = -1;
				int i;

				DPRINTF("handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lu, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n",
				e->handle, e->dwAction,
				e->dwStatus, e->dwEventId, e->dwCardType,
				e->hKernelPlugIn, e->dwOptions,
				e->u.Usb.deviceId.dwVendorId,
				e->u.Usb.deviceId.dwProductId,
				e->u.Usb.dwUniqueID, e->dwEventVer,
				e->dwNumMatchTables);

				devpos = getenv("XILINX_USB_DEV");
				if (devpos != NULL) {
					int j;
					char *devstr = NULL, *remainder;

					DPRINTF("XILINX_USB_DEV=%s\n", devpos);

					for (j = 0; j < strlen(devpos) && devpos[j] != 0; j++) {
						if (devpos[j] == ':') {
							devpos[j] = 0;
							devstr = &(devpos[j+1]);
						}
					}

					if (devstr && strlen(devstr) > 0) {
						busnum = strtol(devpos, &remainder, 10);
						if (devpos == remainder) {
							busnum = -1;
						} else {
							devnum = strtol(devstr, &remainder, 10);
							if (devstr == remainder) {
								busnum = -1;
								devnum = -1;
							} else {
								fprintf(stderr,"Using XILINX platform cable USB at %03d:%03d\n",
								busnum, devnum);
							}
						}
					}
				}

				for (i = 0; i < e->dwNumMatchTables; i++) {

					DPRINTF("match: dev: %04x:%04x, class: %x, subclass: %x, intclass: %x, intsubclass: %x, intproto: %x\n",
					e->matchTables[i].VendorId,
					e->matchTables[i].ProductId,
					e->matchTables[i].bDeviceClass,
					e->matchTables[i].bDeviceSubClass,
					e->matchTables[i].bInterfaceClass,
					e->matchTables[i].bInterfaceSubClass,
					e->matchTables[i].bInterfaceProtocol);

					for (bus = xpcu->busses; bus; bus = bus->next) {
						struct usb_device *dev;

						if ((devnum != -1) && (strtol(bus->dirname, NULL, 10) != busnum))
							continue;

						for (dev = bus->devices; dev; dev = dev->next) {
							struct usb_device_descriptor *desc = &(dev->descriptor);

							if((desc->idVendor == e->matchTables[i].VendorId) &&
							   (desc->idProduct == e->matchTables[i].ProductId) &&
							   (desc->bDeviceClass == e->matchTables[i].bDeviceClass) &&
							   (desc->bDeviceSubClass == e->matchTables[i].bDeviceSubClass) &&
							   ((devnum == -1) || (strtol(dev->filename, NULL, 10) == devnum)) ) {
								   int ac;
								   for (ac = 0; ac < desc->bNumConfigurations; ac++) {
									   struct usb_interface *interface = dev->config[ac].interface;
									   int ai;

									   for (ai = 0; ai < interface->num_altsetting; ai++) {

										   DPRINTF("intclass: %x, intsubclass: %x, intproto: %x\n",
										   interface->altsetting[i].bInterfaceClass,
										   interface->altsetting[i].bInterfaceSubClass,
										   interface->altsetting[i].bInterfaceProtocol);

										   if ((interface->altsetting[ai].bInterfaceSubClass == e->matchTables[i].bInterfaceSubClass) &&
												   (interface->altsetting[ai].bInterfaceProtocol == e->matchTables[i].bInterfaceProtocol)){
											   /* TODO: check interfaceClass! */
											   DPRINTF("found device with libusb\n");
											   xpcu->dev = dev;
											   card_type = e->dwCardType;
										   }
									   }
								   }
							}
						}
					}
				}

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				e->handle++;
#endif

#ifdef DEBUG
				DPRINTF("handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lu, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n",
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
			DPRINTF("EVENT_UNREGISTER\n");
#ifndef NO_WINDRVR
			ret = (*ioctl_func) (fd, request, wdioctl);
#endif
			break;

		case INT_WAIT:
			DPRINTF("INT_WAIT\n");
			{
				struct interrupt *it = (struct interrupt*)(wdheader->data);

				DPRINTF("Handle: %lu, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n",
				it->hInterrupt, it->dwOptions,
				it->dwCmds, it->fEnableOk, it->dwCounter,
				it->dwLost, it->fStopped);

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				if (xpcu->dev) {
					if (it->dwCounter == 0) {
						it->dwCounter = 1;
					} else {
						pthread_mutex_lock(&int_wait);
						pthread_mutex_unlock(&int_wait);
					}
				} else {
					pthread_mutex_lock(&int_wait);
					pthread_mutex_unlock(&int_wait);
				}
#endif

				DPRINTF("INT_WAIT_RETURN: Handle: %lu, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n",
				it->hInterrupt, it->dwOptions, it->dwCmds,
				it->fEnableOk, it->dwCounter, it->dwLost,
				it->fStopped);
			}
			break;

		case CARD_UNREGISTER:
			DPRINTF("CARD_UNREGISTER\n");
			{
				struct card_register* cr = (struct card_register*)(wdheader->data);

				DPRINTF("Addr: 0x%lx, bytes: %lu, bar: %lu\n",
				(unsigned long)cr->Card.Item[0].I.IO.dwAddr,
				cr->Card.Item[0].I.IO.dwBytes,
				cr->Card.Item[0].I.IO.dwBar);

				DPRINTF("hCard: %lu\n", cr->hCard);

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

				DPRINTF("handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lx, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n",
				e->handle, e->dwAction, e->dwStatus,
				e->dwEventId, e->dwCardType, e->hKernelPlugIn,
				e->dwOptions, e->u.Usb.deviceId.dwVendorId,
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

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				if (xpcu->dev) {
					struct usb_interface *interface = xpcu->dev->config->interface;

					e->dwCardType = card_type;
					e->dwAction = 1;
					e->dwEventId = 109;
					e->u.Usb.dwUniqueID = 110;
					e->matchTables[0].VendorId = xpcu->dev->descriptor.idVendor;
					e->matchTables[0].ProductId = xpcu->dev->descriptor.idProduct;
					e->matchTables[0].bDeviceClass = xpcu->dev->descriptor.bDeviceClass;
					e->matchTables[0].bDeviceSubClass = xpcu->dev->descriptor.bDeviceSubClass;
					e->matchTables[0].bInterfaceClass = interface->altsetting[0].bInterfaceClass;
					e->matchTables[0].bInterfaceSubClass = interface->altsetting[0].bInterfaceSubClass;
					e->matchTables[0].bInterfaceProtocol = interface->altsetting[0].bInterfaceProtocol;
				}
#endif

#ifdef DEBUG
				DPRINTF("handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lx, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n",
				e->handle, e->dwAction, e->dwStatus,
				e->dwEventId, e->dwCardType, e->hKernelPlugIn,
				e->dwOptions, e->u.Usb.deviceId.dwVendorId,
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
	int ret;

	if (!ioctl_func)                                                                    
		ioctl_func = (int (*) (int, int, void *)) dlsym (RTLD_NEXT, "ioctl");             

	va_start (args, request);
	argp = va_arg (args, void *);
	va_end (args);

	if (fd == windrvrfd)
		ret = do_wdioctl(fd, request, argp);
	else
		ret = (*ioctl_func) (fd, request, argp);

	return ret;
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
		DPRINTF("opening windrvr6\n");
#ifdef NO_WINDRVR
		windrvrfd = fd = (*func) ("/dev/null", flags, mode);
#else
		windrvrfd = fd = (*func) (pathname, flags, mode);
#endif
		if (!xpcu)
			xpcu = xpcu_open();

		return fd;
	}

	return (*func) (pathname, flags, mode);
}

int close(int fd) {
	static int (*func) (int) = NULL;

	if (!func)
		func = (int (*) (int)) dlsym(RTLD_NEXT, "close");
	
	if (fd == windrvrfd && windrvrfd >= 0) {
		DPRINTF("close windrvrfd\n");

		if (xpcu->handle) {
			xpcu_claim(xpcu, XPCU_RELEASE);
			usb_close(xpcu->handle);
		}

		xpcu->handle = NULL;
		xpcu->interface = -1;
		xpcu = NULL;
		windrvrfd = -1;
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

#if __WORDSIZE == 32
int uname (struct utsname *__name) {
	static int (*func) (struct utsname*);
	int ret;

	if (!func)
		func = (int (*) (struct utsname*)) dlsym(RTLD_NEXT, "uname");
	
	ret = (*func)(__name);

	if (ret == 0 && (!strcmp(__name->machine, "x86_64"))) {
		strcpy(__name->machine, "i686");
	}
	
	return ret;
}
#endif
