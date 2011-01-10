struct parport_config {
	int num;
	unsigned long ppbase;
	unsigned char real;
	unsigned short usb_vid;
	unsigned short usb_pid;
	unsigned short usb_iface;
	int (*open) (int num);
	void (*close) (int handle);
	int (*transfer) (WD_TRANSFER *tr, int fd, unsigned int request, int ppbase, int ecpbase, int num);
};

struct parport_config __attribute__ ((visibility ("hidden"))) *config_get(int num);
unsigned char __attribute__ ((visibility ("hidden"))) config_is_real_pport(int num);
unsigned short __attribute__ ((visibility ("hidden"))) config_usb_vid(int num);
unsigned short __attribute__ ((visibility ("hidden"))) config_usb_pid(int num);
unsigned short __attribute__ ((visibility ("hidden"))) config_usb_iface(int num);
