#define JTAGKEY_TCK	0x01
#define JTAGKEY_TDI	0x02
#define JTAGKEY_TDO	0x04
#define JTAGKEY_TMS	0x08
#define JTAGKEY_VREF	0x20
#define JTAGKEY_OEn	0x10

int jtagkey_init(unsigned short vid, unsigned short pid);
void jtagkey_close();
int jtagkey_transfer(WD_TRANSFER *tr, int fd, unsigned int request, int ppbase, int ecpbase, int num);
