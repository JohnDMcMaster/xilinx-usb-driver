struct parport_config {
	int num;
	unsigned long ppbase;
	unsigned char real;
	unsigned short usb_vid;
	unsigned short usb_pid;
};

unsigned char config_is_real_pport(int num);
unsigned short config_usb_vid(int num);
unsigned short config_usb_pid(int num);
