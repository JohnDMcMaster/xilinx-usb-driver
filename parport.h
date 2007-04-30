int parport_transfer(WD_TRANSFER *tr, int fd, unsigned int request, int ppbase, int ecpbase, int num);
int parport_open(int num);
void parport_close(int handle);
