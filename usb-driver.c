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
#include <signal.h>
#include "xilinx.h"

static int (*ioctl_func) (int, int, void *) = NULL;
static int windrvrfd = 0;
static struct usb_bus *busses = NULL;
static struct usb_device *usbdevice;
static usb_dev_handle *usb_devhandle = NULL;
static unsigned long card_type;

#define NO_WINDRVR 1

void hexdump(unsigned char *buf, int len);
void diff(unsigned char *buf1, unsigned char *buf2, int len);

//unique: 94, bytes: 276, options: 0
//Vendor: 3fd
//12 01 00 02 00 00 00 40 fd 03 08 00 00 00 01 02                              12 01 00 02 00 00 00 40 fd 03 08 00 00 00 01 02
//00 01 00 00 00 00 00 00 40 00 00 00 00 00 00 00                              00 01 00 00 00 00 00 00 40 00 00 00 00 00 00 00
//03 00 00 00 00 00 00 00 38 45 21 08 38 45 21 08                              03 00 00 00 00 00 00 00 38 45 21 08 38 45 21 08
//4c 45 21 08 00 00 00 00 00 00 00 00 00 00 00 00                              4c 45 21 08 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00                              00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
//00 00 00 00 00 00 00 00 09 02 20 00 01 02 00 80                              00 00 00 00 00 00 00 00 09 02 20 00 01 02 00 80
//8c 00 00 00 01 00 00 00 4c 45 21 08 58 45 21 08                              8c 00 00 00 01 00 00 00 4c 45 21 08 58 45 21 08
//01 00 00 00 58 45 21 08 09 04 00 00 02 ff 00 00                              01 00 00 00 58 45 21 08 09 04 00 00 02 ff 00 00
//00 00 00 00 6c 45 21 08 7c 45 21 08 07 05 02 02                              00 00 00 00 6c 45 21 08 7c 45 21 08 07 05 02 02
//00 02 00 00 07 05 86 02 00 02 00 00 02 00 00 00                              00 02 00 00 07 05 86 02 00 02 00 00 02 00 00 00
//00 02 00 00 02 00 00 00 02 00 00 00 00 00 00 00                              00 02 00 00 02 00 00 00 02 00 00 00 00 00 00 00
//86 00 00 00 00 02 00 00 02 00 00 00 01 00 00 00                              86 00 00 00 00 02 00 00 02 00 00 00 01 00 00 00
//00 00 00 00                                                                  00 00 00 00
int usb_deviceinfo(unsigned char *buf) {
	int i,j,k,l;
	int len = 0;
	WDU_CONFIGURATION **pConfigs, **pActiveConfig;
	WDU_INTERFACE **pActiveInterface;

	if (buf) {
		struct usb_device_info *udi = (struct usb_device_info*)(buf+len);

		udi->Descriptor.bLength = sizeof(WDU_DEVICE_DESCRIPTOR);
		udi->Descriptor.bDescriptorType = usbdevice->descriptor.bDescriptorType;
		udi->Descriptor.bcdUSB = usbdevice->descriptor.bcdUSB;
		udi->Descriptor.bDeviceClass = usbdevice->descriptor.bDeviceClass;
		udi->Descriptor.bDeviceSubClass = usbdevice->descriptor.bDeviceSubClass;
		udi->Descriptor.bDeviceProtocol = usbdevice->descriptor.bDeviceProtocol;
		udi->Descriptor.bMaxPacketSize0 = usbdevice->descriptor.bMaxPacketSize0;
		udi->Descriptor.idVendor = usbdevice->descriptor.idVendor;
		udi->Descriptor.idProduct = usbdevice->descriptor.idProduct;
		udi->Descriptor.bcdDevice = usbdevice->descriptor.bcdDevice;
		udi->Descriptor.iManufacturer = usbdevice->descriptor.iManufacturer;
		udi->Descriptor.iProduct = usbdevice->descriptor.iProduct;
		udi->Descriptor.iSerialNumber = usbdevice->descriptor.iSerialNumber;
		udi->Descriptor.bNumConfigurations = usbdevice->descriptor.bNumConfigurations;

		/* TODO: Fix Pipe0! */
		udi->Pipe0.dwNumber = 0x00;
		udi->Pipe0.dwMaximumPacketSize = usbdevice->descriptor.bMaxPacketSize0;
		udi->Pipe0.type = 0;
		udi->Pipe0.direction = 3;
		udi->Pipe0.dwInterval = 0;

		pConfigs = &(udi->pConfigs);
		pActiveConfig = &(udi->pActiveConfig);
		pActiveInterface = &(udi->pActiveInterface[0]);
	}

	len = sizeof(struct usb_device_info);

	for (i=0; i<usbdevice->descriptor.bNumConfigurations; i++)
	{
		struct usb_config_descriptor *conf_desc = &usbdevice->config[i];
		WDU_INTERFACE **pInterfaces;
		WDU_ALTERNATE_SETTING **pAlternateSettings[conf_desc->bNumInterfaces];
		WDU_ALTERNATE_SETTING **pActiveAltSetting[conf_desc->bNumInterfaces];

		if (buf) {
			WDU_CONFIGURATION *cfg = (WDU_CONFIGURATION*)(buf+len);

			*pConfigs = cfg;
			*pActiveConfig = cfg;

			cfg->Descriptor.bLength = conf_desc->bLength;
			cfg->Descriptor.bDescriptorType = conf_desc->bDescriptorType;
			cfg->Descriptor.wTotalLength = conf_desc->wTotalLength;
			cfg->Descriptor.bNumInterfaces = conf_desc->bNumInterfaces;
			cfg->Descriptor.bConfigurationValue = conf_desc->bConfigurationValue;
			cfg->Descriptor.iConfiguration = conf_desc->iConfiguration;
			cfg->Descriptor.bmAttributes = conf_desc->bmAttributes;
			cfg->Descriptor.MaxPower = conf_desc->MaxPower;

			cfg->dwNumInterfaces = conf_desc->bNumInterfaces;

			pInterfaces = &(cfg->pInterfaces);
		}
		len += sizeof(WDU_CONFIGURATION);

		if (buf) {
			*pInterfaces = (WDU_INTERFACE*)(buf+len);
			for (j=0; j<conf_desc->bNumInterfaces; j++) {
				WDU_INTERFACE *iface = (WDU_INTERFACE*)(buf+len);

				pActiveInterface[j] = iface;

				pAlternateSettings[j] = &(iface->pAlternateSettings);
				iface->dwNumAltSettings = usbdevice->config[i].interface[j].num_altsetting;
				pActiveAltSetting[j] = &(iface->pActiveAltSetting);

				len += sizeof(WDU_INTERFACE);
			}
		} else {
			len += sizeof(WDU_INTERFACE) * conf_desc->bNumInterfaces;
		}

		for (j=0; j<conf_desc->bNumInterfaces; j++)
		{
			struct usb_interface *interface = &usbdevice->config[i].interface[j];

			if (buf) {
				*pAlternateSettings[j] = (WDU_ALTERNATE_SETTING*)(buf+len);
				/* FIXME: */
				*pActiveAltSetting[j] = (WDU_ALTERNATE_SETTING*)(buf+len);
			}

			for(k=0; k<interface->num_altsetting; k++)
			{
				unsigned char bNumEndpoints = interface->altsetting[k].bNumEndpoints;
				WDU_ENDPOINT_DESCRIPTOR **pEndpointDescriptors;
				WDU_PIPE_INFO **pPipes;

				if (buf) {
					WDU_ALTERNATE_SETTING *altset = (WDU_ALTERNATE_SETTING*)(buf+len);

					altset->Descriptor.bLength = interface->altsetting[k].bLength;
					altset->Descriptor.bDescriptorType = interface->altsetting[k].bDescriptorType;
					altset->Descriptor.bInterfaceNumber = interface->altsetting[k].bInterfaceNumber;
					altset->Descriptor.bAlternateSetting = interface->altsetting[k].bAlternateSetting;
					altset->Descriptor.bNumEndpoints = interface->altsetting[k].bNumEndpoints;
					altset->Descriptor.bInterfaceClass = interface->altsetting[k].bInterfaceClass;
					altset->Descriptor.bInterfaceSubClass = interface->altsetting[k].bInterfaceSubClass;
					altset->Descriptor.bInterfaceProtocol = interface->altsetting[k].bInterfaceProtocol;
					altset->Descriptor.iInterface = interface->altsetting[k].iInterface;
					pEndpointDescriptors = &(altset->pEndpointDescriptors);
					pPipes = &(altset->pPipes);

				}
				len +=sizeof(WDU_ALTERNATE_SETTING);

				if (buf) {
					*pEndpointDescriptors = (WDU_ENDPOINT_DESCRIPTOR*)(buf+len);
					for (l = 0; l < bNumEndpoints; l++) {
						WDU_ENDPOINT_DESCRIPTOR *ed = (WDU_ENDPOINT_DESCRIPTOR*)(buf+len);

						ed->bLength = interface->altsetting[k].endpoint[l].bLength;
						ed->bDescriptorType = interface->altsetting[k].endpoint[l].bDescriptorType;
						ed->bEndpointAddress = interface->altsetting[k].endpoint[l].bEndpointAddress;
						ed->bmAttributes = interface->altsetting[k].endpoint[l].bmAttributes;
						ed->wMaxPacketSize = interface->altsetting[k].endpoint[l].wMaxPacketSize;
						ed->bInterval = interface->altsetting[k].endpoint[l].bInterval;

						len += sizeof(WDU_ENDPOINT_DESCRIPTOR);
					}
						
					*pPipes = (WDU_PIPE_INFO*)(buf+len);
					for (l = 0; l < bNumEndpoints; l++) {
						WDU_PIPE_INFO *pi = (WDU_PIPE_INFO*)(buf+len);

						pi->dwNumber = interface->altsetting[k].endpoint[l].bEndpointAddress;
						pi->dwMaximumPacketSize = WDU_GET_MAX_PACKET_SIZE(interface->altsetting[k].endpoint[l].wMaxPacketSize);
						pi->type = interface->altsetting[k].endpoint[l].bmAttributes & USB_ENDPOINT_TYPE_MASK;
						if (pi->type == PIPE_TYPE_CONTROL)
							pi->direction = WDU_DIR_IN_OUT;
						else
						{
							pi->direction = interface->altsetting[k].endpoint[l].bEndpointAddress & USB_ENDPOINT_DIR_MASK ?  WDU_DIR_IN : WDU_DIR_OUT;
						}

						pi->dwInterval = interface->altsetting[k].endpoint[l].bInterval;

						len += sizeof(WDU_PIPE_INFO);
					}
				} else {
					len +=(sizeof(WDU_ENDPOINT_DESCRIPTOR)+sizeof(WDU_PIPE_INFO))*bNumEndpoints;
				}
			}
		}
	}

	return len;
}

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
#else
				if (usbdevice) {
					int iface;

					if (!usb_devhandle)
						usb_devhandle = usb_open(usbdevice);
//MGMG
					ret = usb_claim_interface(usb_devhandle, iface);
				}
#endif
				fprintf(stderr,"unique: %lu, interfacenum: %lu, alternatesetting: %lu, options: %lx\n", usi->dwUniqueID, usi->dwInterfaceNum, usi->dwAlternateSetting, usi->dwOptions);
			}
			break;

		case USB_GET_DEVICE_DATA:
			fprintf(stderr,"USB_GET_DEVICE_DATA\n");
			{
				struct usb_get_device_data *ugdd = (struct usb_get_device_data*)(wdheader->data);
				int pSize;

				fprintf(stderr, "unique: %lu, bytes: %lu, options: %lx\n", ugdd->dwUniqueID, ugdd->dwBytes, ugdd->dwOptions);
				pSize = ugdd->dwBytes;
#ifndef NO_WINDRVR
				ret = (*ioctl_func) (fd, request, wdioctl);
#else
				if (!ugdd->dwBytes) {
					if (usbdevice) {
						ugdd->dwBytes = usb_deviceinfo(NULL);
					}
				} else {
					usb_deviceinfo((unsigned char*)ugdd->pBuf);
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
								   int ac;
								   for (ac = 0; ac < desc->bNumConfigurations; ac++) {
									   struct usb_interface *interface = dev->config[ac].interface;
									   int ai;

									   for (ai = 0; ai < interface->num_altsetting; ai++) {
										   fprintf(stderr, "intclass: %x, intsubclass: %x, intproto: %x\n", interface->altsetting[i].bInterfaceClass, interface->altsetting[i].bInterfaceSubClass, interface->altsetting[i].bInterfaceProtocol);
										   if ((interface->altsetting[ai].bInterfaceSubClass == e->matchTables[i].bInterfaceSubClass) &&
												   (interface->altsetting[ai].bInterfaceProtocol == e->matchTables[i].bInterfaceProtocol)){
											   /* TODO: check interfaceClass! */
											   fprintf(stderr,"!!!FOUND DEVICE WITH LIBUSB!!!\n");
											   usbdevice = dev;
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
//handle: 0, action: 16371, status: 0, eventid: 0, cardtype: 4294967294, kplug: 0, options: 0, dev: 0:0, unique: 0, ver: 1, nummatch: 2
//handle: 1, action: 16371, status: 0, eventid: 0, cardtype: 4294967294, kplug: 0, options: 0, dev: 0:0, unique: 0, ver: 1, nummatch: 2
				e->handle++;
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
				if (usbdevice) {
					if (it->dwCounter == 0) {
						it->dwCounter = 1;
					} else {
						//FIXME: signal durch FUTEX, overload futex!
						kill(getpid(), SIGHUP);
					}
				} else {
					kill(getpid(), SIGHUP);
				}
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

				fprintf(stderr,"handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lx, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n", e->handle, e->dwAction, e->dwStatus, e->dwEventId, e->dwCardType, e->hKernelPlugIn, e->dwOptions, e->u.Usb.deviceId.dwVendorId, e->u.Usb.deviceId.dwProductId, e->u.Usb.dwUniqueID, e->dwEventVer, e->dwNumMatchTables);
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
				if (usbdevice) {
					struct usb_interface *interface = usbdevice->config->interface;

					e->dwCardType = card_type;
					e->dwAction = 1;
					e->dwEventId = 109;
					e->u.Usb.dwUniqueID = 4711;
					e->matchTables[0].VendorId = usbdevice->descriptor.idVendor;
					e->matchTables[0].ProductId = usbdevice->descriptor.idProduct;
					e->matchTables[0].bDeviceClass = usbdevice->descriptor.bDeviceClass;
					e->matchTables[0].bDeviceSubClass = usbdevice->descriptor.bDeviceSubClass;
					e->matchTables[0].bInterfaceClass = interface->altsetting[0].bInterfaceClass;
					e->matchTables[0].bInterfaceSubClass = interface->altsetting[0].bInterfaceSubClass;
					e->matchTables[0].bInterfaceProtocol = interface->altsetting[0].bInterfaceProtocol;
				}
#endif

				fprintf(stderr,"handle: %lu, action: %lu, status: %lu, eventid: %lu, cardtype: %lx, kplug: %lu, options: %lu, dev: %lx:%lx, unique: %lu, ver: %lu, nummatch: %lu\n", e->handle, e->dwAction, e->dwStatus, e->dwEventId, e->dwCardType, e->hKernelPlugIn, e->dwOptions, e->u.Usb.deviceId.dwVendorId, e->u.Usb.deviceId.dwProductId, e->u.Usb.dwUniqueID, e->dwEventVer, e->dwNumMatchTables);
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
