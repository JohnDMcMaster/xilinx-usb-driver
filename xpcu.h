#define XPCU_CLAIM	1
#define XPCU_RELEASE	0

struct xpcu_s {
	struct usb_device *dev;
	usb_dev_handle *handle;
	struct usb_bus *busses;
	int interface;
	int alternate;
};

int __attribute__ ((visibility ("hidden"))) xpcu_deviceinfo(struct xpcu_s *xpcu, unsigned char *buf);
int __attribute__ ((visibility ("hidden"))) xpcu_claim(struct xpcu_s *xpcu, int claim);
struct xpcu_s __attribute__ ((visibility ("hidden"))) *xpcu_open(void);
