int __attribute__ ((visibility ("hidden"))) parport_transfer(WD_TRANSFER *tr, int fd, unsigned int request, int ppbase, int ecpbase, int num);
int __attribute__ ((visibility ("hidden"))) parport_open(int num);
void __attribute__ ((visibility ("hidden"))) parport_close(int handle);
