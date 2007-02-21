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

#define SNIFFLEN 4096

void hexdump(unsigned char *buf, int len);

struct header_struct {
	unsigned long dwHeader;
	void* data;
	unsigned long dwSize;
};

struct version_struct {
	unsigned long versionul;
	char version[128];
};

struct license_struct {
    char cLicense[128]; // Buffer with license string to put.
                      // If empty string then get current license setting
                      // into dwLicense.
    unsigned long dwLicense;  // Returns license settings: LICENSE_DEMO, LICENSE_WD
                      // etc..., or 0 for invalid license.
    unsigned long dwLicense2; // Returns additional license settings, if dwLicense
                      // could not hold all the information.
                      // Then dwLicense will return 0.
};

#define WD_IOCTL_HEADER_CODE 0xa410b413UL

void parse_wdioctlreq(unsigned char *wdioctl, unsigned int request) {
	struct header_struct* wdheader = (struct header_struct*)wdioctl;

	if (wdheader->dwHeader != WD_IOCTL_HEADER_CODE) {
		fprintf(stderr,"!!!ERROR: Header does not match!!!\n");
		return;
	}

	fprintf(stderr, "Request: ");
	switch(request) {
		case 0x910:
			fprintf(stderr,"IOCTL_WD_VERSION");
			//fprintf(stderr," %s(%d)", ((struct version_struct*)(wdheader->data))->version, ((struct version_struct*)(wdheader->data))->versionul);
			break;
		case 0x952:
			fprintf(stderr,"IOCTL_WD_LICENSE");
			break;
		case 0x98c:
			fprintf(stderr,"IOCTL_WD_TRANSFER");
			break;
		case 0x983:
			fprintf(stderr,"IOCTL_WDU_TRANSFER");
			break;
		case 0x987:
			fprintf(stderr,"IOCTL_WD_EVENT_UNREGISTER");
			break;
		case 0x91f:
			fprintf(stderr,"IOCTL_WD_INT_DISABLE");
			break;
		case 0x94b:
			fprintf(stderr,"IOCTL_WD_INT_WAIT");
			break;
		case 0x92b:
			fprintf(stderr,"IOCTL_WD_CARD_UNREGISTER");
			break;
		case 0x9a7:
			fprintf(stderr,"IOCTL_WDU_GET_DEVICE_DATA");
			break;
		case 0x98e:
			fprintf(stderr,"IOCTL_WD_INT_ENABLE");
			break;
		case 0x988:
			fprintf(stderr,"IOCTL_WD_EVENT_PULL");
			break;
		case 0x981:
			fprintf(stderr,"IOCTL_WDU_SET_INTERFACE");
			break;
		default:
			fprintf(stderr,"Unknown(%x)",request);
	}

	fprintf(stderr, ", size: %d\n", wdheader->dwSize);
}

void parse_wdioctlans(unsigned char *wdioctl, unsigned int request, int result) {
	struct header_struct* wdheader = (struct header_struct*)wdioctl;

	if (wdheader->dwHeader != WD_IOCTL_HEADER_CODE) {
		fprintf(stderr,"!!!ERROR: Header does not match!!!\n");
		return;
	}

	fprintf(stderr, "Answer: %d ", result);
	switch(request) {
		case 0x910:
			fprintf(stderr,"\"%s\" (%d)", ((struct version_struct*)(wdheader->data))->version, ((struct version_struct*)(wdheader->data))->versionul);
			break;
		case 0x952:
			fprintf(stderr,"\"%s\" (XX,XX)", ((struct license_struct*)(wdheader->data))->cLicense);
			break;
		default:
			break;
	}
	fprintf(stderr, "\n");
}


typedef int (*open_funcptr_t) (const char *, int, mode_t);

static windrvrfd = 0;
static void* mmapped = NULL;
static size_t mmapplen = 0;

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

	fd = (*func) (pathname, flags, mode);

	if (!strcmp (pathname, "/dev/windrvr6")) {
		fprintf(stderr,"opening windrvr6\n");
		windrvrfd = fd;
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
	static int (*func) (int, int, void *) = NULL;
	va_list args;
	void *argp;
	unsigned char prebuf[SNIFFLEN];
	int ret;

	if (!func)                                                                    
		func = (int (*) (int, int, void *)) dlsym (REAL_LIBC, "ioctl");             

	va_start (args, request);
	argp = va_arg (args, void *);
	va_end (args);

	if (fd == windrvrfd) {
		memcpy(prebuf, argp, SNIFFLEN);
		parse_wdioctlreq(argp, request);
	}

	ret = (*func) (fd, request, argp);

	if (fd == windrvrfd) {
		parse_wdioctlans(argp, request, ret);
	}
	return ret;
}

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
