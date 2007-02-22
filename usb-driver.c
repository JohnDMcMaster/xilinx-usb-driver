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
static struct usb_device *usb_cable;
static unsigned long card_type;

#define NO_WINDRVR 1

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

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#endif

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
#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				it->dwCounter = 0;
				it->fStopped = 1;
#endif
				fprintf(stderr,"Handle: %lu, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n", it->hInterrupt, it->dwOptions, it->dwCmds, it->fEnableOk, it->dwCounter, it->dwLost, it->fStopped);
			}
			break;

		case USB_SET_INTERFACE:
			fprintf(stderr,"USB_SET_INTERFACE\n");
			{
				struct usb_set_interface *usi = (struct usb_set_interface*)(wdheader->data);

				fprintf(stderr,"unique: %lu, interfacenum: %lu, alternatesetting: %lu, options: %lx\n", usi->dwUniqueID, usi->dwInterfaceNum, usi->dwAlternateSetting, usi->dwOptions);
#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#endif
			}
			break;

		case USB_GET_DEVICE_DATA:
			fprintf(stderr,"USB_GET_DEVICE_DATA\n");
			{
				struct usb_get_device_data *ugdd = (struct usb_get_device_data*)(wdheader->data);
				int pSize;

				fprintf(stderr, "unique: %lu, bytes: %lu, options: %lx\n", ugdd->dwUniqueID, ugdd->dwBytes, ugdd->dwOptions);
				pSize = ugdd->dwBytes;
				if (pSize) {
					hexdump(ugdd->pBuf, pSize);
				}
#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				if (!ugdd->dwBytes) {
					if (usb_cable) {
						int i,j,k;
						ugdd->dwBytes = sizeof(struct usb_device_info);

						for (i=0; i<usb_cable->descriptor.bNumConfigurations; i++)
						{
							struct usb_config_descriptor *conf_desc = &usb_cable->config[i];
							ugdd->dwBytes+=sizeof(WDU_CONFIGURATION);
							ugdd->dwBytes+=sizeof(WDU_INTERFACE) * conf_desc->bNumInterfaces;

							for (j=0; j<conf_desc->bNumInterfaces; j++)
							{
								struct usb_interface *interface = &usb_cable->config[i].interface[j];
								for(k=0; k<interface->num_altsetting; k++)
								{
									unsigned char bNumEndpoints;
									bNumEndpoints = interface->altsetting[k].bNumEndpoints;
									ugdd->dwBytes+=sizeof(WDU_ALTERNATE_SETTING);
									ugdd->dwBytes+=(sizeof(WDU_ENDPOINT_DESCRIPTOR)+sizeof(WDU_PIPE_INFO))*bNumEndpoints;
								}
							}
						}

						
					}
				} else {
					struct usb_device_info_get *udi = (struct usb_device_info_get*)ugdd->pBuf;
					struct usb_endpoint_descriptor *ep;
unsigned char dings[] = {0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0xfd, 0x03, 0x08, 0x00, 0x00, 0x00, 0x01, 0x02,
0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x45, 0x21, 0x08, 0x38, 0x45, 0x21, 0x08,
0x4c, 0x45, 0x21, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x02, 0x20, 0x00, 0x01, 0x02, 0x00, 0x80,
0x8c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x4c, 0x45, 0x21, 0x08, 0x58, 0x45, 0x21, 0x08,
0x01, 0x00, 0x00, 0x00, 0x58, 0x45, 0x21, 0x08, 0x09, 0x04, 0x00, 0x00, 0x02, 0xff, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x6c, 0x45, 0x21, 0x08, 0x7c, 0x45, 0x21, 0x08, 0x07, 0x05, 0x02, 0x02,
0x00, 0x02, 0x00, 0x00, 0x07, 0x05, 0x86, 0x02, 0x00, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
0x00, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x86, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00};

//unique: 94, bytes: 276, options: 0
//Vendor: 3fd
//12 01 00 02 00 00 00 40 fd 03 08 00 00 00 01 02                              12 01 00 02 00 00 00 40 fd 03 08 00 00 00 01 02
//00 01 00 00 00 00 00 00 40 00 00 00 00 00 00 00                              00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//03 00 00 00 00 00 00 00 38 45 21 08 38 45 21 08                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//4c 45 21 08 00 00 00 00 00 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 09 02 20 00 01 02 00 80                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//8c 00 00 00 01 00 00 00 4c 45 21 08 58 45 21 08                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//01 00 00 00 58 45 21 08 09 04 00 00 02 ff 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 6c 45 21 08 7c 45 21 08 07 05 02 02                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 02 00 00 07 05 86 02 00 02 00 00 02 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 02 00 00 02 00 00 00 02 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//86 00 00 00 00 02 00 00 02 00 00 00 01 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00                                                                  00 00 00 00

					bzero(udi, ugdd->dwBytes);
					//memcpy(udi, dings, 276);
					udi->Descriptor.bLength = sizeof(WDU_DEVICE_DESCRIPTOR);
					udi->Descriptor.bDescriptorType = usb_cable->descriptor.bDescriptorType;
					udi->Descriptor.bcdUSB = usb_cable->descriptor.bcdUSB;
					udi->Descriptor.bDeviceClass = usb_cable->descriptor.bDeviceClass;
					udi->Descriptor.bDeviceSubClass = usb_cable->descriptor.bDeviceSubClass;
					udi->Descriptor.bDeviceProtocol = usb_cable->descriptor.bDeviceProtocol;
					udi->Descriptor.bMaxPacketSize0 = usb_cable->descriptor.bMaxPacketSize0;
					udi->Descriptor.idVendor = usb_cable->descriptor.idVendor;
					udi->Descriptor.idProduct = usb_cable->descriptor.idProduct;
					udi->Descriptor.bcdDevice = usb_cable->descriptor.bcdDevice;
					udi->Descriptor.iManufacturer = usb_cable->descriptor.iManufacturer;
					udi->Descriptor.iProduct = usb_cable->descriptor.iProduct;
					udi->Descriptor.iSerialNumber = usb_cable->descriptor.iSerialNumber;
					udi->Descriptor.bNumConfigurations = usb_cable->descriptor.bNumConfigurations;

					ep = usb_cable->config->interface->altsetting[0].endpoint;

					udi->Pipe0.dwNumber = 0x00;
					udi->Pipe0.dwMaximumPacketSize = usb_cable->descriptor.bMaxPacketSize0;
					udi->Pipe0.type = 0;
					udi->Pipe0.direction = 3;
					udi->Pipe0.dwInterval = 0;

					udi->cfg.Descriptor.bLength = usb_cable->config->bLength;
					udi->cfg.Descriptor.bDescriptorType = usb_cable->config->bDescriptorType;
					udi->cfg.Descriptor.wTotalLength = usb_cable->config->wTotalLength;
					udi->cfg.Descriptor.bNumInterfaces = usb_cable->config->bNumInterfaces;
					udi->cfg.Descriptor.bConfigurationValue = usb_cable->config->bConfigurationValue;
					udi->cfg.Descriptor.iConfiguration = usb_cable->config->iConfiguration;
					udi->cfg.Descriptor.bmAttributes = usb_cable->config->bmAttributes;
					udi->cfg.Descriptor.MaxPower = usb_cable->config->MaxPower;

					// ab offset 168 config desc
				}
#endif
				if (pSize) {
					struct usb_device_info *udi = (struct usb_device_info*)ugdd->pBuf;

					fprintf(stderr, "Vendor: %x\n", udi->Descriptor.idVendor);

					hexdump(ugdd->pBuf, pSize);
					fprintf(stderr, "\n");
				}
			}
			break;

		case EVENT_REGISTER:
			fprintf(stderr,"EVENT_REGISTER\n");
			{
				struct event *e = (struct event*)(wdheader->data);
				struct usb_bus *bus;
				int i;

				fprintf(stderr,"handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lu, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n", e->handle, e->dwAction, e->dwStatus, e->dwEventId, e->dwCardType, e->hKernelPlugIn, e->dwOptions, e->u.Usb.deviceId.dwVendorId, e->u.Usb.deviceId.dwProductId, e->u.Usb.dwUniqueID, e->dwEventVer, e->dwNumMatchTables);
				for (i = 0; i < e->dwNumMatchTables; i++) {
					fprintf(stderr,"match: dev: %04x:%04x, class: %x, subclass: %x, intclass: %x, intsubclass: %x, intproto: %x\n", e->matchTables[i].VendorId, e->matchTables[i].ProductId, e->matchTables[i].bDeviceClass, e->matchTables[i].bDeviceSubClass, e->matchTables[i].bInterfaceClass, e->matchTables[i].bInterfaceSubClass, e->matchTables[i].bInterfaceProtocol);

					for (bus = busses; bus; bus = bus->next) {
						struct usb_device *dev;

						for (dev = bus->devices; dev; dev = dev->next) {
							struct usb_device_descriptor *desc = &(dev->descriptor);

							if((desc->idVendor == e->matchTables[i].VendorId) &&
							   (desc->idProduct == e->matchTables[i].ProductId) &&
							   (desc->bDeviceClass == e->matchTables[i].bDeviceClass) &&
							   (desc->bDeviceSubClass == e->matchTables[i].bDeviceSubClass)) {
							   	struct usb_interface *interface = dev->config->interface;
								int ai;
								
								for (ai = 0; ai < interface->num_altsetting; ai++) {
									fprintf(stderr, "intclass: %x, intsubclass: %x, intproto: %x\n", interface->altsetting[i].bInterfaceClass, interface->altsetting[i].bInterfaceSubClass, interface->altsetting[i].bInterfaceProtocol);
									if ((interface->altsetting[i].bInterfaceSubClass == e->matchTables[i].bInterfaceSubClass) &&
									    (interface->altsetting[i].bInterfaceProtocol == e->matchTables[i].bInterfaceProtocol)){
										/* TODO: check interfaceClass! */
										fprintf(stderr,"!!!FOUND DEVICE WITH LIBUSB!!!\n");
										usb_cable = dev;
										card_type = e->dwCardType;
									}
								}
							}
						}
					}
				}

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#endif

				fprintf(stderr,"handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lu, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n", e->handle, e->dwAction, e->dwStatus, e->dwEventId, e->dwCardType, e->hKernelPlugIn, e->dwOptions, e->u.Usb.deviceId.dwVendorId, e->u.Usb.deviceId.dwProductId, e->u.Usb.dwUniqueID, e->dwEventVer, e->dwNumMatchTables);
				for (i = 0; i < e->dwNumMatchTables; i++)
					fprintf(stderr,"match: dev: %04x:%04x, class: %x, subclass: %x, intclass: %x, intsubclass: %x, intproto: %x\n", e->matchTables[i].VendorId, e->matchTables[i].ProductId, e->matchTables[i].bDeviceClass, e->matchTables[i].bDeviceSubClass, e->matchTables[i].bInterfaceClass, e->matchTables[i].bInterfaceSubClass, e->matchTables[i].bInterfaceProtocol);
			}
			break;

		case TRANSFER:
			fprintf(stderr,"TRANSFER\n");
#ifndef NO_WINDRVR
			ret = (*ioctl_func) (fd, request, wdioctl);
#endif
			break;

		case EVENT_UNREGISTER:
			fprintf(stderr,"EVENT_UNREGISTER\n");
#ifndef NO_WINDRVR
			ret = (*ioctl_func) (fd, request, wdioctl);
#endif
			break;

		case INT_WAIT:
			fprintf(stderr,"INT_WAIT\n");
			{
				struct interrupt *it = (struct interrupt*)(wdheader->data);

				fprintf(stderr,"Handle: %lu, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n", it->hInterrupt, it->dwOptions, it->dwCmds, it->fEnableOk, it->dwCounter, it->dwLost, it->fStopped);

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				if (usb_cable)
					it->dwCounter++;
#endif

				fprintf(stderr,"Handle: %lu, Options: %lx, ncmds: %lu, enableok: %lu, count: %lu, lost: %lu, stopped: %lu\n", it->hInterrupt, it->dwOptions, it->dwCmds, it->fEnableOk, it->dwCounter, it->dwLost, it->fStopped);
			}
			break;

		case CARD_UNREGISTER:
			fprintf(stderr,"CARD_UNREGISTER\n");
#ifndef NO_WINDRVR
			ret = (*ioctl_func) (fd, request, wdioctl);
#endif
			break;

		case EVENT_PULL:
			fprintf(stderr,"EVENT_PULL\n");
			{
				struct event *e = (struct event*)(wdheader->data);
				int i;

				fprintf(stderr,"handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lu, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n", e->handle, e->dwAction, e->dwStatus, e->dwEventId, e->dwCardType, e->hKernelPlugIn, e->dwOptions, e->u.Usb.deviceId.dwVendorId, e->u.Usb.deviceId.dwProductId, e->u.Usb.dwUniqueID, e->dwEventVer, e->dwNumMatchTables);
				for (i = 0; i < e->dwNumMatchTables; i++)
					fprintf(stderr,"match: dev: %04x:%04x, class: %x, subclass: %x, intclass: %x, intsubclass: %x, intproto: %x\n", e->matchTables[i].VendorId, e->matchTables[i].ProductId, e->matchTables[i].bDeviceClass, e->matchTables[i].bDeviceSubClass, e->matchTables[i].bInterfaceClass, e->matchTables[i].bInterfaceSubClass, e->matchTables[i].bInterfaceProtocol);

#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
//EVENT_PULL
//handle: 1, action: 0, status: 0, eventid: 0, cardtype: 0, kplug: 0, options: 0, dev: 0:0, unique: 0, ver: 1, nummatch: 1
//match: dev: 0:0, class: 0, subclass: 0, intclass: 0, intsubclass: 0, intproto: 0
//handle: 1, action: 1, status: 0, eventid: 109, cardtype: 4294967294, kplug: 0, options: 0, dev: 0:0, unique: 90, ver: 1, nummatch: 1
//match: dev: 3fd:8, class: 0, subclass: 0, intclass: ff, intsubclass: 0, intproto: 0
				if (usb_cable) {
					struct usb_interface *interface = usb_cable->config->interface;

					e->dwCardType = card_type;
					e->dwAction = 1;
					e->dwEventId = 109;
					e->u.Usb.dwUniqueID = 4711;
					e->matchTables[0].VendorId = usb_cable->descriptor.idVendor;
					e->matchTables[0].ProductId = usb_cable->descriptor.idProduct;
					e->matchTables[0].bDeviceClass = usb_cable->descriptor.bDeviceClass;
					e->matchTables[0].bDeviceSubClass = usb_cable->descriptor.bDeviceSubClass;
					e->matchTables[0].bInterfaceClass = interface->altsetting[0].bInterfaceClass;
					e->matchTables[0].bInterfaceSubClass = interface->altsetting[0].bInterfaceSubClass;
					e->matchTables[0].bInterfaceProtocol = interface->altsetting[0].bInterfaceProtocol;
				}
#endif

				fprintf(stderr,"handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lu, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n", e->handle, e->dwAction, e->dwStatus, e->dwEventId, e->dwCardType, e->hKernelPlugIn, e->dwOptions, e->u.Usb.deviceId.dwVendorId, e->u.Usb.deviceId.dwProductId, e->u.Usb.dwUniqueID, e->dwEventVer, e->dwNumMatchTables);
				for (i = 0; i < e->dwNumMatchTables; i++)
					fprintf(stderr,"match: dev: %04x:%04x, class: %x, subclass: %x, intclass: %x, intsubclass: %x, intproto: %x\n", e->matchTables[i].VendorId, e->matchTables[i].ProductId, e->matchTables[i].bDeviceClass, e->matchTables[i].bDeviceSubClass, e->matchTables[i].bInterfaceClass, e->matchTables[i].bInterfaceSubClass, e->matchTables[i].bInterfaceProtocol);
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

	if (!strcmp (pathname, "/dev/windrvr6")) {
		fprintf(stderr,"opening windrvr6\n");
#ifdef NO_WINDRVR
		windrvrfd = fd = (*func) ("/dev/null", flags, mode);
#else
		windrvrfd = fd = (*func) (pathname, flags, mode);
#endif
		if (!busses) {
			usb_init();
			usb_find_busses();
			usb_find_devices();

			busses = usb_get_busses();
		}
	} else {
		fd = (*func) (pathname, flags, mode);
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
