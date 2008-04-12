#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <usb.h>
#include <errno.h>
#include <pthread.h>
#include "usb-driver.h"
#include "xpcu.h"

struct xpcu_s {
	struct usb_device *dev;
	usb_dev_handle *handle;
	int interface;
	int alternate;
	unsigned long card_type;
	pthread_mutex_t interrupt;
};

static struct usb_bus *busses = NULL;
static pthread_mutex_t dummy_interrupt = PTHREAD_MUTEX_INITIALIZER;

int xpcu_deviceinfo(struct usb_get_device_data *ugdd) {
	struct xpcu_s *xpcu = (struct xpcu_s*)ugdd->dwUniqueID;
	int i,j,k,l;
	int len = 0;
	unsigned char *buf = NULL;
	WDU_CONFIGURATION **pConfigs, **pActiveConfig;
	WDU_INTERFACE **pActiveInterface;

	if (!xpcu)
		return -ENODEV;

	if (ugdd->dwBytes)
		buf = ugdd->pBuf;

	if (buf) {
		struct usb_device_info *udi = (struct usb_device_info*)(buf+len);

		udi->Descriptor.bLength = sizeof(WDU_DEVICE_DESCRIPTOR);
		udi->Descriptor.bDescriptorType = xpcu->dev->descriptor.bDescriptorType;
		udi->Descriptor.bcdUSB = xpcu->dev->descriptor.bcdUSB;
		udi->Descriptor.bDeviceClass = xpcu->dev->descriptor.bDeviceClass;
		udi->Descriptor.bDeviceSubClass = xpcu->dev->descriptor.bDeviceSubClass;
		udi->Descriptor.bDeviceProtocol = xpcu->dev->descriptor.bDeviceProtocol;
		udi->Descriptor.bMaxPacketSize0 = xpcu->dev->descriptor.bMaxPacketSize0;
		udi->Descriptor.idVendor = xpcu->dev->descriptor.idVendor;
		udi->Descriptor.idProduct = xpcu->dev->descriptor.idProduct;
		udi->Descriptor.bcdDevice = xpcu->dev->descriptor.bcdDevice;
		udi->Descriptor.iManufacturer = xpcu->dev->descriptor.iManufacturer;
		udi->Descriptor.iProduct = xpcu->dev->descriptor.iProduct;
		udi->Descriptor.iSerialNumber = xpcu->dev->descriptor.iSerialNumber;
		udi->Descriptor.bNumConfigurations = xpcu->dev->descriptor.bNumConfigurations;

		/* TODO: Fix Pipe0! */
		udi->Pipe0.dwNumber = 0x00;
		udi->Pipe0.dwMaximumPacketSize = xpcu->dev->descriptor.bMaxPacketSize0;
		udi->Pipe0.type = 0;
		udi->Pipe0.direction = WDU_DIR_IN_OUT;
		udi->Pipe0.dwInterval = 0;

		pConfigs = &(udi->pConfigs);
		pActiveConfig = &(udi->pActiveConfig);
		pActiveInterface = &(udi->pActiveInterface[0]);
	}

	len = sizeof(struct usb_device_info);

	for (i=0; i<xpcu->dev->descriptor.bNumConfigurations; i++)
	{
		struct usb_config_descriptor *conf_desc = &xpcu->dev->config[i];
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
				iface->dwNumAltSettings = xpcu->dev->config[i].interface[j].num_altsetting;
				pActiveAltSetting[j] = &(iface->pActiveAltSetting);

				len += sizeof(WDU_INTERFACE);
			}
		} else {
			len += sizeof(WDU_INTERFACE) * conf_desc->bNumInterfaces;
		}

		for (j=0; j<conf_desc->bNumInterfaces; j++)
		{
			struct usb_interface *interface = &xpcu->dev->config[i].interface[j];

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

	ugdd->dwBytes = len;

	return 0;
}

static int xpcu_claim(struct xpcu_s *xpcu, int claim) {
	int ret = 0;
	static int claimed = 0;

	if (xpcu->interface < 0)
		return -1;
	
	if (claim == XPCU_CLAIM) {
		if (claimed)
			return 0;

		ret = usb_claim_interface(xpcu->handle, xpcu->interface);
		if (!ret) {
			claimed = 1;
			ret = usb_set_altinterface(xpcu->handle, xpcu->alternate);
			if (ret)
				fprintf(stderr, "usb_set_altinterface: %d\n", ret);
		} else {
			fprintf(stderr, "usb_claim_interface: %d -> %d (%s)\n",
					xpcu->interface, ret, usb_strerror());
		}
	} else {
		if (!claimed)
			return 0;

		ret = usb_release_interface(xpcu->handle, xpcu->interface);
		if (!ret)
			claimed = 0;
	}

	return ret;
}

int xpcu_transfer(struct usb_transfer *ut) {
	struct xpcu_s *xpcu = (struct xpcu_s*)ut->dwUniqueID;
	int ret = 0;

	if (!xpcu)
		return -ENODEV;

	xpcu_claim(xpcu, XPCU_CLAIM);
	/* http://www.jungo.com/support/documentation/windriver/802/wdusb_man_mhtml/node55.html#SECTION001213000000000000000 */
	if (ut->dwPipeNum == 0) { /* control pipe */
		int requesttype, request, value, index, size;
		requesttype = ut->SetupPacket[0];
		request = ut->SetupPacket[1];
		value = ut->SetupPacket[2] | (ut->SetupPacket[3] << 8);
		index = ut->SetupPacket[4] | (ut->SetupPacket[5] << 8);
		size = ut->SetupPacket[6] | (ut->SetupPacket[7] << 8);
		DPRINTF("-> requesttype: %x, request: %x, value: %u, index: %u, size: %u\n", requesttype, request, value, index, size);
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

	return ret;
}

int xpcu_set_interface(struct usb_set_interface *usi) {
	struct xpcu_s *xpcu = (struct xpcu_s*)usi->dwUniqueID;

	if (!xpcu)
		return -ENODEV;

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

	return 0;
}

static void xpcu_init(void) {
	if (busses)
		return;

	usb_init();
	usb_find_busses();
	usb_find_devices();

	busses = usb_get_busses();
}


int xpcu_find(struct event *e) {
	struct xpcu_s *xpcu = NULL;
	char* devpos;
	struct usb_bus *bus;
	int busnum = -1, devnum = -1;
	int i;

	xpcu_init();

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

		for (bus = busses; bus; bus = bus->next) {
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

								xpcu = malloc(sizeof(struct xpcu_s));
								if (!xpcu)
									return -ENOMEM;

								bzero(xpcu, sizeof(struct xpcu_s));
								xpcu->interface = -1;
								xpcu->alternate = -1;
								xpcu->dev = dev;
								xpcu->card_type = e->dwCardType;
								pthread_mutex_init(&xpcu->interrupt, NULL);
								e->handle = (unsigned long)&xpcu;
							}
						}
					}
				}
			}
		}
	}

	e->handle = (unsigned long)xpcu;

	return 0;
}

int xpcu_found(struct event *e) {
	struct xpcu_s *xpcu = (struct xpcu_s*)e->handle;

	if (xpcu && xpcu->dev) {
		struct usb_interface *interface = xpcu->dev->config->interface;

		e->dwCardType = xpcu->card_type;
		e->dwAction = 1;
		e->dwEventId = 1;
		e->u.Usb.dwUniqueID = e->handle;
		e->matchTables[0].VendorId = xpcu->dev->descriptor.idVendor;
		e->matchTables[0].ProductId = xpcu->dev->descriptor.idProduct;
		e->matchTables[0].bDeviceClass = xpcu->dev->descriptor.bDeviceClass;
		e->matchTables[0].bDeviceSubClass = xpcu->dev->descriptor.bDeviceSubClass;
		e->matchTables[0].bInterfaceClass = interface->altsetting[0].bInterfaceClass;
		e->matchTables[0].bInterfaceSubClass = interface->altsetting[0].bInterfaceSubClass;
		e->matchTables[0].bInterfaceProtocol = interface->altsetting[0].bInterfaceProtocol;
	}

	return 0;
}

int xpcu_close(struct event *e) {
	struct xpcu_s *xpcu = (struct xpcu_s*)e->handle;

	if (!xpcu)
		return -ENODEV;

	if(xpcu) {
		if (xpcu->handle) {
			xpcu_claim(xpcu, XPCU_RELEASE);
			usb_close(xpcu->handle);
		}

		busses = NULL;
		free(xpcu);
	}

	return 0;
}

int xpcu_int_state(struct interrupt *it, int enable) {
	struct xpcu_s *xpcu = (struct xpcu_s*)it->hInterrupt;
	pthread_mutex_t *interrupt = &dummy_interrupt;

	if (xpcu)
		interrupt = &xpcu->interrupt;
	
	if (enable == ENABLE_INTERRUPT) {
		it->fEnableOk = 1;
		it->fStopped = 0;
		pthread_mutex_trylock(interrupt);
	} else {
		it->dwCounter = 0;
		it->fStopped = 1;
		if (pthread_mutex_trylock(interrupt) == EBUSY)
			pthread_mutex_unlock(interrupt);
	}

	return 0;
}

int xpcu_int_wait(struct interrupt *it) {
	struct xpcu_s *xpcu = (struct xpcu_s*)it->hInterrupt;

	if (it->hInterrupt != (unsigned long)xpcu)
		return -ENODEV;
	
	if (xpcu) {
		if (it->dwCounter == 0) {
			it->dwCounter = 1;
		} else {
			pthread_mutex_lock(&xpcu->interrupt);
			pthread_mutex_unlock(&xpcu->interrupt);
		}
	} else {
		pthread_mutex_lock(&dummy_interrupt);
		pthread_mutex_unlock(&dummy_interrupt);
	}

	return 0;
}
